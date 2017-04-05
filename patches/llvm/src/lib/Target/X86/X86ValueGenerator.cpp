//===----- X86TargetValueGenerator.cpp - X86 specific value generator -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "X86ValueGenerator.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

using namespace llvm;
typedef MachineGeneratedVal::ValueGenInstList VGIL;

#define DEBUG_TYPE "stacktransform"

void
X86ValueGenerator::getValueGenInstr(const MachineInstr *MI,
                                    VGIL &IL) const {
  const TargetInstrInfo *TII =
    MI->getParent()->getParent()->getSubtarget().getInstrInfo();

  switch(MI->getOpcode()) {
  default:
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }
}

MachineLiveValPtr
X86ValueGenerator::getMachineValue(const MachineInstr *MI) const {
  MachineLiveVal* Val = nullptr;
  const TargetInstrInfo *TII;

  switch(MI->getOpcode()) {
  default:
    TII =  MI->getParent()->getParent()->getSubtarget().getInstrInfo();
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }

  return MachineLiveValPtr(Val);
}

