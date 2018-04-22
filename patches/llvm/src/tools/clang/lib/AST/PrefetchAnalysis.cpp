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
#include <utility>

using namespace clang;

//===----------------------------------------------------------------------===//
// Common utilities
//

/// Return whether a type is both scalar and integer.
static bool isScalarIntType(const QualType &Ty) {
  return Ty->isIntegerType() && Ty->isScalarType();
}

/// Return the variable declaration if the declared value is a variable and if
/// it is a scalar integer type.
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

/// An array access.
class ArrayAccess {
public:
  ArrayAccess(VarDecl *Base, Expr *Idx) : Valid(true), Base(Base), Idx(Idx) {}

  bool isValid() const { return Valid; }
  VarDecl *getBase() const { return Base; }
  Expr *getIndex() const { return Idx; }
  const VarVec &getVarsInIdx() const { return VarsInIdx; }

  void setInvalid() { Valid = false; }
  void addVarInIdx(VarDecl *V) { if(V != Base) VarsInIdx.push_back(V); }

  void print(llvm::raw_ostream &O, PrintingPolicy &Policy) const {
    O << "Array: " << Base->getName() << "\n";
    O << "Index expression: ";
    Idx->printPretty(O, nullptr, Policy);
    O << "\nVariables used in index calculation:";
    for(auto Var : VarsInIdx) O << " " << Var->getName();
    O << "\n";
  }

  void dump(PrintingPolicy &Policy) const { print(llvm::dbgs(), Policy); }

private:
  bool Valid;
  VarDecl *Base; // The array base
  Expr *Idx; // Expression used to calculate index
  VarVec VarsInIdx; // Variables used in index calculation
};

/// Traverse a statement looking for array accesses.
// TODO *** NEED TO LIMIT TO AFFINE ACCESSES ***
class ArrayAccessPattern : public RecursiveASTVisitor<ArrayAccessPattern> {
public:
  /// Which sub-tree of a binary operator we're traversing.  This determines
  /// whether we're reading or writing the array.
  enum TraverseStructure { LHS, RHS, None };

  /// Traverse a statement.  There's a couple of special traversal rules:
  ///
  ///  - If it's an assignment operation, record structure of the traversal
  ///    before visiting each of the left & right sub-trees
  ///  - If it's an array subscript, record all variables used to calculate
  ///    the index
  bool TraverseStmt(Stmt *S) {
    BinaryOperator *BinOp = dyn_cast<BinaryOperator>(S);
    ArraySubscriptExpr *Subscript = dyn_cast<ArraySubscriptExpr>(S);

    if(BinOp && FilterAssignOp(BinOp->getOpcode())) {
      Side = LHS;
      RecursiveASTVisitor<ArrayAccessPattern>::TraverseStmt(BinOp->getLHS());
      Side = RHS;
      RecursiveASTVisitor<ArrayAccessPattern>::TraverseStmt(BinOp->getRHS());
      Side = None;
    }
    else if(Subscript) {
      RecursiveASTVisitor<ArrayAccessPattern>::TraverseStmt(S);
      CurAccess = nullptr; // Don't record any more variables
    }
    else RecursiveASTVisitor<ArrayAccessPattern>::TraverseStmt(S);
    return true;
  }

  /// Analyze an array access; in particular, the index.
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *Sub) {
    Expr *Base = Sub->getBase(), *Idx = Sub->getIdx();
    DeclRefExpr *DR;
    VarDecl *VD;

    assert(Side != None && "Unhandled tree structure");

    DR = dyn_cast<DeclRefExpr>(Base->IgnoreImpCasts());
    if(!DR) goto end;
    VD = dyn_cast<VarDecl>(DR->getDecl());
    if(!VD) goto end;
    if(Side == LHS) {
      ArrayWrites.emplace_back(VD, Idx);
      CurAccess = &ArrayWrites.back();
    }
    else {
      ArrayReads.emplace_back(VD, Idx);
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

  // Traversal state
  enum TraverseStructure Side;
  ArrayAccess *CurAccess;
};

//===----------------------------------------------------------------------===//
// Prefetch analysis -- ForStmts
//

/// An induction variable and expressions describing its range.
class InductionVariable {
public:
  /// The direction of change for the induction variable
  enum Direction {
    Increases, // Update changes variable from lower to higher values
    Decreases, // Update changes variable from higher to lower values
    Unknown // Update has an unknown effect, e.g., container interators
  };

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
      LowerB = Init;
      UpperB = Cond;
    }
    else if(Dir == Decreases) {
      LowerB = Cond;
      UpperB = Init;
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
    O << "Induction Variable: " << Var->getName() << "\n";
    O << "Direction: ";
    switch(Dir) {
    case Increases: O << "increases\n"; break;
    case Decreases: O << "decreases\n"; break;
    case Unknown: O << "unknown update direction\n"; break;
    }
    if(LowerB) {
      O << "Lower bound:\n";
      LowerB->printPretty(O, nullptr, Policy);
    }
    if(UpperB) {
      O << "Upper bound:\n";
      UpperB->printPretty(O, nullptr, Policy);
    }
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
};

/// Map a variable declaration to the induction variable's information.
typedef llvm::DenseMap<VarDecl *, InductionVariable> IVMap;
typedef std::pair<VarDecl *, InductionVariable> IVPair;

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

/// Search a for-loop statement for array access patterns based on loop
/// induction variables that can be prefetched at runtime.
void PrefetchAnalysis::analyzeForStmt() {
  ForStmt *Loop = cast<ForStmt>(S);
  Expr *InitExpr, *CondExpr, *UpdateExpr;
  IVFinder<NoUnaryOp, FilterAssignOp> Init;
  IVFinder<NoUnaryOp, FilterRelationalOp> Cond;
  IVFinder<FilterMathOp, FilterMathLogicOp> Update;
  IVMap IVs;
  ArrayAccessPattern ArrAccesses;

  // Start by finding the induction variables in the loop expressions.
  Init.TraverseStmt(Loop->getInit());
  Cond.TraverseStmt(Loop->getCond());
  Update.TraverseStmt(Loop->getInc());

  // Find induction variables which are referenced in all three
  const IVBoundMap &InitVars = Init.getInductionVars();
  for(auto IV = InitVars.begin(), E = InitVars.end(); IV != E; IV++) {
    InitExpr = IV->second;
    CondExpr = Cond.getVarBound(IV->first),
    UpdateExpr = Update.getVarBound(IV->first);
    if(InitExpr && CondExpr && UpdateExpr)
      IVs.insert(IVPair(IV->first, InductionVariable(IV->first, InitExpr,
                                                     CondExpr, UpdateExpr)));
  }

  // Look for array/pointer accesses based on induction variables
  ArrAccesses.TraverseStmt(Loop->getBody());
  for(auto &Read : ArrAccesses.getArrayReads()) {
    bool AllInductionVars = true;
    for(auto &Var : Read.getVarsInIdx())
      if(!InitVars.count(Var)) AllInductionVars = false;

    if(AllInductionVars) {
      // TODO this is totally wrong for anything besides trivial accesses.  We
      // need to do the following:
      //
      //  - Clone the array index expression sub-tree
      //  - Generate 2 new expressions - one with the lower bound(s) of the
      //    induction variable(s) and one with higher bound(s)
      VarDecl *IdxVar = Read.getVarsInIdx()[0];
      InductionVariable &VarInfo = IVs.find(IdxVar)->second;
      Expr *LB = cast<BinaryOperator>(VarInfo.getLowerBound())->getRHS(),
           *UB = cast<BinaryOperator>(VarInfo.getUpperBound())->getRHS();
      ToPrefetch.emplace_back(PrefetchRange::Read, Read.getBase(), LB, UB);
    }
  }

  for(auto &Write : ArrAccesses.getArrayWrites()) {
    bool AllInductionVars = true;
    for(auto &Var : Write.getVarsInIdx())
      if(!InitVars.count(Var)) AllInductionVars = false;

    if(AllInductionVars) {
      // TODO this is totally wrong for anything besides trivial accesses.  We
      // need to do the following:
      //
      //  - Clone the array index expression sub-tree
      //  - Generate 2 new expressions - one with the lower bound(s) of the
      //    induction variable(s) and one with higher bound(s)
      VarDecl *IdxVar = Write.getVarsInIdx()[0];
      InductionVariable &VarInfo = IVs.find(IdxVar)->second;
      Expr *LB = cast<BinaryOperator>(VarInfo.getLowerBound())->getRHS(),
           *UB = cast<BinaryOperator>(VarInfo.getUpperBound())->getRHS();
      ToPrefetch.emplace_back(PrefetchRange::Write, Write.getBase(), LB, UB);
    }
  }
}

//===----------------------------------------------------------------------===//
// Prefetch analysis API
//

void PrefetchAnalysis::analyzeStmt() {
  // TODO other types of statements
  if(isa<ForStmt>(S)) analyzeForStmt();
}

void PrefetchAnalysis::print(llvm::raw_ostream &O, ASTContext &Ctx) const {
  PrintingPolicy Policy(Ctx.getLangOpts());
  for(auto &Range : ToPrefetch) {
    O << "Array '" << Range.getArray()->getName() << "': ";
    Range.getStart()->printPretty(O, nullptr, Policy);
    O << " to ";
    Range.getEnd()->printPretty(O, nullptr, Policy);
    O << " (" << Range.getTypeName() << ")\n";
  }
}

