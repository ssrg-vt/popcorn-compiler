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

/// Return whether the machine operand is a specific immediate value.
static bool isImmOp(const MachineOperand &MO, int64_t Imm) {
  if(!MO.isImm()) return false;
  else if(MO.getImm() != Imm) return false;
  else return true;
}

/// Return whether the machine operand is a specific register.
static bool isRegOp(const MachineOperand &MO, unsigned Reg) {
  if(!MO.isReg()) return false;
  else if(MO.getReg() != Reg) return false;
  else return true;
}

/// Return whether the machine operand is the sentinal %noreg register.
static bool isNoRegOp(const MachineOperand &MO) { return isRegOp(MO, 0); }

MachineLiveVal *X86Values::genLEAInstructions(const MachineInstr *MI) const {
  unsigned Reg, Size;
  int64_t Imm;
  ValueGenInstList IL;

  // TODO do we need to handle the segment register operand?
  switch(MI->getOpcode()) {
  case X86::LEA64r:
    Size = 8;

    if(MI->getOperand(1 + X86::AddrBaseReg).isFI()) {
      // Stack slot address
      if(!isImmOp(MI->getOperand(1 + X86::AddrScaleAmt), 1)) {
        DEBUG(dbgs() << "Unhandled scale amount for frame index\n");
        break;
      }

      if(!isNoRegOp(MI->getOperand(1 + X86::AddrIndexReg))) {
        DEBUG(dbgs() <<  "Unhandled index register for frame index\n");
        break;
      }

      if(!isImmOp(MI->getOperand(1 + X86::AddrDisp), 0)) {
        DEBUG(dbgs() << "Unhandled index register for frame index\n");
        break;
      }

      return new
        MachineStackObject(MI->getOperand(1 + X86::AddrBaseReg).getIndex(),
                           false, MI, true);
    }
    else if(isRegOp(MI->getOperand(1 + X86::AddrBaseReg), X86::RIP)) {
      // PC-relative symbol address
      if(!isImmOp(MI->getOperand(1 + X86::AddrScaleAmt), 1)) {
        DEBUG(dbgs() << "Unhandled scale amount for PC-relative address\n");
        break;
      }

      if(!isNoRegOp(MI->getOperand(1 + X86::AddrIndexReg))) {
        DEBUG(dbgs() << "Unhandled index register for PC-relative address\n");
        break;
      }

      return new
        MachineSymbolRef(MI->getOperand(1 + X86::AddrDisp), false, MI);
    }
    else {
      // Raw form of LEA
      if(!MI->getOperand(1 + X86::AddrBaseReg).isReg() ||
         !MI->getOperand(1 + X86::AddrDisp).isImm()) {
        DEBUG(dbgs() << "Unhandled base register/displacement operands\n");
        break;
      }

      // Initialize to index register * scale if indexing, or zero otherwise
      Reg = MI->getOperand(1 + X86::AddrIndexReg).getReg();
      if(Reg) {
        Imm = MI->getOperand(1 + X86::AddrScaleAmt).getImm();
        IL.emplace_back(new RegInstruction<InstType::Set>(Reg));
        IL.emplace_back(new ImmInstruction<InstType::Multiply>(Size, Imm));
      }
      else IL.emplace_back(new ImmInstruction<InstType::Set>(Size, 0));

      // Add the base register & displacement
      Reg = MI->getOperand(1 + X86::AddrBaseReg).getReg();
      Imm = MI->getOperand(1 + X86::AddrDisp).getImm();
      IL.emplace_back(new RegInstruction<InstType::Add>(Reg));
      IL.emplace_back(new ImmInstruction<InstType::Add>(Size, Imm));
      return new MachineGeneratedVal(IL, MI, true);
    }

    break;
  default:
    DEBUG(dbgs() << "Unhandled LEA machine instruction");
    break;
  }
  return nullptr;
}

MachineLiveValPtr X86Values::getMachineValue(const MachineInstr *MI) const {
  MachineLiveVal* Val = nullptr;
  const MachineOperand *MO, *MO2;
  const TargetInstrInfo *TII;

  switch(MI->getOpcode()) {
  case X86::LEA64r:
    Val = genLEAInstructions(MI);
    break;
  case X86::MOV32r0:
    Val = new MachineImmediate(4, 0, MI, false);
    break;
  case X86::MOV32ri:
    MO = &MI->getOperand(1);
    if(MO->isImm()) Val = new MachineImmediate(4, MO->getImm(), MI, false);
    break;
  case X86::MOV32ri64:
    // TODO the upper 32 bits of this reference are supposed to be masked
    MO = &MI->getOperand(1);
    if(TargetValues::isSymbolValue(MO))
      Val = new MachineSymbolRef(*MO, false, MI);
    break;
  case X86::MOV64ri:
    MO = &MI->getOperand(1);
    if(MO->isImm()) Val = new MachineImmediate(8, MO->getImm(), MI, false);
    else if(TargetValues::isSymbolValue(MO))
      Val = new MachineSymbolRef(*MO, false, MI);
    break;
  case X86::MOV64rm:
    MO = &MI->getOperand(1 + X86::AddrBaseReg);
    MO2 = &MI->getOperand(1 + X86::AddrDisp);
    // Note: codegen'd a PC relative symbol reference
    // Note 2: we *must* ensure the symbol is const-qualified, otherwise we
    // risk creating a new value if the symbol's value changes between when the
    // initial load would have occurred and the transformation, e.g.,
    //
    //   movq <ga:mysym>, %rax
    //   ... (somebody changes mysym's value) ...
    //   callq <ga:myfunc>
    //
    // In this situation, the transformation occurs at the call site and
    // retrieves the updated value rather than the value that would have been
    // loaded at the ldr instruction.
    if(MO->isReg() && MO->getReg() == X86::RIP &&
       TargetValues::isSymbolValue(MO2) &&
       TargetValues::isSymbolValueConstant(MO2))
        Val = new MachineSymbolRef(*MO2, true, MI);
    break;
  default:
    TII =  MI->getParent()->getParent()->getSubtarget().getInstrInfo();
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }

  return MachineLiveValPtr(Val);
}

