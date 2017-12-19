//===--- PopcornUtil.h - Popcorn Linux Utilities ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_CODEGEN_POPCORNUTIL_H
#define LLVM_CLANG_CODEGEN_POPCORNUTIL_H

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Module.h>
#include <clang/Basic/TargetOptions.h>
#include <memory>

namespace clang {
namespace Popcorn {

/// Return a TargetOptions with features appropriate for Popcorn Linux
std::shared_ptr<TargetOptions> GetPopcornTargetOpts(llvm::StringRef TripleStr);

/// Strip target-specific CPUs & features from function attributes in all
/// functions in the module.  This silences warnings from the compiler about
/// unsupported target features when compiling the IR for multiple
/// architectures.
void StripTargetAttributes(llvm::Module &M);


/// Add the target-features attribute specified in TargetOpts to every function
/// in module M.
void AddArchSpecificTargetFeatures(llvm::Module &M,
                                   std::shared_ptr<TargetOptions> TargetOpts);

} /* end Popcorn namespace */
} /* end clang namespace */

#endif
