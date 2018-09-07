//===- AArch64TargetValues.cpp - AArch64 specific value generator -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "AArch64Values.h"
#include "AArch64.h"
#include "MCTargetDesc/AArch64AddressingModes.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/IR/Constants.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

#define DEBUG_TYPE "stacktransform"

using namespace llvm;

static TemporaryValue *getTemporaryReference(const MachineInstr *MI,
                                             const VirtRegMap *VRM) {
  TemporaryValue *Val = nullptr;
  if(MI->getOperand(0).isReg()) {
    // Instruction format:    ADDXri  xd    xn    imm#  lsl#
    // Stack slot reference:                <fi>  0     0
    if(MI->getOperand(1).isFI() &&
       MI->getOperand(2).isImm() && MI->getOperand(2).getImm() == 0 &&
       MI->getOperand(3).isImm() && MI->getOperand(3).getImm() == 0) {
      Val = new TemporaryValue;
      Val->Type = TemporaryValue::StackSlotRef;
      Val->Vreg = MI->getOperand(0).getReg();
      Val->StackSlot = MI->getOperand(1).getIndex();
      Val->Offset = 0;
    }
  }

  return Val;
}

TemporaryValuePtr
AArch64Values::getTemporaryValue(const MachineInstr *MI,
                                 const VirtRegMap *VRM) const {
  TemporaryValue *Val = nullptr;
  switch(MI->getOpcode()) {
  case AArch64::ADDXri: Val = getTemporaryReference(MI, VRM); break;
  default: break;
  }
  return TemporaryValuePtr(Val);
}

typedef ValueGenInst::InstType InstType;
template <InstType T> using RegInstruction = RegInstruction<T>;
template <InstType T> using ImmInstruction = ImmInstruction<T>;

// Bitwise-conversions between floats & ints
union IntFloat64 { double d; uint64_t i; };
union IntFloat32 { float f; uint64_t i; };

MachineLiveVal *
AArch64Values::genADDInstructions(const MachineInstr *MI) const {
  int Index;

  switch(MI->getOpcode()) {
  case AArch64::ADDXri:
    if(MI->getOperand(1).isFI()) {
      Index = MI->getOperand(1).getIndex();
      assert(MI->getOperand(2).isImm() && MI->getOperand(2).getImm() == 0);
      assert(MI->getOperand(3).isImm() && MI->getOperand(3).getImm() == 0);
      return new MachineStackObject(Index, false, MI, true);
    }
    break;
  default:
    DEBUG(dbgs() << "Unhandled ADD machine instruction");
    break;
  }
  return nullptr;
}

MachineLiveVal *
AArch64Values::genADRPInstructions(const MachineInstr *MI) const {
  ValueGenInstList IL;
  if(isSymbolValue(MI->getOperand(1))) {
    IL.emplace_back(new RefInstruction(MI->getOperand(1)));
    IL.emplace_back(new ImmInstruction<InstType::Mask>(8, ~0xfff));
    return new MachineGeneratedVal(IL, MI, false);
  }
  return nullptr;
}

MachineLiveVal *
AArch64Values::genBitfieldInstructions(const MachineInstr *MI) const {
  int64_t R, S;
  unsigned Size, Bits;
  uint64_t Mask;
  ValueGenInstList IL;

  switch(MI->getOpcode()) {
  case AArch64::UBFMXri:
    Size = 8;
    Bits = 64;
    Mask = UINT64_MAX;

    assert(MI->getOperand(1).isReg() &&
           MI->getOperand(2).isImm() &&
           MI->getOperand(3).isImm());

    // TODO ensure this is correct
    IL.emplace_back(
      new RegInstruction<InstType::Set>(MI->getOperand(1).getReg()));
    R = MI->getOperand(2).getImm();
    S = MI->getOperand(3).getImm();
    if(S >= R) {
      IL.emplace_back(new ImmInstruction<InstType::RightShiftLog>(Size, R));
      IL.emplace_back(
        new ImmInstruction<InstType::Mask>(Size, ~(Mask << (S - R + 1))));
    }
    else {
      IL.emplace_back(
        new ImmInstruction<InstType::Mask>(Size, ~(Mask << (S + 1))));
      IL.emplace_back(new ImmInstruction<InstType::LeftShift>(Size, Bits - R));
    }
    return new MachineGeneratedVal(IL, MI, false);
    break;
  default:
    DEBUG(dbgs() << "Unhandled bitfield instruction");
    break;
  }
  return nullptr;
}

MachineLiveVal *
AArch64Values::genLoadRegValue(const MachineInstr *MI) const {
  switch(MI->getOpcode()) {
  case AArch64::LDRDui:
    if(MI->getOperand(2).isCPI()) {
      int Idx = MI->getOperand(2).getIndex();
      const MachineFunction *MF = MI->getParent()->getParent();
      const MachineConstantPool *MCP = MF->getConstantPool();
      const std::vector<MachineConstantPoolEntry> &CP = MCP->getConstants();
      if(CP[Idx].isMachineConstantPoolEntry()) {
        // TODO unhandled for now
      }
      else {
        const Constant *Val = CP[Idx].Val.ConstVal;
        if(isa<ConstantFP>(Val)) {
          const ConstantFP *FPVal = cast<ConstantFP>(Val);
          const APFloat &Flt = FPVal->getValueAPF();
          switch(APFloat::getSizeInBits(Flt.getSemantics())) {
          case 32: {
            IntFloat32 I2F = { Flt.convertToFloat() };
            return new MachineImmediate(4, I2F.i, MI, false);
          }
          case 64: {
            IntFloat64 I2D = { Flt.convertToDouble() };
            return new MachineImmediate(8, I2D.i, MI, false);
          }
          default: break;
          }
        }
      }
    }
    break;
  case AArch64::LDRXui:
    // Note: if this is of the form %vreg, <ga:...>, then the compiler has
    // emitted multiple instructions in order to form the full address.  We,
    // however, don't have the instruction encoding limitations.
    // TODO verify this note above is true, maybe using MO::getTargetFlags?
    // Note 2: we *must* ensure the symbol is const-qualified, otherwise we
    // risk creating a new value if the symbol's value changes between when the
    // initial load would have occurred and the transformation, e.g.,
    //
    //   ldr x20, <ga:mysym>
    //   ... (somebody changes mysym's value) ...
    //   bl <ga:myfunc>
    //
    // In this situation, the transformation occurs at the call site and
    // retrieves the updated value rather than the value that would have been
    // loaded at the ldr instruction.
    if(TargetValues::isSymbolValue(MI->getOperand(2)) &&
       TargetValues::isSymbolValueConstant(MI->getOperand(2)))
      return new MachineSymbolRef(MI->getOperand(2), true, MI);
    break;
  default: break;
  }
  return nullptr;
}

MachineLiveValPtr AArch64Values::getMachineValue(const MachineInstr *MI) const {
  IntFloat64 Conv64;
  MachineLiveVal* Val = nullptr;
  const MachineOperand *MO;
  const TargetInstrInfo *TII;

  switch(MI->getOpcode()) {
  case AArch64::ADDXri:
    Val = genADDInstructions(MI);
    break;
  case AArch64::ADRP:
    Val = genADRPInstructions(MI);
    break;
  case AArch64::MOVaddr:
    MO = &MI->getOperand(1);
    if(MO->isCPI())
      Val = new MachineConstPoolRef(MO->getIndex(), MI);
    else if(TargetValues::isSymbolValue(MO))
      Val = new MachineSymbolRef(*MO, false, MI);
    break;
  case AArch64::COPY:
    MO = &MI->getOperand(1);
    if(MO->isReg() && MO->getReg() == AArch64::LR) Val = new ReturnAddress(MI);
    break;
  case AArch64::FMOVD0:
    Conv64.d = 0.0;
    Val = new MachineImmediate(8, Conv64.i, MI, false);
    break;
  case AArch64::FMOVDi:
    Conv64.d = (double)AArch64_AM::getFPImmFloat(MI->getOperand(1).getImm());
    Val = new MachineImmediate(8, Conv64.i, MI, false);
    break;
  case AArch64::LDRXui:
  case AArch64::LDRDui:
    Val = genLoadRegValue(MI);
    break;
  case AArch64::MOVi32imm:
    MO = &MI->getOperand(1);
    assert(MO->isImm() && "Invalid immediate for MOVi32imm");
    Val = new MachineImmediate(4, MO->getImm(), MI, false);
    break;
  case AArch64::MOVi64imm:
    MO = &MI->getOperand(1);
    assert(MO->isImm() && "Invalid immediate for MOVi64imm");
    Val = new MachineImmediate(8, MO->getImm(), MI, false);
    break;
  case AArch64::UBFMXri:
    Val = genBitfieldInstructions(MI);
    break;
  default:
    TII =  MI->getParent()->getParent()->getSubtarget().getInstrInfo();
    DEBUG(dbgs() << "Unhandled opcode: "
                 << TII->getName(MI->getOpcode()) << "\n");
    break;
  }

  return MachineLiveValPtr(Val);
}

