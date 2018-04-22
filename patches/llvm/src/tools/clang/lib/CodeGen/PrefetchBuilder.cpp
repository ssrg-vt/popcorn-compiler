//=- Prefetch.cpp - Prefetching Analysis for Structured Blocks -----------*-==//
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

#include "clang/CodeGen/PrefetchBuilder.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/Debug.h"

using namespace clang;

//===----------------------------------------------------------------------===//
// Prefetch builder API
//

void PrefetchBuilder::EmitPrefetchCallDeclarations() {
  using namespace clang::CodeGen;
  std::vector<llvm::Type *> ParamTypes;
  llvm::FunctionType *FnType;

  // declare void @popcorn_prefetch(i32, i8*, i8*)
  ParamTypes = { CGF.Int32Ty, CGF.Int8PtrTy, CGF.Int8PtrTy };
  FnType = llvm::FunctionType::get(CGF.VoidTy, ParamTypes, false);
  Prefetch = CGM.CreateRuntimeFunction(FnType, "popcorn_prefetch");

  // declare i64 @popcorn_prefetch_execute()
  ParamTypes.clear();
  FnType = llvm::FunctionType::get(CGF.Int64Ty, ParamTypes, false);
  Execute = CGM.CreateRuntimeFunction(FnType, "popcorn_prefetch_execute");
}

static llvm::Constant *getPrefetchKind(CodeGen::CodeGenFunction &CGF,
                                       enum PrefetchRange::Type Perm) {
  llvm::Type *Ty = llvm::Type::getInt32Ty(CGF.CurFn->getContext());
  switch(Perm) {
  case PrefetchRange::Read: return llvm::ConstantInt::get(Ty, 0);
  case PrefetchRange::Write: return llvm::ConstantInt::get(Ty, 1);
  default: llvm_unreachable("Invalid prefetch type\n"); return nullptr;
  }
}

Expr *PrefetchBuilder::buildArrayIndexAddress(VarDecl *Base, Expr *Subscript) {
  // Build DeclRefExpr for variable representing base
  QualType Ty = Base->getType(), ElemTy;
  DeclRefExpr *DRE = DeclRefExpr::Create(Ctx, NestedNameSpecifierLoc(),
                                         SourceLocation(), Base, false,
                                         Base->getSourceRange().getBegin(),
                                         Ty, VK_LValue);

  // Get an array subscript, e.g., arr[idx]
  Ty = Ty.getDesugaredType(Ctx);
  if(isa<ArrayType>(Ty)) ElemTy = cast<ArrayType>(ElemTy)->getElementType();
  else ElemTy = cast<PointerType>(Ty)->getPointeeType();
  Expr *Subscr = new (Ctx) ArraySubscriptExpr(DRE, Subscript, ElemTy, VK_RValue,
                                              OK_Ordinary, SourceLocation());

  // Get the address of the array index, e.g., &arr[idx]
  QualType RePtrTy = Ctx.getPointerType(ElemTy);
  UnaryOperator *Addr = new (Ctx) UnaryOperator(Subscr, UO_AddrOf, RePtrTy,
                                                VK_LValue, OK_Ordinary,
                                                SourceLocation());

  // Finally, cast it to a void *, e.g., (void *)&arr[idx]
  QualType VoidPtrTy = Ctx.getPointerType(Ctx.VoidTy.withConst());
  return ImplicitCastExpr::Create(Ctx, VoidPtrTy, CK_BitCast, Addr, nullptr,
                                  VK_RValue);
}

void PrefetchBuilder::EmitPrefetchCall(const PrefetchRange &P) {
  Expr *StartAddr, *EndAddr;
  CodeGen::RValue LoweredStart, LoweredEnd;
  std::vector<llvm::Value *> Params;

  StartAddr = buildArrayIndexAddress(P.getArray(), P.getStart());
  EndAddr = buildArrayIndexAddress(P.getArray(), P.getEnd());
  LoweredStart = CGF.EmitAnyExpr(StartAddr);
  LoweredEnd = CGF.EmitAnyExpr(EndAddr);
  Params = { getPrefetchKind(CGF, P.getType()),
             LoweredStart.getScalarVal(),
             LoweredEnd.getScalarVal() };
  CGF.EmitCallOrInvoke(Prefetch, Params);
}

void PrefetchBuilder::EmitPrefetchExecuteCall() {
  std::vector<llvm::Value *> Params;
  CGF.EmitCallOrInvoke(Execute, Params);
}

