//===--- PopcornUtil.cpp - LLVM Popcorn Linux Utilities -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <clang/CodeGen/PopcornUtil.h>
#include <llvm/ADT/Triple.h>
#include <llvm/ADT/SmallVector.h>

using namespace clang;
using namespace llvm;

/// Target-specific function attributes
SmallVector<std::string, 2> TargetAttributes = {
  "target-cpu",
  "target-features"
};

std::shared_ptr<TargetOptions>
Popcorn::GetPopcornTargetOpts(StringRef TripleStr) {
  Triple Triple(Triple::normalize(TripleStr));
  assert(!Triple.getTriple().empty() && "Invalid target triple");

  std::shared_ptr<TargetOptions> Opts(new TargetOptions);
  Opts->Triple = Triple.getTriple();
  Opts->ABI = "";
  Opts->FPMath = "";
  Opts->FeaturesAsWritten.clear();
  Opts->LinkerVersion = "";
  Opts->Reciprocals.clear();

  switch(Triple.getArch()) {
  case Triple::ArchType::aarch64:
    Opts->ABI = "aapcs";
    Opts->CPU = "generic";
    Opts->FeaturesAsWritten.push_back("+neon");
    break;
  case Triple::ArchType::x86_64:
    Opts->CPU = "x86-64";
    Opts->FeaturesAsWritten.push_back("+sse");
    Opts->FeaturesAsWritten.push_back("+sse2");
    break;
  default: llvm_unreachable("Triple not currently supported on Popcorn");
  }

  return Opts;
}

void Popcorn::StripTargetAttributes(Module &M) {
  for(Function &F : M) {
    AttrBuilder AB(F.getAttributes(), AttributeSet::FunctionIndex);
    for(std::string &Attr : TargetAttributes) {
      if(F.hasFnAttribute(Attr))
        AB.removeAttribute(Attr);
    }
    F.setAttributes(
      AttributeSet::get(F.getContext(), AttributeSet::FunctionIndex, AB));
  }
}

