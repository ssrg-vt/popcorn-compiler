//===--------- X86TargetValues.cpp - X86 specific value generator ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <set>
#include "X86Values.h"
#include "X86InstrInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

#define DEBUG_TYPE "stacktransform"

using namespace llvm;

static void
getArgRegs(const MachineInstr *MICall, std::vector<unsigned> &regs) {
  size_t opIt;
  unsigned Reg;

  // Find the start of the registers used to pass arguments
  for(opIt = 0; opIt < MICall->getNumOperands(); opIt++) {
    const MachineOperand &MO = MICall->getOperand(opIt);
    if(MO.isReg() && MO.getReg() == X86::RSP) break;
  }

  assert(opIt != MICall->getNumOperands() && "Could not pass argument regs");

  regs.clear();
  regs.reserve(14); // Max 6 integer/8 FP registers used to pass arguments
  for(opIt++; opIt < MICall->getNumOperands(); opIt++) {
    const MachineOperand &op = MICall->getOperand(opIt);
    if(op.isReg()) {
      Reg = op.getReg();
      switch(Reg) {
      case X86::RSP: return;
      // From the x86-64 ABI:
      //   "with variable arguments passes information about the number of
      //    vector registers used"
      // TODO because Popcorn currently doesn't support vector operations, we
      // don't handle this if the vararg function uses vector registers
      case X86::RAX: case X86::EAX: case X86::AX: case X86::AL: break;
      default: regs.push_back(Reg);
      }
    }
  }
}

static int64_t getArgSlots(const MachineInstr *MICall,
                           std::set<int64_t> &Offsets) {
  int RegArg = X86::AddrBaseReg, OffArg = X86::AddrDisp;
  const MachineBasicBlock *parent = MICall->getParent();
  Offsets.clear();

  MachineBasicBlock::const_reverse_iterator it(MICall);
  for(; it != parent->rend(); it++) {
    switch(it->getOpcode()) {
    case X86::ADJCALLSTACKDOWN32: case X86::ADJCALLSTACKDOWN64:
      assert(it->getOperand(0).isImm() && "Invalid frame marshaling?");
      return it->getOperand(0).getImm();
    case X86::MOV32mi: case X86::MOV32mr:
    case X86::MOV64mi32: case X86::MOV64mr:
    case X86::MOVSDmr:
      if(it->getOperand(RegArg).isReg() &&
         it->getOperand(RegArg).getReg() == X86::RSP) {
        assert(it->getOperand(OffArg).isImm() &&
               "Invalid argument marshaling?");
        Offsets.insert(it->getOperand(OffArg).getImm());
      }
      break;
    default: break;
    }
  }
  llvm_unreachable("Could not find frame space marshaling instructions");
}

void
X86Values::getMarshaledArguments(const CallInst *IRCall,
                                 const MachineInstr *MICall,
                                 std::vector<MachineLiveLocPtr> &Locs) const {
  size_t NOps = IRCall->getNumOperands() - 1, Size;
  int64_t MaxOffset;
  std::vector<unsigned> Regs;
  std::set<int64_t> Offsets;
  std::set<int64_t>::const_iterator CurOffset, NextOffset;

  Locs.clear();
  if(!NOps) return;

  getArgRegs(MICall, Regs);
  MaxOffset = getArgSlots(MICall, Offsets);
  Locs.reserve(NOps);

  DEBUG(dbgs() << "Found " << Regs.size() << " argument register(s) and "
               << Offsets.size() << " argument slot(s)\n");

  assert(Regs.size() + Offsets.size() == NOps &&
         "Could not find all argument locations");

  // Walk through arguments, adding location metadata.  The x86 backend is nice
  // to us and directly matches MI register operands to arguments.  Once we've
  // consumed those, the remaining arguments are on the stack.
  if(Offsets.size()) {
    CurOffset = Offsets.begin();
    NextOffset = Offsets.begin(); NextOffset++;
  }

  for(size_t i = 0; i < NOps; i++) {
    const Type *Ty = IRCall->getOperand(i)->getType();
    if(i < Regs.size()) {
      Locs.emplace_back(MachineLiveLocPtr(
        new MachineLiveReg(Regs[i], Ty->isPointerTy())));
    }
    else {
      if(NextOffset == Offsets.end()) Size = MaxOffset - *CurOffset;
      else Size = *NextOffset - *CurOffset;
      Locs.emplace_back(MachineLiveLocPtr(
        new MachineLiveStackAddr(*CurOffset, X86::RSP, Size,
                                 Ty->isPointerTy())));
      CurOffset++;
      NextOffset++;
    }
  }
}

static TemporaryValue *getTemporaryReference(const MachineInstr *MI,
                                             const VirtRegMap *VRM,
                                             unsigned Size) {
  TemporaryValue *Val = nullptr;
  if(MI->getOperand(0).isReg()) {
    // Instruction format:  LEA64  rd     rbase  scale# ridx   disp#  rseg
    // Stack slot reference:              <fi>   1      noreg  off    noreg
    // TODO check for noreg in ridx & rseg?
    if(MI->getOperand(1 + X86::AddrBaseReg).isFI() &&
       MI->getOperand(1 + X86::AddrScaleAmt).isImm() &&
       MI->getOperand(1 + X86::AddrScaleAmt).getImm() == 1) {
      assert(MI->getOperand(1 + X86::AddrDisp).isImm() && "Invalid encoding");
      Val = new TemporaryValue;
      Val->Type = TemporaryValue::StackSlotRef;
      Val->Size = Size;
      Val->Vreg = MI->getOperand(0).getReg();
      Val->StackSlot = MI->getOperand(1 + X86::AddrBaseReg).getIndex();
      Val->Offset = MI->getOperand(1 + X86::AddrDisp).getImm();
    }
  }
  return Val;
}

TemporaryValuePtr
X86Values::getTemporaryValue(const MachineInstr *MI,
                             const VirtRegMap *VRM) const {
  TemporaryValue *Val = nullptr;
  switch(MI->getOpcode()) {
  case X86::LEA64r: Val = getTemporaryReference(MI, VRM, 8); break;
  default: break;
  }
  return TemporaryValuePtr(Val);
}

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

