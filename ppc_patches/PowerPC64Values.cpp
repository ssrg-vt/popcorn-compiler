//===- PowerPC64TargetValues.cpp - PowerPC64 specific value generator -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PowerPC64Values.h"
#include "PPC.h"
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

void PowerPC64Values::genADDInstructions(const MachineInstr *MI,
                                       ValueGenInstList &IL) const {
  int Index;

  /*switch(MI->getOpcode()) {
  case PowerPC64::ADDXri:
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
  }*/
}

void PowerPC64Values::genBitfieldInstructions(const MachineInstr *MI,
                                            ValueGenInstList &IL) const {
  int64_t R, S;
  unsigned Size = 8, Bits = 64;
  const uint64_t Mask = UINT64_MAX;

  /*switch(MI->getOpcode()) {
  case PowerPC64::UBFMXri:
    // TODO ensure this is correct
    assert(MI->getOperand(1).isReg() && MI->getOperand(2).isImm() &&
           MI->getOperand(3).isImm());
    IL.push_back(ValueGenInstPtr(
      new RegInstruction<InstType::Set>(MI->getOperand(1).getReg())));
    R = MI->getOperand(2).getImm();
    S = MI->getOperand(3).getImm();
    if(S >= R) {
      IL.push_back(ValueGenInstPtr(
        new ImmInstruction<InstType::RightShiftLog>(Size, R)));
      IL.push_back(ValueGenInstPtr(
        new ImmInstruction<InstType::Mask>(Size, ~(Mask << (S - R + 1)))));
    }
    else {
      IL.push_back(ValueGenInstPtr(
        new ImmInstruction<InstType::Mask>(Size, ~(Mask << (S + 1)))));
      IL.push_back(ValueGenInstPtr(
        new ImmInstruction<InstType::LeftShift>(Size, Bits - R)));
    }
    break;
  }*/
}

MachineLiveValPtr PowerPC64Values::getMachineValue(const MachineInstr *MI) const {
  MachineLiveVal* Val = nullptr;
  const MachineOperand *MO;
  const TargetInstrInfo *TII;
  ValueGenInstList IL;

  return nullptr;
 /* switch(MI->getOpcode()) {
  case PowerPC64::MOVaddr:
  case PowerPC64::ADRP:
    MO = &MI->getOperand(1);
    if(MO->isCPI()) {
      IL.push_back(ValueGenInstPtr(
        new PseudoInstruction<InstType::ConstantPool>(MO->getIndex(),
                                                      InstType::Set)));
      Val = new MachineGeneratedVal(IL, MI);
    }
    else {
      assert((MO->isGlobal() || MO->isSymbol() || MO->isMCSymbol()) &&
             "Invalid operand for address generation");
      if(MO->isGlobal())
        Val = new MachineReference(MO->getGlobal()->getName(), MI);
      else if(MO->isSymbol())
        Val = new MachineReference(MO->getSymbolName(), MI);
      else if(MO->isMCSymbol())
        Val = new MachineReference(MO->getMCSymbol()->getName(), MI);
    }
    break;
  case PowerPC64::ADDXri:
    genADDInstructions(MI, IL);
    if(IL.size()) Val = new MachineGeneratedVal(IL, MI);
    break;
  case PowerPC64::UBFMXri:
    genBitfieldInstructions(MI, IL);
    if(IL.size()) Val = new MachineGeneratedVal(IL, MI);
    break;
  default:
    TII =  MI->getParent()->getParent()->getSubtarget().getInstrInfo();
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }

  return MachineLiveValPtr(Val);*/
}

