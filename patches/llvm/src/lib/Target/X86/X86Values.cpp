//===--------- X86TargetValues.cpp - X86 specific value generator ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "X86Values.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/Debug.h"
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

void X86Values::genLEAInstructions(const MachineInstr *MI,
                                   ValueGenInstList &IL) const {
  int Index;
  unsigned Reg;
  int64_t Imm;
  unsigned Size = 8; // in bytes

  // TODO do we need to handle the segment register operand?
  switch(MI->getOpcode()) {
  case X86::LEA64r:
    // Set the index register & scale (if we're doing indexing)
    Reg = MI->getOperand(1 + X86::AddrIndexReg).getReg();
    if(Reg) {
      IL.push_back(ValueGenInstPtr(
        new RegInstruction<InstType::Set>(Reg, 0)));

      Imm = MI->getOperand(1 + X86::AddrScaleAmt).getImm();
      IL.push_back(ValueGenInstPtr(
        new ImmInstruction<InstType::Multiply>(Size, Imm)));
    }

    if(MI->getOperand(1 + X86::AddrBaseReg).isFI()) {
      // The frame index becomes the base register + displacement after virtual
      // register rewriting and stack slot allocation
      Index = MI->getOperand(1 + X86::AddrBaseReg).getIndex();
      IL.push_back(ValueGenInstPtr(
        new PseudoInstruction<InstType::StackSlot>(Index, InstType::Add)));
    }
    else {
      assert(MI->getOperand(1 + X86::AddrBaseReg).isReg());
      Reg = MI->getOperand(1 + X86::AddrBaseReg).getReg();
      IL.push_back(ValueGenInstPtr(
        new RegInstruction<InstType::Add>(Reg, 0)));

      Imm = MI->getOperand(1 + X86::AddrDisp).getImm();
      IL.push_back(ValueGenInstPtr(
        new ImmInstruction<InstType::Add>(Size, Imm)));
    }
    break;
  default:
    llvm_unreachable("Unhandled LEA machine instruction");
    break;
  }
}

MachineLiveValPtr X86Values::getMachineValue(const MachineInstr *MI) const {
  MachineLiveVal* Val = nullptr;
  const TargetInstrInfo *TII;
  ValueGenInstList IL;

  switch(MI->getOpcode()) {
  case X86::LEA64r:
    genLEAInstructions(MI, IL);
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

