//===- AArch64TargetValueGenerator.cpp - AArch64 specific value generator -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "AArch64ValueGenerator.h"
#include "AArch64.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

using namespace llvm;
typedef MachineGeneratedVal::ValueGenInstList VGIL;

#define DEBUG_TYPE "stacktransform"

void
AArch64ValueGenerator::getValueGenInstr(const MachineInstr *MI,
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
AArch64ValueGenerator::getMachineValue(const MachineInstr *MI) const {
  MachineLiveVal* Val = nullptr;
  const MachineOperand *MO;
  const TargetInstrInfo *TII;

  switch(MI->getOpcode()) {
  case AArch64::MOVaddr:
    MO = &MI->getOperand(1);
    assert((MO->isGlobal() || MO->isSymbol() || MO->isMCSymbol()) &&
           "Invalid operand for MOVaddr");
    if(MO->isGlobal())
      Val = new MachineReference(MO->getGlobal()->getName(), MI);
    else if(MO->isSymbol())
      Val = new MachineReference(MO->getSymbolName(), MI);
    else if(MO->isMCSymbol())
      Val = new MachineReference(MO->getMCSymbol()->getName(), MI);
    break;
  default:
    TII =  MI->getParent()->getParent()->getSubtarget().getInstrInfo();
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }

  return MachineLiveValPtr(Val);
}

