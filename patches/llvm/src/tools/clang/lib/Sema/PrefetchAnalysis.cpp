//=- PrefetchAnalysis.cpp - Prefetching Analysis for Structured Blocks ---*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements prefetching analysis for structured blocks.  The
// analysis traverses the AST to determine how arrays are accessed in structured
// blocks and generates expressions defining ranges of elements accessed.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/PrefetchAnalysis.h"
#include "clang/Sema/PrefetchDataflow.h"
#include "clang/Sema/PrefetchExprBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Debug.h"

using namespace clang;

typedef PrefetchAnalysis::InductionVariablePtr InductionVariablePtr;
typedef PrefetchAnalysis::IVMap IVMap;
typedef PrefetchAnalysis::IVPair IVPair;

//===----------------------------------------------------------------------===//
// Common utilities
//

/// Return whether a type is both scalar and integer.
bool PrefetchAnalysis::isScalarIntType(const QualType &Ty) {
  return Ty->isIntegerType() && Ty->isScalarType();
}

/// Get the size in bits for builtin integer types.
unsigned PrefetchAnalysis::getTypeSize(BuiltinType::Kind K) {
  switch(K) {
  case BuiltinType::Bool:
  case BuiltinType::Char_U: case BuiltinType::UChar:
  case BuiltinType::Char_S: case BuiltinType::SChar:
    return 8;
  case BuiltinType::WChar_U: case BuiltinType::Char16:
  case BuiltinType::UShort:
  case BuiltinType::WChar_S:
  case BuiltinType::Short:
    return 16;
  case BuiltinType::Char32:
  case BuiltinType::UInt:
  case BuiltinType::Int:
    return 32;
  case BuiltinType::ULong:
  case BuiltinType::ULongLong:
  case BuiltinType::Long:
  case BuiltinType::LongLong:
    return 64;
  case BuiltinType::UInt128:
  case BuiltinType::Int128:
    return 128;
  default: return UINT32_MAX;
  }
}

/// Return the variable declaration if the declared value is a variable and if
/// it is a scalar integer type or nullptr otherwise.
VarDecl *PrefetchAnalysis::getVarIfScalarInt(ValueDecl *VD) {
  if(isa<VarDecl>(VD)) {
    VarDecl *Var = cast<VarDecl>(VD);
    if(isScalarIntType(Var->getType())) return Var;
  }
  return nullptr;
}

// Filter functions to only select appropriate operator types.  Return true if
// the operator is of a type that should be analyzed, or false otherwise.
typedef bool (*UnaryOpFilter)(UnaryOperator::Opcode);
typedef bool (*BinaryOpFilter)(BinaryOperator::Opcode);

// Don't analyze *any* operation types.
bool NoUnaryOp(UnaryOperator::Opcode Op) { return false; }
bool NoBinaryOp(BinaryOperator::Opcode Op) { return false; }

// Filter out non-assignment binary operations.
static bool FilterAssignOp(BinaryOperator::Opcode Op) {
  switch(Op) {
  case BO_Assign: case BO_MulAssign: case BO_DivAssign: case BO_RemAssign:
  case BO_AddAssign: case BO_SubAssign: case BO_ShlAssign: case BO_ShrAssign:
  case BO_AndAssign: case BO_XorAssign: case BO_OrAssign:
    return true;
  default: return false;
  }
}

// Filter out non-relational binary operations.
static bool FilterRelationalOp(BinaryOperator::Opcode Op) {
  switch(Op) {
  case BO_LT: case BO_GT: case BO_LE: case BO_GE: case BO_EQ: case BO_NE:
    return true;
  default: return false;
  }
}

// Filter out non-math/logic binary operations.
static bool FilterMathLogicOp(BinaryOperator::Opcode Op) {
  switch(Op) {
  case BO_Mul: case BO_Div: case BO_Rem: case BO_Add: case BO_Sub:
  case BO_Shl: case BO_Shr: case BO_And: case BO_Xor: case BO_Or:
    return true;
  default: return false;
  }
}

// Filter out non-math unary operations.
static bool FilterMathOp(UnaryOperator::Opcode Op) {
  switch(Op) {
  case UO_PostInc: case UO_PostDec: case UO_PreInc: case UO_PreDec:
    return true;
  default: return false;
  }
}

/// Return the statement if it is a scoping statement (e.g., for-loop) or
/// nullptr otherwise.
static bool isScopingStmt(Stmt *S) {
  if(isa<CapturedStmt>(S) || isa<CompoundStmt>(S) || isa<CXXCatchStmt>(S) ||
     isa<CXXForRangeStmt>(S) || isa<CXXTryStmt>(S) || isa<DoStmt>(S) ||
     isa<ForStmt>(S) || isa<IfStmt>(S) || isa<OMPExecutableDirective>(S) ||
     isa<SwitchStmt>(S) || isa<WhileStmt>(S)) return true;
  else return false;
}

/// A vector of variable declarations.
typedef llvm::SmallVector<VarDecl *, 4> VarVec;

//===----------------------------------------------------------------------===//
// Prefetch analysis -- array accesses
//

/// Scoping information for array analyses.  A node in a singly-linked list
/// which allows traversal from innermost scope outwards.  Nodes are reference
/// counted, so when array accesses which reference the scope (if any) are
/// deleted, the scoping chain itself gets deleted.
struct ScopeInfo {
  Stmt *ScopeStmt;                        // Statement providing scope
  std::shared_ptr<ScopeInfo> ParentScope; // The parent in the scope chain
  ScopeInfo(Stmt *ScopeStmt, std::shared_ptr<ScopeInfo> &ParentScope)
    : ScopeStmt(ScopeStmt), ParentScope(ParentScope) {}
};
typedef std::shared_ptr<ScopeInfo> ScopeInfoPtr;

/// An array access.
class ArrayAccess {
public:
  ArrayAccess(VarDecl *Base, Expr *Idx, const ScopeInfoPtr &AccessScope)
    : Valid(true), Base(Base), Idx(Idx), AccessScope(AccessScope) {}

  bool isValid() const { return Valid; }
  VarDecl *getBase() const { return Base; }
  Expr *getIndex() const { return Idx; }
  const VarVec &getVarsInIdx() const { return VarsInIdx; }
  const ScopeInfoPtr &getScope() const { return AccessScope; }

  void setInvalid() { Valid = false; }
  void addVarInIdx(VarDecl *V) { if(V != Base) VarsInIdx.push_back(V); }

  void print(llvm::raw_ostream &O, PrintingPolicy &Policy) const {
    O << "Array: " << Base->getName() << "\nIndex expression: ";
    Idx->printPretty(O, nullptr, Policy);
    O << "\nScoping statement:\n";
    AccessScope->ScopeStmt->printPretty(O, nullptr, Policy);
    O << "\nVariables used in index calculation:";
    for(auto Var : VarsInIdx) O << " " << Var->getName();
    O << "\n";
  }
  void dump(PrintingPolicy &Policy) const { print(llvm::dbgs(), Policy); }

private:
  bool Valid;               // Is the access valid?
  VarDecl *Base;            // The array base
  Expr *Idx;                // Expression used to calculate index
  VarVec VarsInIdx;         // Variables used in index calculation
  ScopeInfoPtr AccessScope; // Scope of the array access
};

/// Traverse a statement looking for array accesses.
// TODO *** NEED TO LIMIT TO AFFINE ACCESSES ***
class ArrayAccessPattern : public RecursiveASTVisitor<ArrayAccessPattern> {
public:
  ArrayAccessPattern(llvm::SmallPtrSet<VarDecl *, 4> &Ignore)
    : Ignore(Ignore) {}

  /// Which sub-tree of a binary operator we're traversing.  This determines
  /// whether we're reading or writing the array.
  enum TraverseStructure { LHS, RHS };

  void InitTraversal() {
    Side.push_back(RHS);
    CurAccess = nullptr;
  }

  /// Traverse a statement.  There's a couple of special traversal rules:
  ///
  ///  - If it's a scoping statement, add an enclosing scope to the scope chain
  //     before traversing the sub-tree
  ///  - If it's an assignment operation, record structure of the traversal
  ///    before visiting each of the left & right sub-trees
  ///  - If it's an array subscript, record all variables used to calculate
  ///    the index
  bool TraverseStmt(Stmt *S) {
    if(!S) return true;

    bool isScope = isScopingStmt(S);
    BinaryOperator *BinOp = dyn_cast<BinaryOperator>(S);
    ArraySubscriptExpr *Subscript = dyn_cast<ArraySubscriptExpr>(S);

    if(isScope) CurScope = ScopeInfoPtr(new ScopeInfo(S, CurScope));

    if(BinOp && FilterAssignOp(BinOp->getOpcode())) {
      // For assignment operations, LHS = write and RHS = read
      Side.push_back(LHS);
      TraverseStmt(BinOp->getLHS());
      Side.pop_back();
      Side.push_back(RHS);
      TraverseStmt(BinOp->getRHS());
      Side.pop_back();
    }
    else if(Subscript) {
      // TODO doesn't work for nested accesses, e.g., a[b[i]]
      RecursiveASTVisitor<ArrayAccessPattern>::TraverseStmt(S);
      CurAccess = nullptr; // Don't record any more variables
    }
    else RecursiveASTVisitor<ArrayAccessPattern>::TraverseStmt(S);

    if(isScope) CurScope = CurScope->ParentScope;

    return true;
  }

  /// Analyze an array access; in particular, the index.
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *Sub) {
    Expr *Base = Sub->getBase(), *Idx = Sub->getIdx();
    DeclRefExpr *DR;
    VarDecl *VD;

    DR = dyn_cast<DeclRefExpr>(Base->IgnoreImpCasts());
    if(!DR) return true;
    VD = dyn_cast<VarDecl>(DR->getDecl());
    if(!VD || Ignore.count(VD)) return true;
    if(Side.back() == LHS) {
      ArrayWrites.emplace_back(VD, Idx, CurScope);
      CurAccess = &ArrayWrites.back();
    }
    else {
      ArrayReads.emplace_back(VD, Idx, CurScope);
      CurAccess = &ArrayReads.back();
    }
    return true;
  }

  /// Record any variables seen during traversing
  bool VisitDeclRefExpr(DeclRefExpr *DR) {
    if(CurAccess) {
      VarDecl *VD = dyn_cast<VarDecl>(DR->getDecl());
      if(VD) CurAccess->addVarInIdx(VD);
      else CurAccess->setInvalid(); // Can't analyze if decl != variable
    }
    return true;
  }

  const llvm::SmallVector<ArrayAccess, 8> &getArrayReads() const
  { return ArrayReads; }
  const llvm::SmallVector<ArrayAccess, 8> &getArrayWrites() const
  { return ArrayWrites; }

private:
  llvm::SmallVector<ArrayAccess, 8> ArrayReads, ArrayWrites;
  ScopeInfoPtr CurScope;
  llvm::SmallPtrSet<VarDecl *, 4> &Ignore;

  // Traversal state
  llvm::SmallVector<enum TraverseStructure, 8> Side;
  ArrayAccess *CurAccess;
};

void PrefetchAnalysis::mergeArrayAccesses() {
  // TODO!
}

void PrefetchAnalysis::pruneEmptyArrayAccesses() {
  // TODO!
}

//===----------------------------------------------------------------------===//
// Prefetch analysis -- ForStmts
//

namespace clang {

/// An induction variable and expressions describing its range.
class InductionVariable {
public:
  /// The direction of change for the induction variable
  enum Direction {
    Increases, // Update changes variable from lower to higher values
    Decreases, // Update changes variable from higher to lower values
    Unknown // Update has an unknown effect, e.g., container interators
  };

  InductionVariable() : Var(nullptr), Init(nullptr), Cond(nullptr),
                        Update(nullptr), Dir(Unknown), LowerB(nullptr),
                        UpperB(nullptr) {}

  InductionVariable(VarDecl *Var, Expr *Init, Expr *Cond, Expr *Update,
                    ASTContext &Ctx)
    : Var(Var), Init(Init), Cond(Cond), Update(Update), Dir(Unknown),
      LowerB(nullptr), UpperB(nullptr) {

    PrefetchExprBuilder::Modifier UpperMod, LowerMod;
    const UnaryOperator *Unary;

    assert(PrefetchAnalysis::isScalarIntType(Var->getType()) &&
           "Invalid induction variable");

    // Try to classify update direction to determine which expression specifies
    // lower and upper bounds
    if((Unary = dyn_cast<UnaryOperator>(Update)))
      Dir = classifyUnaryOpDirection(Unary->getOpcode());

    if(Dir == Increases) {
      LowerMod.ClassifyModifier(Init, Ctx);
      UpperMod.ClassifyModifier(Cond, Ctx);
      LowerB = stripInductionVar(Init);
      UpperB = stripInductionVar(Cond);
    }
    else if(Dir == Decreases) {
      LowerMod.ClassifyModifier(Cond, Ctx);
      UpperMod.ClassifyModifier(Init, Ctx);
      LowerB = stripInductionVar(Cond);
      UpperB = stripInductionVar(Init);
    }

    if(LowerB && UpperB) {
      LowerB = PrefetchExprBuilder::cloneAndModifyExpr(LowerB, LowerMod, &Ctx);
      UpperB = PrefetchExprBuilder::cloneAndModifyExpr(UpperB, UpperMod, &Ctx);
    }
  }

  VarDecl *getVariable() const { return Var; }
  Expr *getInit() const { return Init; }
  Expr *getCond() const { return Cond; }
  Expr *getUpdate() const { return Update; }
  enum Direction getUpdateDirection() const { return Dir; }
  Expr *getLowerBound() const { return LowerB; }
  Expr *getUpperBound() const { return UpperB; }

  void print(llvm::raw_ostream &O, PrintingPolicy &Policy) const {
    O << "Induction Variable: " << Var->getName() << "\nDirection: ";
    switch(Dir) {
    case Increases: O << "increases\n"; break;
    case Decreases: O << "decreases\n"; break;
    case Unknown: O << "unknown update direction\n"; break;
    }
    if(LowerB && UpperB) {
      O << "Lower bound: ";
      LowerB->printPretty(O, nullptr, Policy);
      O << "\nUpper bound: ";
      UpperB->printPretty(O, nullptr, Policy);
    }
    else O << "-> Could not determine bounds <-";
    O << "\n";
  }

  void dump(PrintingPolicy &Policy) const { print(llvm::dbgs(), Policy); }

private:
  VarDecl *Var;
  Expr *Init, *Cond, *Update;

  /// Expressions describing the lower & upper bounds of the induction variable
  /// and its update direction.
  enum Direction Dir;
  Expr *LowerB, *UpperB;

  /// Try to classify the induction variable's update direction based on the
  /// unary operation type.
  static enum Direction classifyUnaryOpDirection(UnaryOperator::Opcode Op) {
    switch(Op) {
    case UO_PostInc:
    case UO_PreInc:
      return Increases;
    case UO_PostDec:
    case UO_PreDec:
      return Decreases;
    default: return Unknown;
    }
  }

  Expr *stripInductionVarFromBinOp(BinaryOperator *B) {
    DeclRefExpr *D;
    VarDecl *VD;

    D = dyn_cast<DeclRefExpr>(B->getLHS()->IgnoreImpCasts());
    if(!D) return nullptr;
    VD = dyn_cast<VarDecl>(D->getDecl());
    if(!VD) return nullptr;
    if(VD == Var) return B->getRHS();
    return nullptr;
  }

  Expr *stripInductionVarFromExpr(Expr *E) {
    DeclRefExpr *D;
    VarDecl *VD;

    D = dyn_cast<DeclRefExpr>(E->IgnoreImpCasts());
    if(!D) return nullptr;
    VD = dyn_cast<VarDecl>(D->getDecl());
    if(!VD) return nullptr;
    if(VD != Var) return D;
    return nullptr;
  }

  /// Remove the induction variable & operator from the expression, leaving
  /// only a bounds expression.
  Expr *stripInductionVar(Expr *E) {
    BinaryOperator *B;
    IntegerLiteral *L;

    if((B = dyn_cast<BinaryOperator>(E))) return stripInductionVarFromBinOp(B);
    else if((L = dyn_cast<IntegerLiteral>(E))) return L;
    else if(E) return stripInductionVarFromExpr(E);
    else return nullptr;
  }
};

} /* end namespace clang */

/// Map an induction variable to an expression describing a bound.
typedef llvm::DenseMap<VarDecl *, Expr *> IVBoundMap;
typedef std::pair<VarDecl *, Expr *> IVBoundPair;

/// Traversal to find induction variables in loop initialization, condition and
/// update expressions.
template<UnaryOpFilter UnaryFilt, BinaryOpFilter BinaryFilt>
class IVFinder : public RecursiveASTVisitor<IVFinder<UnaryFilt, BinaryFilt>> {
public:
  // Visit binary operators to find induction variables.
  bool VisitBinaryOperator(BinaryOperator *B) {
    Expr *LHS;
    DeclRefExpr *DR;
    VarDecl *Var;

    // Filter out irrelevant operation types
    if(!BinaryFilt(B->getOpcode())) return true;

    // Look for DeclRefExprs of scalar integer type -- these reference
    // induction variables
    LHS = B->getLHS();
    if(!PrefetchAnalysis::isScalarIntType(LHS->getType())) return true;
    DR = dyn_cast<DeclRefExpr>(LHS->IgnoreImpCasts());
    if(!DR) return true;

    // Make sure the expression acting on the induction variable is a scalar
    // integer (casts may change types)
    Var = PrefetchAnalysis::getVarIfScalarInt(DR->getDecl());
    if(!Var) return true;
    InductionVars[Var] = B;
    return true;
  }

  // Visit unary operators to find induction variables.
  bool VisitUnaryOperator(UnaryOperator *U) {
    Expr *SubExpr;
    DeclRefExpr *DR;
    VarDecl *Var;

    // Filter out irrelevant operation types
    if(!UnaryFilt(U->getOpcode())) return true;

    // Look for DeclRefExprs of scalar integer type -- these reference
    // induction variables
    SubExpr = U->getSubExpr();
    if(!PrefetchAnalysis::isScalarIntType(SubExpr->getType())) return true;
    DR = dyn_cast<DeclRefExpr>(SubExpr->IgnoreImpCasts());
    if(!DR) return true;

    // Make sure the expression acting on the induction variable is a scalar
    // integer (casts may change types)
    Var = PrefetchAnalysis::getVarIfScalarInt(DR->getDecl());
    if(!Var) return true;
    InductionVars[Var] = U;
    return true;
  }

  bool VisitDeclStmt(DeclStmt *D) {
    VarDecl *Var;
    for(auto &Child : D->getDeclGroup()) {
      Var = PrefetchAnalysis::getVarIfScalarInt(dyn_cast<VarDecl>(Child));
      if(!Var || !Var->hasInit()) continue;
      InductionVars[Var] = Var->getInit();
    }
    return true;
  }

  /// Return all induction variables found.
  const IVBoundMap &getInductionVars() const { return InductionVars; }

  /// Return the bounds expression for a given induction variable, or nullptr
  /// if none was found.
  Expr *getVarBound(VarDecl *Var) {
    IVBoundMap::iterator it = InductionVars.find(Var);
    if(it != InductionVars.end()) return it->second;
    else return nullptr;
  }

private:
  IVBoundMap InductionVars;
};

/// Structural information about a for-loop, including induction variables and
/// parent/child loops.
class ForLoopInfo {
public:
  ForLoopInfo(ForStmt *Loop, std::shared_ptr<ForLoopInfo> &Parent, int Level)
    : Loop(Loop), Parent(Parent), Level(Level) {}

  /// Add an induction variable.
  void addInductionVar(const InductionVariablePtr &IV)
  { InductionVars.insert(IVPair(IV->getVariable(), IV)); }

  /// Remove an induction variable if present.  Return true if removed or false
  /// if we don't have the variable.
  bool removeInductionVar(const InductionVariablePtr &IV) {
    IVMap::iterator it = InductionVars.find(IV->getVariable());
    if(it != InductionVars.end()) {
      InductionVars.erase(it);
      return true;
    }
    else return false;
  }

  /// Add a child loop.
  void addChildLoop(std::shared_ptr<ForLoopInfo> &S) { Children.push_back(S); }

  ForStmt *getLoop() const { return Loop; }
  const std::shared_ptr<ForLoopInfo> &getParent() const { return Parent; }
  int getLevel() const { return Level; }
  const IVMap &getInductionVars() const { return InductionVars; }
  const llvm::SmallVector<std::shared_ptr<ForLoopInfo>, 4> &getChildren() const
  { return Children; }

  void print(llvm::raw_ostream &O, PrintingPolicy &Policy) const {
    O << "Loop @ " << this << "\nDepth: " << Level
      << "\nParent: " << Parent.get();
    if(Children.size()) {
      O << "\nChildren:";
      for(auto &Child : Children) O << " " << Child.get();
    }
    O << "\n";
    Loop->printPretty(O, nullptr, Policy);
    O << "\n";
  }
  void dump(PrintingPolicy &Policy) const { print(llvm::dbgs(), Policy); }

private:
  ForStmt *Loop;
  std::shared_ptr<ForLoopInfo> Parent;
  size_t Level;

  IVMap InductionVars;
  llvm::SmallVector<std::shared_ptr<ForLoopInfo>, 4> Children;
};
typedef std::shared_ptr<ForLoopInfo> ForLoopInfoPtr;

/// Search a sub-tree for loops, calculating induction variables found in any
/// loops along the way.  We *must* construct tree structural information in
/// order to correctly handle complex loop nests, e.g.:
///
/// int a, b;
/// for(a = ...; a < ...; a++) {
///   for(b = 0; b < 10; b++) {
///     ...
///   }
///
///   for(b = 10; b < 20; b++) {
///
///   }
/// }
///
/// In this example, induction variable 'b' has different ranges in each of the
/// nested loops.
class LoopNestTraversal : public RecursiveASTVisitor<LoopNestTraversal> {
public:
  LoopNestTraversal(ASTContext &Ctx) : Ctx(Ctx) {}

  void InitTraversal() { if(!LoopNest.size()) LoopNest.emplace_back(nullptr); }

  bool VisitForStmt(ForStmt *S) {
    Expr *InitExpr, *CondExpr, *UpdateExpr;
    IVFinder<NoUnaryOp, FilterAssignOp> Init;
    IVFinder<NoUnaryOp, FilterRelationalOp> Cond;
    IVFinder<FilterMathOp, FilterMathLogicOp> Update;

    // Set up data & tree structure information.
    LoopNest.emplace_back(
      new ForLoopInfo(S, LoopNest.back(), LoopNest.size() - 1));
    ForLoopInfoPtr &Cur = LoopNest.back();
    Loops[S] = Cur;
    if(Cur->getParent()) Cur->getParent()->addChildLoop(Cur);

    // Find the induction variables in the loop expressions.
    Init.TraverseStmt(S->getInit());
    Cond.TraverseStmt(S->getCond());
    Update.TraverseStmt(S->getInc());

    // Find induction variables which are referenced in all three parts of the
    // for-loop header.
    const IVBoundMap &InitVars = Init.getInductionVars();
    for(auto Var = InitVars.begin(), E = InitVars.end(); Var != E; Var++) {
      InitExpr = Var->second;
      CondExpr = Cond.getVarBound(Var->first),
      UpdateExpr = Update.getVarBound(Var->first);
      if(InitExpr && CondExpr && UpdateExpr) {
        InductionVariablePtr IV(
          new InductionVariable(Var->first, InitExpr, CondExpr,
                                UpdateExpr, Ctx));
        Cur->addInductionVar(std::move(IV));
      }
    }

    return true;
  }

  bool TraverseStmt(Stmt *S) {
    if(!S) return true;
    RecursiveASTVisitor<LoopNestTraversal>::TraverseStmt(S);
    if(isa<ForStmt>(S)) LoopNest.pop_back();
    return true;
  }

  /// Prune induction variables so each each loop only maintains its own
  /// induction variables and not those of any nested loops.
  // TODO this may not be necessary...
  void PruneInductionVars() {
    // Each loop nest is a tree in a forest of all loop nests
    for(auto &Info : Loops)
      if(Info.second->getLevel() == 0)
        PruneInductionVars(Info.second);
  }

  /// Get all loops discovered during the tree traversal.
  const llvm::DenseMap<ForStmt *, ForLoopInfoPtr> &getLoops() const
  { return Loops; }

  /// Get the enclosing loop's information for an array access.
  const ForLoopInfoPtr getEnclosingLoop(const ArrayAccess &A) const {
    ScopeInfoPtr S = A.getScope();
    while(S && !isa<ForStmt>(S->ScopeStmt)) S = S->ParentScope;
    if(!S) return ForLoopInfoPtr(nullptr);

    llvm::DenseMap<ForStmt *, ForLoopInfoPtr>::const_iterator it
      = Loops.find(cast<ForStmt>(S->ScopeStmt));
    if(it != Loops.end()) return it->second;
    else return ForLoopInfoPtr(nullptr);
  }

private:
  ASTContext &Ctx;

  // A stack of nested loops to provide induction variable scoping information.
  llvm::SmallVector<ForLoopInfoPtr, 4> LoopNest;

  // Map loop statements to information gathered during traversal.
  llvm::DenseMap<ForStmt *, ForLoopInfoPtr> Loops;

  // Recursively prune induction variables in a bottom-up fashion (post-order
  // traversal).
  void PruneInductionVars(ForLoopInfoPtr Loop) {
    for(auto &Child : Loop->getChildren()) {
      PruneInductionVars(Child);
      for(auto &IV : Child->getInductionVars())
        Loop->removeInductionVar(IV.second);
    }
  }
};

/// Search the loop scoping chain for an induction variable.  Return the
/// induction variable information if found, or nullptr otherwise.
static const InductionVariablePtr
findInductionVariable(VarDecl *V, const ForLoopInfoPtr &Scope) {
  assert(V && Scope && "Invalid arguments");

  ForLoopInfoPtr TmpScope = Scope;
  InductionVariablePtr IV(nullptr);
  do {
    const IVMap &IVs = TmpScope->getInductionVars();
    IVMap::const_iterator it = IVs.find(V);
    if(it != IVs.end()) {
      IV = it->second;
      break;
    }
    else TmpScope = TmpScope->getParent();
  } while(TmpScope);
  return IV;
}

/// Search a for-loop statement for array access patterns based on loop
/// induction variables that can be prefetched at runtime.
void PrefetchAnalysis::analyzeForStmt() {
  LoopNestTraversal Loops(*Ctx);
  ArrayAccessPattern ArrAccesses(Ignore);
  PrefetchDataflow Dataflow(Ctx);
  PrefetchDataflow::VarSet VarsToTrack;
  ReplaceMap LowerBounds, UpperBounds;
  Expr *UpperBound, *LowerBound;
  PrefetchExprBuilder::BuildInfo LowerBuild = { Ctx, LowerBounds, true },
                                 UpperBuild = { Ctx, UpperBounds, true };

  // Gather loop nest information, including induction variables
  Loops.InitTraversal();
  Loops.TraverseStmt(S);
  Loops.PruneInductionVars();

  // Find array/pointer accesses.
  ArrAccesses.InitTraversal();
  ArrAccesses.TraverseStmt(S);

  // Run the dataflow analysis to see if induction variables are used in any
  // expressions which initialize other variables.
  for(auto &Read : ArrAccesses.getArrayReads()) {
    ForLoopInfoPtr Scope = Loops.getEnclosingLoop(Read);
    for(auto &Var : Read.getVarsInIdx()) {
      if(!findInductionVariable(Var, Scope))
        VarsToTrack.insert(Var);
    }
  }

  for(auto &Write: ArrAccesses.getArrayWrites()) {
    ForLoopInfoPtr Scope = Loops.getEnclosingLoop(Write);
    for(auto &Var : Write.getVarsInIdx()) {
      if(!findInductionVariable(Var, Scope))
        VarsToTrack.insert(Var);
    }
  }

  Dataflow.reset();
  Dataflow.runDataflow(cast<ForStmt>(S)->getBody(), VarsToTrack);

  // TODO delete
  Dataflow.dump();

  // Reconstruct array subscript expressions with induction variable references
  // replaced by their bounds.
  for(auto &Read : ArrAccesses.getArrayReads()) {
    LowerBounds.clear();
    UpperBounds.clear();
    ForLoopInfoPtr Scope = Loops.getEnclosingLoop(Read);

    // Get upper & lower bounds of induction variable for replacement
    for(auto &Var : Read.getVarsInIdx()) {
      const InductionVariablePtr IV = findInductionVariable(Var, Scope);
      if(IV) {
        LowerBounds.insert(ReplacePair(IV->getVariable(), IV->getLowerBound()));
        UpperBounds.insert(ReplacePair(IV->getVariable(), IV->getUpperBound()));
      }
    }

    // Create array access bounds expressions
    LowerBound =
      PrefetchExprBuilder::cloneWithReplacement(Read.getIndex(), LowerBuild),
    UpperBound =
      PrefetchExprBuilder::cloneWithReplacement(Read.getIndex(), UpperBuild);
    if(LowerBound && UpperBound)
      ToPrefetch.emplace_back(PrefetchRange::Read, Read.getBase(),
                              LowerBound, UpperBound);
  }

  for(auto &Write : ArrAccesses.getArrayWrites()) {
    LowerBounds.clear();
    UpperBounds.clear();
    ForLoopInfoPtr Scope = Loops.getEnclosingLoop(Write);

    for(auto &Var : Write.getVarsInIdx()) {
      const InductionVariablePtr IV = findInductionVariable(Var, Scope);
      if(IV) {
        LowerBounds.insert(ReplacePair(IV->getVariable(), IV->getLowerBound()));
        UpperBounds.insert(ReplacePair(IV->getVariable(), IV->getUpperBound()));
      }
    }

    LowerBound =
      PrefetchExprBuilder::cloneWithReplacement(Write.getIndex(), LowerBuild),
    UpperBound =
      PrefetchExprBuilder::cloneWithReplacement(Write.getIndex(), UpperBuild);
    if(LowerBound && UpperBound)
      ToPrefetch.emplace_back(PrefetchRange::Write, Write.getBase(),
                              LowerBound, UpperBound);
  }
}


//===----------------------------------------------------------------------===//
// Prefetch analysis API
//

void PrefetchAnalysis::analyzeStmt() {
  if(!Ctx || !S) return;

  // TODO other types of statements
  if(isa<ForStmt>(S)) analyzeForStmt();

  mergeArrayAccesses();
  pruneEmptyArrayAccesses();
}

void PrefetchAnalysis::print(llvm::raw_ostream &O) const {
  PrintingPolicy Policy(Ctx->getLangOpts());
  for(auto &Range : ToPrefetch) {
    O << "Array '" << Range.getArray()->getName() << "': ";
    Range.getStart()->printPretty(O, nullptr, Policy);
    O << " to ";
    Range.getEnd()->printPretty(O, nullptr, Policy);
    O << " (" << Range.getTypeName() << ")\n";
  }
}

