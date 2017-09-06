//===- LoopPaths.h - Enumerate paths in loops -------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides Popcorn-specific utility APIs.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_POPCORNUTIL_H
#define LLVM_ANALYSIS_POPCORNUTIL_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

namespace llvm {
namespace Popcorn {

#define POPCORN_META "popcorn"
#define POPCORN_EQPOINT "eqpoint"

/// Add named metadata node with string operand to an instruction.
static inline void addMetadata(Instruction *I, StringRef name, StringRef op) {
  SmallVector<Metadata *, 2> MetaOps;
  LLVMContext &C = I->getContext();
  MDNode *MetaNode = I->getMetadata(name);

  if(MetaNode) {
    for(auto &Op : MetaNode->operands()) {
      if(isa<MDString>(Op) && cast<MDString>(Op)->getString() == op) return;
      else MetaOps.push_back(Op);
    }
  }

  MetaOps.push_back(MDString::get(C, op));
  MetaNode = MDNode::get(C, MetaOps);
  I->setMetadata(name, MetaNode);
}

/// Remove string operand from named metadata node.
static inline void
removeMetadata(Instruction *I, StringRef name, StringRef op) {
  SmallVector<Metadata *, 2> MetaOps;
  MDNode *MetaNode = I->getMetadata(name);

  if(MetaNode) {
    for(auto &Op : MetaNode->operands()) {
      if(isa<MDString>(Op) && cast<MDString>(Op)->getString() == op) continue;
      else MetaOps.push_back(Op);
    }

    if(MetaOps.size()) {
      MetaNode = MDNode::get(I->getContext(), MetaOps);
      I->setMetadata(name, MetaNode);
    }
    else I->setMetadata(name, nullptr);
  }
}

/// Check to see if instruction has named metadata node with string operand.
static inline bool
hasMetadata(const Instruction *I, StringRef name, StringRef op) {
  const MDNode *MetaNode = I->getMetadata(name);
  if(MetaNode)
    for(auto &Op : MetaNode->operands())
      if(isa<MDString>(Op) && cast<MDString>(Op)->getString() == op)
        return true;
  return false;
}

/// Return whether the instruction is a "true" call site, i.e., not an LLVM
/// IR-level intrinsic.
bool isCallSite(const Instruction *I) {
  if((isa<CallInst>(I) || isa<InvokeInst>(I)) && !isa<IntrinsicInst>(I))
    return true;
  else return false;
}

/// Add metadata to an instruction marking it as an equivalence point.
void addEquivalencePointMetadata(Instruction *I) {
  addMetadata(I, POPCORN_META, POPCORN_EQPOINT);
}

/// Remove metadata from an instruction marking it as an equivalence point.
void removeEquivalencePointMetadata(Instruction *I) {
  removeMetadata(I, POPCORN_META, POPCORN_EQPOINT);
}

/// Return whether an instruction is an equivalence point.  The instruction must
/// satisfy one of the following:
///
/// 1. Is a function call site (not an intrinsic function call)
/// 2. Analysis has tagged the instruction with appropriate metadata
bool isEquivalencePoint(const Instruction *I) {
  if(isCallSite(I)) return true;
  else if(hasMetadata(I, POPCORN_META, POPCORN_EQPOINT)) return true;
  else return false;
}

}
}

#endif

