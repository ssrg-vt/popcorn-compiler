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

typedef std::shared_ptr<TargetOptions> TargetOptionsPtr;

TargetOptionsPtr Popcorn::GetPopcornTargetOpts(StringRef TripleStr) {
  Triple Triple(Triple::normalize(TripleStr));
  assert(!Triple.getTriple().empty() && "Invalid target triple");

  TargetOptionsPtr Opts(new TargetOptions);
  Opts->Triple = Triple.getTriple();
  Opts->ABI = "";
  Opts->FPMath = "";
  Opts->FeaturesAsWritten.clear();
  Opts->LinkerVersion = "";
  Opts->Reciprocals.clear();

  // TODO need to make CPU selectable & add target features according to CPU

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
    Opts->FeaturesAsWritten.push_back("+rtm");
    break;
  default: llvm_unreachable("Triple not currently supported on Popcorn");
  }

  return Opts;
}

void Popcorn::StripTargetAttributes(Module &M) {
  /// Target-specific function attributes
  static SmallVector<std::string, 2> TargetAttributes = {
    "target-cpu",
    "target-features"
  };

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

void Popcorn::AddArchSpecificTargetFeatures(Module &M,
                                            TargetOptionsPtr TargetOpts) {
  static const char *TF = "target-features";
  std::string AllFeatures("");

  for(auto &Feature : TargetOpts->FeaturesAsWritten)
    AllFeatures += Feature + ",";
  AllFeatures = AllFeatures.substr(0, AllFeatures.length() - 1);

  for(Function &F : M) {
    AttrBuilder AB(F.getAttributes(), AttributeSet::FunctionIndex);
    assert(!F.hasFnAttribute(TF) && "Target features weren't stripped");
    AB.addAttribute(TF, AllFeatures);
    F.setAttributes(
      AttributeSet::get(F.getContext(), AttributeSet::FunctionIndex, AB));
  }
}

