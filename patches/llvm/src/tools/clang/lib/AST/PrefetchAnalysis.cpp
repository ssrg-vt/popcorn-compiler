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
#include "clang/AST/PrefetchAnalysis.h"
#include "clang/AST/RecursiveASTVisitor.h"
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
static bool isScalarIntType(const QualType &Ty) {
  return Ty->isIntegerType() && Ty->isScalarType();
}

/// Return the variable declaration if the declared value is a variable and if
/// it is a scalar integer type or nullptr otherwise.
static VarDecl *getVarIfScalarInt(ValueDecl *VD) {
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
bool FilterAssignOp(BinaryOperator::Opcode Op) {
  switch(Op) {
  case BO_Assign: case BO_MulAssign: case BO_DivAssign: case BO_RemAssign:
  case BO_AddAssign: case BO_SubAssign: case BO_ShlAssign: case BO_ShrAssign:
  case BO_AndAssign: case BO_XorAssign: case BO_OrAssign:
    return true;
  default: return false;
  }
}

// Filter out non-relational binary operations.
bool FilterRelationalOp(BinaryOperator::Opcode Op) {
  switch(Op) {
  case BO_LT: case BO_GT: case BO_LE: case BO_GE: case BO_EQ: case BO_NE:
    return true;
  default: return false;
  }
}

// Filter out non-math/logic binary operations.
bool FilterMathLogicOp(BinaryOperator::Opcode Op) {
  switch(Op) {
  case BO_Mul: case BO_Div: case BO_Rem: case BO_Add: case BO_Sub:
  case BO_Shl: case BO_Shr: case BO_And: case BO_Xor: case BO_Or:
    return true;
  default: return false;
  }
}

// Filter out non-math unary operations.
bool FilterMathOp(UnaryOperator::Opcode Op) {
  switch(Op) {
  case UO_PostInc: case UO_PostDec: case UO_PreInc: case UO_PreDec:
    return true;
  default: return false;
  }
}

/// A vector of variable declarations.
typedef llvm::SmallVector<VarDecl *, 4> VarVec;

/// Return the statement if it is a scoping statement (e.g., for-loop) or
/// nullptr otherwise.
static bool isScopingStmt(Stmt *S) {
  if(isa<CapturedStmt>(S) || isa<CompoundStmt>(S) || isa<CXXCatchStmt>(S) ||
     isa<CXXForRangeStmt>(S) || isa<CXXTryStmt>(S) || isa<DoStmt>(S) ||
     isa<ForStmt>(S) || isa<IfStmt>(S) || isa<OMPExecutableDirective>(S) ||
     isa<SwitchStmt>(S) || isa<WhileStmt>(S)) return true;
  else return false;
}

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

//===----------------------------------------------------------------------===//
// Prefetch analysis -- array accesses
//

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

    assert(Side.size() && "Unhandled tree structure");

    DR = dyn_cast<DeclRefExpr>(Base->IgnoreImpCasts());
    if(!DR) goto end;
    VD = dyn_cast<VarDecl>(DR->getDecl());
    if(!VD) goto end;
    if(Side.back() == LHS) {
      ArrayWrites.emplace_back(VD, Idx, CurScope);
      CurAccess = &ArrayWrites.back();
    }
    else {
      ArrayReads.emplace_back(VD, Idx, CurScope);
      CurAccess = &ArrayReads.back();
    }
end:
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

  // Traversal state
  llvm::SmallVector<enum TraverseStructure, 8> Side;
  ArrayAccess *CurAccess;
};

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
                        Update(nullptr), LowerB(nullptr), UpperB(nullptr),
                        Dir(Unknown) {}

  InductionVariable(VarDecl *Var, Expr *Init, Expr *Cond, Expr *Update)
    : Var(Var), Init(Init), Cond(Cond), Update(Update), LowerB(nullptr),
      UpperB(nullptr), Dir(Unknown) {

    const UnaryOperator *Unary;
    assert(getVarIfScalarInt(Var) && "Invalid induction variable");

    // Try to classify update direction to determine upper/lower bounds
    if((Unary = dyn_cast<UnaryOperator>(Update))) {
      classifyUnaryOpDirection(Unary->getOpcode());
      if(Dir != Unknown) goto end;
    }

end:
    // TODO if update is a math/assign operator, e.g., +=, need to update
    // bounds expression to *unwind*
    if(Dir == Increases) {
      LowerB = stripInductionVar(Init);
      UpperB = stripInductionVar(Cond);
    }
    else if(Dir == Decreases) {
      LowerB = stripInductionVar(Cond);
      UpperB = stripInductionVar(Init);
    }
  }

  VarDecl *getVariable() const { return Var; }
  Expr *getInit() const { return Init; }
  Expr *getCond() const { return Cond; }
  Expr *getUpdate() const { return Update; }
  Expr *getLowerBound() const { return LowerB; }
  Expr *getUpperBound() const { return UpperB; }
  enum Direction getUpdateDirection() const { return Dir; }

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
  Expr *LowerB, *UpperB;
  enum Direction Dir;

  /// Try to classify the induction variable's update direction based on the
  /// unary operation type.
  void classifyUnaryOpDirection(UnaryOperator::Opcode Op) {
    switch(Op) {
    case UO_PostInc:
    case UO_PreInc:
      Dir = Increases;
      break;
    case UO_PostDec:
    case UO_PreDec:
      Dir = Decreases;
      break;
    default: break;
    }
  }

  Expr *stripInductionVar(Expr *E) {
    Expr *Stripped = nullptr;
    BinaryOperator *B;
    DeclRefExpr *D;
    VarDecl *VD;

    B = dyn_cast<BinaryOperator>(E);
    if(!B) goto end;
    D = dyn_cast<DeclRefExpr>(B->getLHS()->IgnoreImpCasts());
    if(!D) goto end;
    VD = dyn_cast<VarDecl>(D->getDecl());
    if(!VD) goto end;
    if(VD == Var) return B->getRHS();

end:
    return Stripped;
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
    if(!BinaryFilt(B->getOpcode())) goto end;

    // Look for DeclRefExprs -- these reference induction variables
    LHS = B->getLHS();
    DR = dyn_cast<DeclRefExpr>(LHS->IgnoreImpCasts());
    if(!DR) goto end;

    // Make sure that both the expression acting on the induction variable &
    // the variable itself are scalar integers (casts may change types)
    Var = getVarIfScalarInt(DR->getDecl());
    if(!isScalarIntType(LHS->getType()) || !Var) goto end;
    InductionVars[Var] = B;
end:
    return true;
  }

  // Visit unary operators to find induction variables.
  bool VisitUnaryOperator(UnaryOperator *U) {
    Expr *SubExpr;
    DeclRefExpr *DR;
    VarDecl *Var;

    // Filter out irrelevant operation types
    if(!UnaryFilt(U->getOpcode())) goto end;

    // Look for DeclRefExprs -- these reference induction variables
    SubExpr = U->getSubExpr();
    DR = dyn_cast<DeclRefExpr>(SubExpr->IgnoreImpCasts());
    if(!DR) goto end;

    // Make sure that both the expression acting on the induction variable &
    // the variable itself are scalar integers (casts may change types)
    Var = getVarIfScalarInt(DR->getDecl());
    if(!isScalarIntType(SubExpr->getType()) || !Var) goto end;
    InductionVars[Var] = U;
end:
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
/// Induction variable b has different ranges in each of the nested loops.
class LoopNestTraversal : public RecursiveASTVisitor<LoopNestTraversal> {
public:
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
          new InductionVariable(Var->first, InitExpr, CondExpr, UpdateExpr));
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
  LoopNestTraversal Loops;
  ArrayAccessPattern ArrAccesses;

  // Gather loop nest information, including induction variables
  Loops.InitTraversal();
  Loops.TraverseStmt(S);
  Loops.PruneInductionVars();

  // Find array/pointer accesses.
  ArrAccesses.InitTraversal();
  ArrAccesses.TraverseStmt(S);

  // Reconstruct array subscript expressions with induction variable references
  // replaced by their bounds.
  for(auto &Read : ArrAccesses.getArrayReads()) {
    IVMap IVs;
    ForLoopInfoPtr Scope = Loops.getEnclosingLoop(Read);
    for(auto &Var : Read.getVarsInIdx()) {
      const InductionVariablePtr IV = findInductionVariable(Var, Scope);
      if(IV) IVs.insert(IVPair(Var, std::move(IV)));
    }

    Expr *UpperBound = cloneWithIV(Read.getIndex(), IVs, true),
         *LowerBound = cloneWithIV(Read.getIndex(), IVs, false);
    if(UpperBound && LowerBound)
      ToPrefetch.emplace_back(PrefetchRange::Read, Read.getBase(),
                              LowerBound, UpperBound);
  }

  for(auto &Write : ArrAccesses.getArrayWrites()) {
    IVMap IVs;
    ForLoopInfoPtr Scope = Loops.getEnclosingLoop(Write);
    for(auto &Var : Write.getVarsInIdx()) {
      const InductionVariablePtr IV = findInductionVariable(Var, Scope);
      if(IV) IVs.insert(IVPair(Var, std::move(IV)));
    }

    Expr *UpperBound = cloneWithIV(Write.getIndex(), IVs, true),
         *LowerBound = cloneWithIV(Write.getIndex(), IVs, false);
    if(UpperBound && LowerBound)
      ToPrefetch.emplace_back(PrefetchRange::Write, Write.getBase(),
                              LowerBound, UpperBound);
  }
}

//===----------------------------------------------------------------------===//
// Reconstruction API
//

Expr *
PrefetchAnalysis::cloneWithIV(Expr *E, const IVMap &IVs, bool Upper) const {
  Expr *NewE = nullptr;
  BinaryOperator *B;
  UnaryOperator *U;
  DeclRefExpr *D;
  ImplicitCastExpr *C;
  IntegerLiteral *I;

  // TODO better way to switch on type?
  if((B = dyn_cast<BinaryOperator>(E)))
    NewE = cloneBinaryOperation(B, IVs, Upper);
  else if((U = dyn_cast<UnaryOperator>(E)))
    NewE = cloneUnaryOperation(U, IVs, Upper);
  else if((D = dyn_cast<DeclRefExpr>(E)))
    NewE = cloneDeclRefExpr(D, IVs, Upper);
  else if((C = dyn_cast<ImplicitCastExpr>(E)))
    NewE = cloneImplicitCastExpr(C, IVs, Upper);
  else if((I = dyn_cast<IntegerLiteral>(E)))
    NewE = cloneIntegerLiteral(I, IVs, Upper);
  else {
    // TODO delete
    llvm::dbgs() << "Unhandled expression:\n";
    E->dump();
  }

  return NewE;
}

Expr *PrefetchAnalysis::clone(Expr *E) const {
  IVMap Dummy; // DeclRefExprs will never replace declaration usage
  return cloneWithIV(E, Dummy, false);
}

Expr *PrefetchAnalysis::cloneBinaryOperation(BinaryOperator *B,
                                             const IVMap &IVs,
                                             bool Upper) const {
  Expr *LHS = cloneWithIV(B->getLHS(), IVs, Upper),
       *RHS = cloneWithIV(B->getRHS(), IVs, Upper);
  if(!LHS || !RHS) return nullptr;
  return new (*Ctx) BinaryOperator(LHS, RHS, B->getOpcode(),
                                   B->getType(),
                                   B->getValueKind(),
                                   B->getObjectKind(),
                                   SourceLocation(),
                                   B->isFPContractable());
}

Expr *PrefetchAnalysis::cloneUnaryOperation(UnaryOperator *U,
                                            const IVMap &IVs,
                                            bool Upper) const {
  Expr *Sub = cloneWithIV(U->getSubExpr(), IVs, Upper);
  if(!Sub) return nullptr;
  return new (*Ctx) UnaryOperator(Sub, U->getOpcode(),
                                  U->getType(),
                                  U->getValueKind(),
                                  U->getObjectKind(),
                                  SourceLocation());
}

Expr *PrefetchAnalysis::cloneDeclRefExpr(DeclRefExpr *D,
                                         const IVMap &IVs,
                                         bool Upper) const {
  VarDecl *VD;
  IVMap::const_iterator it;

  VD = dyn_cast<VarDecl>(D->getDecl());
  if(!VD) goto no_replace;
  it = IVs.find(VD);
  if(it == IVs.end()) goto no_replace;
  if(Upper) return clone(it->second->getUpperBound());
  else return clone(it->second->getLowerBound());

no_replace:
  return new (*Ctx) DeclRefExpr(D->getDecl(),
                                D->refersToEnclosingVariableOrCapture(),
                                D->getType(),
                                D->getValueKind(),
                                SourceLocation(),
                                D->getNameInfo().getInfo());
}

Expr *PrefetchAnalysis::cloneImplicitCastExpr(ImplicitCastExpr *C,
                                              const IVMap &IVs,
                                              bool Upper) const {
  Expr *Sub = cloneWithIV(C->getSubExpr(), IVs, Upper);
  if(!Sub) return nullptr;

  if(C->getCastKind() == CastKind::CK_LValueToRValue &&
     Sub->getValueKind() == VK_RValue) return Sub;
  else
    return new (*Ctx) ImplicitCastExpr(ImplicitCastExpr::OnStack,
                                       C->getType(),
                                       C->getCastKind(),
                                       Sub,
                                       C->getValueKind());
}

Expr *PrefetchAnalysis::cloneIntegerLiteral(IntegerLiteral *L,
                                            const IVMap &IVs,
                                            bool Upper) const {
  return new (*Ctx) IntegerLiteral(*Ctx, L->getValue(),
                                   L->getType(),
                                   SourceLocation());
}

//===----------------------------------------------------------------------===//
// Prefetch analysis API
//

void PrefetchAnalysis::analyzeStmt() {
  if(!Ctx || !S) return;

  // TODO other types of statements
  if(isa<ForStmt>(S)) analyzeForStmt();
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

