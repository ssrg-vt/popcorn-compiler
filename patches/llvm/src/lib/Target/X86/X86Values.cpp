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
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

#define DEBUG_TYPE "stacktransform"

using namespace llvm;

typedef ValueGenInst::InstType InstType;
template <InstType T> using RegInstruction = RegInstruction<T>;
template <InstType T> using ImmInstruction = ImmInstruction<T>;

MachineLiveVal *X86Values::genLEAInstructions(const MachineInstr *MI) const {
  unsigned Reg, Size;
  int64_t Imm;
  ValueGenInstList IL;

  // TODO do we need to handle the segment register operand?
  switch(MI->getOpcode()) {
  case X86::LEA64r:
    Size = 8;

    // Initialize to index register * scale if indexing, or zero otherwise
    Reg = MI->getOperand(1 + X86::AddrIndexReg).getReg();
    if(Reg) {
      Imm = MI->getOperand(1 + X86::AddrScaleAmt).getImm();
      IL.emplace_back(new RegInstruction<InstType::Set>(Reg));
      IL.emplace_back(new ImmInstruction<InstType::Multiply>(Size, Imm));
    }
    else IL.emplace_back(new ImmInstruction<InstType::Set>(8, 0));

    // Add the base register & displacement
    if(!MI->getOperand(1 + X86::AddrBaseReg).isFI()) {
      assert(MI->getOperand(1 + X86::AddrBaseReg).isReg() &&
             MI->getOperand(1 + X86::AddrDisp).isImm());

      Reg = MI->getOperand(1 + X86::AddrBaseReg).getReg();
      Imm = MI->getOperand(1 + X86::AddrDisp).getImm();
      IL.emplace_back(new RegInstruction<InstType::Add>(Reg));
      IL.emplace_back(new ImmInstruction<InstType::Add>(Size, Imm));
      return new MachineGeneratedVal(IL, MI, true);
    }
    // TODO what if we're referencing a frame? The frame index becomes the base
    // register + displacement after register rewriting & stack slot allocation
    //Index = MI->getOperand(1 + X86::AddrBaseReg).getIndex();

    break;
  default:
    DEBUG(dbgs() << "Unhandled LEA machine instruction");
    break;
  }
  return nullptr;
}

MachineLiveValPtr X86Values::getMachineValue(const MachineInstr *MI) const {
  MachineLiveVal* Val = nullptr;
  const MachineOperand *MO;
  const TargetInstrInfo *TII;

  switch(MI->getOpcode()) {
  case X86::LEA64r:
    Val = genLEAInstructions(MI);
    break;
  case X86::MOV64ri:
    MO = &MI->getOperand(1);
    if(MO->isGlobal() || MO->isSymbol() || MO->isMCSymbol())
      Val = new MachineSymbolRef(*MO, MI, true);
    break;
  default:
    TII =  MI->getParent()->getParent()->getSubtarget().getInstrInfo();
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }

  return MachineLiveValPtr(Val);
}

