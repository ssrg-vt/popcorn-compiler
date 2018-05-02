//===- Prefetch.h - Prefetching Analysis for Statements -----------*- C++ --*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interface for building prefetching calls based on the
// prefetching analysis.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_CODEGEN_PREFETCHBUILDER_H
#define LLVM_CLANG_CODEGEN_PREFETCHBUILDER_H

#include "CodeGenFunction.h"
#include "clang/Sema/PrefetchAnalysis.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {

/// Generate calls to the prefetching API for analyzed regions.
class PrefetchBuilder {
public:
  PrefetchBuilder(clang::CodeGen::CodeGenFunction *CGF)
    : CGM(CGF->CGM), CGF(*CGF), Ctx(CGF->getContext()) {}

  /// Emit prefetching API declarations.
  void EmitPrefetchCallDeclarations();

  /// Emit a prefetch call for a particular range of memory.
  void EmitPrefetchCall(const PrefetchRange &P);

  /// Emit a call to send the prefetch requests to the OS.
  void EmitPrefetchExecuteCall();

  // TODO print & dump

private:
  clang::CodeGen::CodeGenModule &CGM;
  clang::CodeGen::CodeGenFunction &CGF;
  ASTContext &Ctx;

  // Prefetch API declarations
  llvm::Constant *Prefetch, *Execute;

  Expr *buildArrayIndexAddress(VarDecl *Base, Expr *Subscript);
};

} // end namespace clang

#endif

