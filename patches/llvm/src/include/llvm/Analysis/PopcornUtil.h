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
#include "llvm/IR/Module.h"

namespace llvm {
namespace Popcorn {

#define POPCORN_META "popcorn"
#define POPCORN_MIGPOINT "migpoint"
#define POPCORN_HTM_BEGIN "htmbegin"
#define POPCORN_HTM_END "htmend"

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
static inline bool isCallSite(const Instruction *I) {
  if((isa<CallInst>(I) || isa<InvokeInst>(I)) && !isa<IntrinsicInst>(I))
    return true;
  else return false;
}

/// Add metadata to an instruction marking it as an equivalence point.
static inline void addEquivalencePointMetadata(Instruction *I) {
  addMetadata(I, POPCORN_META, POPCORN_MIGPOINT);
}

/// Remove metadata from an instruction marking it as an equivalence point.
static inline void removeEquivalencePointMetadata(Instruction *I) {
  removeMetadata(I, POPCORN_META, POPCORN_MIGPOINT);
}

static inline bool hasEquivalencePointMetadata(Instruction *I) {
  return hasMetadata(I , POPCORN_META, POPCORN_MIGPOINT);
}

/// Return whether an instruction is an equivalence point.  The instruction must
/// satisfy one of the following:
///
/// 1. Is a function call site (not an intrinsic function call)
/// 2. Analysis has tagged the instruction with appropriate metadata
static inline bool isEquivalencePoint(const Instruction *I) {
  if(isCallSite(I)) return true;
  else return hasMetadata(I, POPCORN_META, POPCORN_MIGPOINT);
}

/// Add metadata to an instruction marking it as an HTM begin point.
static inline void addHTMBeginMetadata(Instruction *I) {
  addMetadata(I, POPCORN_META, POPCORN_HTM_BEGIN);
}

/// Remove metadata from an instruction marking it as an HTM begin point.
static inline void removeHTMBeginMetadata(Instruction *I) {
  removeMetadata(I, POPCORN_META, POPCORN_HTM_BEGIN);
}

/// Return whether an instruction is an HTM begin point.
static inline bool isHTMBeginPoint(Instruction *I) {
  return hasMetadata(I, POPCORN_META, POPCORN_HTM_BEGIN);
}

/// Add metadata to an instruction marking it as an HTM end point.
static inline void addHTMEndMetadata(Instruction *I) {
  addMetadata(I, POPCORN_META, POPCORN_HTM_END);
}

/// Remove metadata from an instruction marking it as an HTM end point.
static inline void removeHTMEndMetadata(Instruction *I) {
  removeMetadata(I, POPCORN_META, POPCORN_HTM_END);
}

/// Return whether an instruction is an HTM end point.
static inline bool isHTMEndPoint(Instruction *I) {
  return hasMetadata(I, POPCORN_META, POPCORN_HTM_END);
}

#define POPCORN_INST_KEY "popcorn-inst-ty"

/// Type of instrumentation.
enum InstrumentType {
  HTM = 0,
  Cycles,
  None,
  NumVals // Don't use!
};

/// Mark the module as having a certain instrumentation type.
static inline void
setInstrumentationType(Module &M, enum InstrumentType Ty) {
  switch(Ty) {
  case HTM:
    M.addModuleFlag(Module::Error, POPCORN_INST_KEY, HTM);
    break;
  case Cycles:
    M.addModuleFlag(Module::Error, POPCORN_INST_KEY, Cycles);
    break;
  case None:
    M.addModuleFlag(Module::Error, POPCORN_INST_KEY, None);
    break;
  default: llvm_unreachable("Unknown instrumentation type"); break;
  }
}

/// Get the type of instrumentation.
static inline enum InstrumentType getInstrumentationType(Module &M) {
  Metadata *MD = M.getModuleFlag(POPCORN_INST_KEY);
  if(MD) {
    ConstantAsMetadata *Val = cast<ConstantAsMetadata>(MD);
    ConstantInt *IntVal = cast<ConstantInt>(Val->getValue());
    uint64_t RawVal = IntVal->getZExtValue();
    assert(RawVal < NumVals && "Invalid instrumentation type");
    return (enum InstrumentType)RawVal;
  }
  else return None;
}

}
}

#endif

