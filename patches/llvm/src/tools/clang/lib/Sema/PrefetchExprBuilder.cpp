//=- PrefetchExprBuilder.cpp - Prefetching expression builder ------------*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a set of utilities for building expressions for
// prefetching.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/Sema/PrefetchAnalysis.h"
#include "clang/Sema/PrefetchExprBuilder.h"
#include "llvm/Support/Debug.h"

using namespace clang;

//===----------------------------------------------------------------------===//
// Prefetch expression comparisondefinitions
//

static bool
BinaryOperatorEqual(const BinaryOperator *A, const BinaryOperator *B) {
  if(A->getOpcode() != B->getOpcode()) return false;
  else return PrefetchExprEquality::exprEqual(A->getRHS(), B->getRHS()) &&
              PrefetchExprEquality::exprEqual(A->getLHS(), B->getLHS());
}

static bool UnaryOperatorEqual(const UnaryOperator *A, const UnaryOperator *B) {
  if(A->getOpcode() != B->getOpcode()) return false;
  else return PrefetchExprEquality::exprEqual(A->getSubExpr(), B->getSubExpr());
}

static bool DeclRefExprEqual(const DeclRefExpr *A, const DeclRefExpr *B) {
  if(A->getDecl() == B->getDecl()) return true;
  else return false;
}

static bool
ImplicitCastExprEqual(const ImplicitCastExpr *A, const ImplicitCastExpr *B) {
  if(A->getCastKind() != B->getCastKind()) return false;
  else return PrefetchExprEquality::exprEqual(A->getSubExpr(), B->getSubExpr());
}

static bool
IntegerLiteralEqual(const IntegerLiteral *A, const IntegerLiteral *B) {
  return A->getValue() == B->getValue();
}

bool PrefetchExprEquality::exprEqual(const Expr *A, const Expr *B) {
  const BinaryOperator *B_A, *B_B;
  const UnaryOperator *U_A, *U_B;
  const DeclRefExpr *D_A, *D_B;
  const ImplicitCastExpr *C_A, *C_B;
  const IntegerLiteral *I_A, *I_B;

  if(!A || ! B) return false;

  // Check common characteristics.  Note that by checking the statement class,
  // we know the expressions are of the same type and can use cast<> below.
  if(A->getStmtClass() != B->getStmtClass() ||
     A->getType() != B->getType() ||
     A->getValueKind() != B->getValueKind() ||
     A->getObjectKind() != B->getObjectKind()) return false;

  // TODO better way to switch on type?
  if((B_A = dyn_cast<BinaryOperator>(A))) {
    B_B = cast<BinaryOperator>(B);
    return BinaryOperatorEqual(B_A, B_B);
  }
  else if((U_A = dyn_cast<UnaryOperator>(A))) {
    U_B = cast<UnaryOperator>(B);
    return UnaryOperatorEqual(U_A, U_B);
  }
  else if((D_A = dyn_cast<DeclRefExpr>(A))) {
    D_B = cast<DeclRefExpr>(B);
    return DeclRefExprEqual(D_A, D_B);
  }
  else if((C_A = dyn_cast<ImplicitCastExpr>(A))) {
    C_B = cast<ImplicitCastExpr>(B);
    return ImplicitCastExprEqual(C_A, C_B);
  }
  else if((I_A = dyn_cast<IntegerLiteral>(A))) {
    I_B = cast<IntegerLiteral>(B);
    return IntegerLiteralEqual(I_A, I_B);
  }
  else return false;
}

//===----------------------------------------------------------------------===//
// Modifier class definitions
//

void PrefetchExprBuilder::Modifier::ClassifyModifier(const Expr *E,
                                                     const ASTContext *Ctx) {
  unsigned Bits;
  const DeclRefExpr *DR;
  const BinaryOperator *B;
  const IntegerLiteral *L;
  QualType BaseTy;

  Ty = Unknown;
  if(!E) return;

  E = E->IgnoreImpCasts();
  if((B = dyn_cast<BinaryOperator>(E))) {
    // Note: both operands *must* have same type
    BaseTy = B->getLHS()->getType().getDesugaredType(*Ctx);
    assert(PrefetchAnalysis::isScalarIntType(BaseTy) &&
           "Invalid expression type");
    Bits = PrefetchAnalysis::getTypeSize(cast<BuiltinType>(BaseTy)->getKind());

    switch(B->getOpcode()) {
    default: Ty = None; break;
    case BO_LT:
      Ty = Sub;
      Val = llvm::APInt(Bits, 1, false);
      break;
    case BO_GT:
      Ty = Add;
      Val = llvm::APInt(Bits, 1, false);
      break;
    // TODO hybrid math/assign operations
    }
  }
  else if((DR = dyn_cast<DeclRefExpr>(E))) Ty = None;
  else if((L = dyn_cast<IntegerLiteral>(E))) Ty = None;
}

//===----------------------------------------------------------------------===//
// Prefetch expression builder definitions
//

typedef PrefetchExprBuilder::BuildInfo BuildInfo;

Expr *PrefetchExprBuilder::cloneWithReplacement(Expr *E, BuildInfo &Info) {
  BinaryOperator *B;
  UnaryOperator *U;
  DeclRefExpr *D;
  ImplicitCastExpr *C;
  IntegerLiteral *I;

  if(!E) return nullptr;

  // TODO better way to switch on type?
  if((B = dyn_cast<BinaryOperator>(E)))
    return cloneBinaryOperator(B, Info);
  else if((U = dyn_cast<UnaryOperator>(E)))
    return cloneUnaryOperator(U, Info);
  else if((D = dyn_cast<DeclRefExpr>(E)))
    return cloneDeclRefExpr(D, Info);
  else if((C = dyn_cast<ImplicitCastExpr>(E)))
    return cloneImplicitCastExpr(C, Info);
  else if((I = dyn_cast<IntegerLiteral>(E)))
    return cloneIntegerLiteral(I, Info);
  else {
    // TODO delete
    llvm::dbgs() << "Unhandled expression:\n";
    if(Info.dumpInColor) E->dumpColor();
    else E->dump();
  }

  return nullptr;
}

Expr *PrefetchExprBuilder::clone(Expr *E, ASTContext *Ctx) {
  ReplaceMap Dummy; // No variables, don't replace any DeclRefExprs
  BuildInfo DummyInfo(Ctx, Dummy, true);
  return cloneWithReplacement(E, DummyInfo);
}

Expr *PrefetchExprBuilder::cloneBinaryOperator(BinaryOperator *B,
                                               BuildInfo &Info) {
  Expr *LHS = cloneWithReplacement(B->getLHS(), Info),
       *RHS = cloneWithReplacement(B->getRHS(), Info);
  if(!LHS || !RHS) return nullptr;
  return new (*Info.Ctx) BinaryOperator(LHS, RHS, B->getOpcode(),
                                        B->getType(),
                                        B->getValueKind(),
                                        B->getObjectKind(),
                                        SourceLocation(),
                                        B->isFPContractable());
}

Expr *PrefetchExprBuilder::cloneUnaryOperator(UnaryOperator *U,
                                              BuildInfo &Info) {
  Expr *Sub = cloneWithReplacement(U->getSubExpr(), Info);
  if(!Sub) return nullptr;
  return new (*Info.Ctx) UnaryOperator(Sub, U->getOpcode(),
                                       U->getType(),
                                       U->getValueKind(),
                                       U->getObjectKind(),
                                       SourceLocation());
}

Expr *PrefetchExprBuilder::cloneDeclRefExpr(DeclRefExpr *D,
                                            BuildInfo &Info) {
  Expr *Clone = nullptr;
  VarDecl *VD;
  ReplaceMap::const_iterator it;

  // If the variable is relevant and we haven't replaced it before, replace it
  // with the specified expression.
  if((VD = dyn_cast<VarDecl>(D->getDecl())) &&
     (it = Info.VarReplace.find(VD)) != Info.VarReplace.end() &&
     !Info.SeenVars.count(VD)) {
    Info.SeenVars.insert(VD);
    Clone = cloneWithReplacement(it->second, Info);
    Info.SeenVars.erase(VD);
    return Clone;
  }

  // Clone the DeclRefExpr if the variable isn't relevant or if cloning the
  // replacement failed.
  return new (*Info.Ctx) DeclRefExpr(D->getDecl(),
                                     D->refersToEnclosingVariableOrCapture(),
                                     D->getType(),
                                     D->getValueKind(),
                                     SourceLocation(),
                                     D->getNameInfo().getInfo());
}

Expr *PrefetchExprBuilder::cloneImplicitCastExpr(ImplicitCastExpr *C,
                                                 BuildInfo &Info) {
  Expr *Sub = cloneWithReplacement(C->getSubExpr(), Info);
  if(!Sub) return nullptr;

  // Avoid the situation that when replacing an induction variable with another
  // expression we accidentally chain together 2 implicit casts (which causes
  // CodeGen to choke).
  if(C->getCastKind() == CastKind::CK_LValueToRValue &&
     Sub->getValueKind() == VK_RValue)
    return Sub;
  else
    return new (*Info.Ctx) ImplicitCastExpr(ImplicitCastExpr::OnStack,
                                            C->getType(),
                                            C->getCastKind(),
                                            Sub,
                                            C->getValueKind());
}

Expr *PrefetchExprBuilder::cloneIntegerLiteral(IntegerLiteral *L,
                                               BuildInfo &Info) {
  return new (*Info.Ctx) IntegerLiteral(*Info.Ctx, L->getValue(),
                                        L->getType(),
                                        SourceLocation());
}

Expr *PrefetchExprBuilder::cloneAndModifyExpr(Expr *E,
                                              const Modifier &Mod,
                                              ASTContext *Ctx) {
  BinaryOperator::Opcode Op;
  IntegerLiteral *RHS;

  E = clone(E, Ctx);
  if(!E) return nullptr;

  switch(Mod.getType()) {
  case Modifier::Add: Op = BO_Add; break;
  case Modifier::Sub: Op = BO_Sub; break;
  case Modifier::Mul: Op = BO_Mul; break;
  case Modifier::Div: Op = BO_Div; break;
  case Modifier::None: return E; // Nothing to do
  case Modifier::Unknown: return nullptr; // Couldn't classify
  }

  RHS = new (*Ctx) IntegerLiteral(*Ctx, Mod.getVal(),
                                  E->getType(),
                                  SourceLocation());
  return new (*Ctx) BinaryOperator(E, RHS, Op,
                                   E->getType(),
                                   E->getValueKind(),
                                   E->getObjectKind(),
                                   SourceLocation(),
                                   false);
}

