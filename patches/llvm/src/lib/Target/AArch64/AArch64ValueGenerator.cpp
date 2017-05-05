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

#define DEBUG_TYPE "stacktransform"

using namespace llvm;

// Make types in MachineGeneratedVal more accessible
typedef MachineGeneratedVal::ValueGenInst ValueGenInst;
typedef MachineGeneratedVal::ValueGenInst::InstType InstType;
typedef MachineGeneratedVal::ValueGenInstPtr ValueGenInstPtr;
typedef MachineGeneratedVal::ValueGenInstList ValueGenInstList;

template <InstType T>
using RegInstruction = MachineGeneratedVal::RegInstruction<T>;
template <InstType T>
using ImmInstruction = MachineGeneratedVal::ImmInstruction<T>;
template <InstType T>
using PseudoInstruction = MachineGeneratedVal::PseudoInstruction<T>;

void
AArch64ValueGenerator::genADDInstructions(const MachineInstr *MI,
                                          ValueGenInstList &IL) const {
  int Index;

  switch(MI->getOpcode()) {
  case AArch64::ADDXri:
    if(MI->getOperand(1).isFI()) {
      Index = MI->getOperand(1).getIndex();
      IL.push_back(ValueGenInstPtr(
        new PseudoInstruction<InstType::StackSlot>(Index, InstType::Set)));
      assert(MI->getOperand(2).isImm() && MI->getOperand(2).getImm() == 0);
      assert(MI->getOperand(3).isImm() && MI->getOperand(3).getImm() == 0);
    }
    break;
  default:
    llvm_unreachable("Unhandled ADD machine instruction");
    break;
  }
}

MachineLiveValPtr
AArch64ValueGenerator::getMachineValue(const MachineInstr *MI) const {
  MachineLiveVal* Val = nullptr;
  const MachineOperand *MO;
  const TargetInstrInfo *TII;
  ValueGenInstList IL;

  switch(MI->getOpcode()) {
  case AArch64::MOVaddr:
  case AArch64::ADRP:
    MO = &MI->getOperand(1);
    assert((MO->isGlobal() || MO->isSymbol() || MO->isMCSymbol()) &&
           "Invalid operand for address generation");
    if(MO->isGlobal())
      Val = new MachineReference(MO->getGlobal()->getName(), MI);
    else if(MO->isSymbol())
      Val = new MachineReference(MO->getSymbolName(), MI);
    else if(MO->isMCSymbol())
      Val = new MachineReference(MO->getMCSymbol()->getName(), MI);
    break;
  case AArch64::ADDXri:
    genADDInstructions(MI, IL);
    if(IL.size() > 0) Val = new MachineGeneratedVal(IL, MI);
    break;
  default:
    TII =  MI->getParent()->getParent()->getSubtarget().getInstrInfo();
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }

  return MachineLiveValPtr(Val);
}

