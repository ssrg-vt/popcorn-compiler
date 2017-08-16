//===-- llvm/Target/TargetValueGenerator.cpp - Value Generator --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/StackTransformTypes.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

#define DEBUG_TYPE "stacktransform"

using namespace llvm;

//===----------------------------------------------------------------------===//
// Types for generating more complex architecture-specific live values
//

const char *ValueGenInst::InstTypeStr[] = {
#define X(type) #type ,
  VALUE_GEN_INST
#undef X
};

const char *ValueGenInst::getInstName(enum InstType Type) {
  switch(Type) {
#define X(type) case type: 
    VALUE_GEN_INST
#undef X
    return InstTypeStr[Type];
    break;
  default:
    return "unknown";
  };
}

std::string ValueGenInst::getInstNameStr(enum InstType Type) {
  return std::string(getInstName(Type));
}

//===----------------------------------------------------------------------===//
// MachineSymbolRef implementation
//

bool MachineSymbolRef::operator==(const MachineLiveVal &RHS) const {
  if(RHS.isSymbolRef()) {
    const MachineSymbolRef &MSR = (const MachineSymbolRef &)RHS;
    if(&MSR.Symbol == &Symbol) return true;
  }
  return false;
}

std::string MachineSymbolRef::toString() const {
  std::string buf = "reference to symbol '";
  switch(Symbol.getType()) {
  case MachineOperand::MO_GlobalAddress:
    buf += Symbol.getGlobal()->getName();
    buf += "' (global)";
    break;
  case MachineOperand::MO_ExternalSymbol:
    buf += Symbol.getSymbolName();
    buf += "' (external)";
    break;
  case MachineOperand::MO_MCSymbol:
    buf += Symbol.getMCSymbol()->getName();
    buf += "' (MC symbol)"; break;
  default:
    DEBUG(dbgs() << "Unhandled reference type: ";
          Symbol.print(dbgs());
          dbgs() << "\n";);
    buf += "n/a' (unhandled type)";
    break;
  }
  return buf;
}

static MCSymbol *GetExternalSymbol(AsmPrinter &AP, StringRef Sym) {
  SmallString<60> Name;
  Mangler::getNameWithPrefix(Name, Sym, *AP.TM.getDataLayout());
  return AP.OutContext.lookupSymbol(Name);
}

MCSymbol *MachineSymbolRef::getReference(AsmPrinter &AP) const {

  switch(Symbol.getType()) {
  case MachineOperand::MO_ExternalSymbol:
    return GetExternalSymbol(AP, Symbol.getSymbolName());
  case MachineOperand::MO_GlobalAddress:
    return AP.TM.getSymbol(Symbol.getGlobal(), *AP.Mang);
  case MachineOperand::MO_MCSymbol:
    return Symbol.getMCSymbol();
  default:
    DEBUG(dbgs() << "Unhandled reference type: ";
          Symbol.print(dbgs());
          dbgs() << "\n";);
    return nullptr;
  }
}

//===----------------------------------------------------------------------===//
// MachineConstPoolRef implementation
//

bool MachineConstPoolRef::operator==(const MachineLiveVal &RHS) const {
  if(RHS.isConstPoolRef()) {
    const MachineConstPoolRef &MCPR = (const MachineConstPoolRef &)RHS;
    if(MCPR.Index == Index) return true;
  }
  return false;
}

MCSymbol *MachineConstPoolRef::getReference(AsmPrinter &AP) const {
  MCSymbol *Sym = AP.GetCPISymbol(Index);
  assert(Sym && "Could not get constant pool reference");
  return Sym;
}

//===----------------------------------------------------------------------===//
// MachineStackObject implementation
//

bool MachineStackObject::operator==(const MachineLiveVal &RHS) const {
  if(RHS.isStackObject()) {
    const MachineStackObject &MSO = (const MachineStackObject &)RHS;
    if(MSO.Index == Index) return true;
  }
  return false;
}

std::string MachineStackObject::toString() const {
  std::string buf;
  if(Load) buf = "load from ";
  else buf = "reference to ";
  return buf + "stack slot " + std::to_string(Index);
}

int
MachineStackObject::getOffsetFromReg(AsmPrinter &AP, unsigned &BR) const {
  const TargetFrameLowering *TFL = AP.MF->getSubtarget().getFrameLowering();
  return TFL->getFrameIndexReference(*AP.MF, Index, BR);
}

//===----------------------------------------------------------------------===//
// ReturnAddress implementation
//

int ReturnAddress::getOffsetFromReg(AsmPrinter &AP, unsigned &BR) const {
  int Off = AP.MF->getSubtarget().getRegisterInfo()->getReturnAddrLoc(*AP.MF,
                                                                      BR);
  if(BR == 0) llvm_unreachable("No saved return address!");
  return Off;
}

//===----------------------------------------------------------------------===//
// MachineImmediate implementation
//

MachineImmediate::MachineImmediate(unsigned Size,
                                   uint64_t Value,
                                   const MachineInstr *DefMI,
                                   bool Ptr)
  : MachineLiveVal(DefMI, Ptr), Size(Size), Value(Value)
{
  if(Size > 8)
    llvm_unreachable("Unsupported immediate value size of > 8 bytes");
}

bool MachineImmediate::operator==(const MachineLiveVal &RHS) const {
  if(RHS.isImm()) {
    const MachineImmediate &MI = (const MachineImmediate &)RHS;
    if(MI.Size == Size && MI.Value == Value) return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// MachineGeneratedVal implementation
//

bool MachineGeneratedVal::operator==(const MachineLiveVal &RHS) const {
  if(!RHS.isGenerated()) return false;
  const MachineGeneratedVal &MGV = (const MachineGeneratedVal &)RHS;

  if(VG.size() != MGV.VG.size()) return false;
  for(size_t i = 0, num = VG.size(); i < num; i++)
    if(VG[i] != MGV.VG[i]) return false;
  return true;
}

//===----------------------------------------------------------------------===//
// MachineLiveReg implementation
//

bool MachineLiveReg::operator==(const MachineLiveLoc &RHS) const {
  if(RHS.isReg()) {
    const MachineLiveReg &MLR = (const MachineLiveReg &)RHS;
    if(MLR.Reg == Reg) return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// MachineLiveStackAddr implementation
//

bool MachineLiveStackAddr::operator==(const MachineLiveLoc &RHS) const {
  if(RHS.isStackAddr() && !RHS.isStackSlot()) {
    const MachineLiveStackAddr &MLSA = (const MachineLiveStackAddr &)RHS;
    if(Offset != INT32_MAX && MLSA.Offset != INT32_MAX &&
       Offset == MLSA.Offset && Reg == MLSA.Reg && Size == MLSA.Size)
      return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// MachineLiveStackSlot implementation
//

bool MachineLiveStackSlot::operator==(const MachineLiveLoc &RHS) const {
  if(RHS.isStackSlot()) {
    const MachineLiveStackSlot &MLSS = (const MachineLiveStackSlot &)RHS;
    if(MLSS.Index == Index) return true;
  }
  return false;
}

int MachineLiveStackSlot::calcAndGetRegOffset(const AsmPrinter &AP, unsigned &BP) {
  if(Offset == INT32_MAX) {
    const TargetFrameLowering *TFL = AP.MF->getSubtarget().getFrameLowering();
    Offset = TFL->getFrameIndexReference(*AP.MF, Index, Reg);
  }
  BP = Reg;
  return Offset;
}

unsigned MachineLiveStackSlot::getSize(const AsmPrinter &AP) {
  if(Size == 0) {
    const MachineFrameInfo *MFI = AP.MF->getFrameInfo();
    Size = MFI->getObjectSize(Index);
  }
  return Size;
}

