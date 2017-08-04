//===---------------------------- StackMaps.cpp ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/UnwindInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOpcodes.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include <iterator>

using namespace llvm;

#define DEBUG_TYPE "stackmaps"

#define TYPE_AND_FLAGS(type, ptr, alloca, dup) \
  ((uint8_t)type) << 4 | ((uint8_t)ptr) << 2 | \
  ((uint8_t)alloca) << 1 | ((uint8_t)dup)

#define ARCH_TYPE_AND_FLAGS(type, ptr) ((uint8_t)type) << 4 | ((uint8_t)ptr)

#define ARCH_OP_TYPE(inst, op) ((uint8_t)inst << 4) | ((uint8_t)op)

static cl::opt<int> StackMapVersion(
    "stackmap-version", cl::init(1),
    cl::desc("Specify the stackmap encoding version (default = 1)"));

const char *StackMaps::WSMP = "Stack Maps: ";

PatchPointOpers::PatchPointOpers(const MachineInstr *MI)
    : MI(MI), HasDef(MI->getOperand(0).isReg() && MI->getOperand(0).isDef() &&
                     !MI->getOperand(0).isImplicit()),
      IsAnyReg(MI->getOperand(getMetaIdx(CCPos)).getImm() ==
               CallingConv::AnyReg) {
#ifndef NDEBUG
  unsigned CheckStartIdx = 0, e = MI->getNumOperands();
  while (CheckStartIdx < e && MI->getOperand(CheckStartIdx).isReg() &&
         MI->getOperand(CheckStartIdx).isDef() &&
         !MI->getOperand(CheckStartIdx).isImplicit())
    ++CheckStartIdx;

  assert(getMetaIdx() == CheckStartIdx &&
         "Unexpected additional definition in Patchpoint intrinsic.");
#endif
}

unsigned PatchPointOpers::getNextScratchIdx(unsigned StartIdx) const {
  if (!StartIdx)
    StartIdx = getVarIdx();

  // Find the next scratch register (implicit def and early clobber)
  unsigned ScratchIdx = StartIdx, e = MI->getNumOperands();
  while (ScratchIdx < e &&
         !(MI->getOperand(ScratchIdx).isReg() &&
           MI->getOperand(ScratchIdx).isDef() &&
           MI->getOperand(ScratchIdx).isImplicit() &&
           MI->getOperand(ScratchIdx).isEarlyClobber()))
    ++ScratchIdx;

  assert(ScratchIdx != e && "No scratch register available");
  return ScratchIdx;
}

StackMaps::StackMaps(AsmPrinter &AP) : AP(AP) {
  if (StackMapVersion != 1)
    llvm_unreachable("Unsupported stackmap version!");
}

/// Go up the super-register chain until we hit a valid dwarf register number.
static unsigned getDwarfRegNum(unsigned Reg, const TargetRegisterInfo *TRI) {
  int RegNum = TRI->getDwarfRegNum(Reg, false);
  for (MCSuperRegIterator SR(Reg, TRI); SR.isValid() && RegNum < 0; ++SR)
    RegNum = TRI->getDwarfRegNum(*SR, false);

  assert(RegNum >= 0 && "Invalid Dwarf register number.");
  return (unsigned)RegNum;
}

/// Get pointer typing information for a stackmap operand
void StackMaps::getPointerInfo(const Value *Op, const DataLayout &DL,
                               bool &isPtr, bool &isAlloca,
                               unsigned &AllocaSize) const {
  isPtr = false;
  isAlloca = false;
  AllocaSize = 0;

  assert(Op != nullptr && "Invalid stackmap operand");
  Type *Ty = Op->getType();
  if(Ty->isPointerTy())
  {
    isPtr = true;
    PointerType *PTy = cast<PointerType>(Ty);
    if(PTy->getElementType()->isSized() && isa<AllocaInst>(Op)) {
      isAlloca = true;
      AllocaSize = DL.getTypeAllocSize(PTy->getElementType());
    }
  }
}

/// Get stackmap information for register location
void StackMaps::getRegLocation(unsigned Phys,
                               unsigned &Dwarf,
                               unsigned &Offset) const {
  const TargetRegisterInfo *TRI = AP.MF->getSubtarget().getRegisterInfo();
  assert(!TRI->isVirtualRegister(Phys) &&
         "Virtual registers should have been rewritten by now");
  Offset = 0;
  Dwarf = getDwarfRegNum(Phys, TRI);
  unsigned LLVMRegNum = TRI->getLLVMRegNum(Dwarf, false);
  unsigned SubRegIdx = TRI->getSubRegIndex(LLVMRegNum, Phys);
  if(SubRegIdx)
    Offset = TRI->getSubRegIdxOffset(SubRegIdx);
}

/// Add duplicate target-specific locations for a stackmap operand
void StackMaps::addDuplicateLocs(const CallInst *StackMap, const Value *Oper,
                                 LocationVec &Locs, unsigned Size, bool Ptr,
                                 bool Alloca, unsigned AllocaSize) const {
  if(AP.MF->hasSMOpLocations(StackMap, Oper)) {
    const TargetRegisterInfo *TRI = AP.MF->getSubtarget().getRegisterInfo();
    const MachineLiveLocs &Dups = AP.MF->getSMOpLocations(StackMap, Oper);
    const unsigned FBPOff = AP.getFBPOffset();

    for(const MachineLiveLocPtr &LL : Dups) {
      if(LL->isReg()) {
        const MachineLiveReg &MR = (const MachineLiveReg &)*LL;
        unsigned Offset, DwarfRegNum;
        getRegLocation(MR.getReg(), DwarfRegNum, Offset);

        Locs.emplace_back(Location::Register, Size, DwarfRegNum, Offset,
                          Ptr, Alloca, true, AllocaSize);
      }
      else if(LL->isStackSlot()) {
        const MachineLiveStackSlot &MSS = (const MachineLiveStackSlot &)*LL;
        const MachineFrameInfo *MFI = AP.MF->getFrameInfo();
        assert(!MFI->isDeadObjectIndex(MSS.getStackSlot()) &&
               "Attempting to add a dead stack slot");
        int64_t Offset = MFI->getObjectOffset(MSS.getStackSlot()) + FBPOff;

        Locs.emplace_back(Location::Indirect, Size,
          getDwarfRegNum(TRI->getFrameRegister(*AP.MF), TRI),
          Offset, Ptr, Alloca, true, AllocaSize);
      }
    }
  }
}

MachineInstr::const_mop_iterator
StackMaps::parseOperand(MachineInstr::const_mop_iterator MOI,
                        MachineInstr::const_mop_iterator MOE, LocationVec &Locs,
                        LiveOutVec &LiveOuts, User::const_op_iterator &Op) const {
  bool isPtr, isAlloca;
  unsigned AllocaSize;
  auto &DL = AP.MF->getDataLayout();
  const TargetRegisterInfo *TRI = AP.MF->getSubtarget().getRegisterInfo();
  const CallInst *IRSM = cast<CallInst>(Op->getUser());
  const Value *IROp = Op->get();
  getPointerInfo(IROp, DL, isPtr, isAlloca, AllocaSize);

  if (MOI->isImm()) {
    switch (MOI->getImm()) {
    default:
      llvm_unreachable("Unrecognized operand type.");
    case StackMaps::DirectMemRefOp: {
      unsigned Size = DL.getPointerSizeInBits();
      assert((Size % 8) == 0 && "Need pointer size in bytes.");
      Size /= 8;
      unsigned Reg = (++MOI)->getReg();
      int64_t Imm = (++MOI)->getImm();
      Locs.emplace_back(Location::Direct, Size, getDwarfRegNum(Reg, TRI), Imm,
                        isPtr, isAlloca, false, AllocaSize);
      break;
    }
    case StackMaps::IndirectMemRefOp: {
      int64_t Size = (++MOI)->getImm();
      assert(Size > 0 && "Need a valid size for indirect memory locations.");
      Size = DL.getTypeAllocSize(IROp->getType());
      unsigned Reg = (++MOI)->getReg();
      int64_t Imm = (++MOI)->getImm();
      Locs.emplace_back(Location::Indirect, (unsigned)Size,
                        getDwarfRegNum(Reg, TRI), Imm, isPtr, isAlloca, false,
                        AllocaSize);
      break;
    }
    case StackMaps::ConstantOp: {
      ++MOI;
      assert(MOI->isImm() && "Expected constant operand.");
      int64_t Imm = MOI->getImm();
      Locs.emplace_back(Location::Constant, sizeof(int64_t), 0, Imm,
                        isPtr, isAlloca, false, AllocaSize);
      break;
    }
    }
    // Note: we shouldn't have alternate locations -- constants aren't stored
    // anywhere, and stack slots should be either allocas (which shouldn't have
    // alternate locations) or register spill locations (handled below in the
    // register path)
    assert(!AP.MF->hasSMOpLocations(IRSM, IROp) &&
           "Unhandled duplicate locations");
    ++Op;
    return ++MOI;
  }

  // The physical register number will ultimately be encoded as a DWARF regno.
  // The stack map also records the size of a spill slot that can hold the
  // register content, accurate to the actual size of the data type.
  if (MOI->isReg()) {
    // Skip implicit registers (this includes our scratch registers)
    if (MOI->isImplicit())
      return ++MOI;

    assert(TargetRegisterInfo::isPhysicalRegister(MOI->getReg()) &&
           "Virtreg operands should have been rewritten before now.");
    assert(!MOI->getSubReg() && "Physical subreg still around.");

    size_t ValSize = DL.getTypeAllocSize(IROp->getType());
    unsigned Offset, DwarfRegNum;
    getRegLocation(MOI->getReg(), DwarfRegNum, Offset);

    Locs.emplace_back(Location::Register, ValSize, DwarfRegNum, Offset,
                      isPtr, isAlloca, false, AllocaSize);
    addDuplicateLocs(IRSM, IROp, Locs, ValSize, isPtr, isAlloca, AllocaSize);
    ++Op;
    return ++MOI;
  }

  if (MOI->isRegLiveOut())
    LiveOuts = parseRegisterLiveOutMask(MOI->getRegLiveOut());

  return ++MOI;
}

void StackMaps::print(raw_ostream &OS) {
  const TargetRegisterInfo *TRI =
      AP.MF ? AP.MF->getSubtarget().getRegisterInfo() : nullptr;
  OS << WSMP << "callsites:\n";
  for (const auto &CSI : CSInfos) {
    const LocationVec &CSLocs = CSI.Locations;
    const LiveOutVec &LiveOuts = CSI.LiveOuts;
    const ArchValues &Values = CSI.Vals;

    OS << WSMP << "callsite " << CSI.ID << "\n";
    OS << WSMP << "  has " << CSLocs.size() << " locations\n";

    unsigned Idx = 0;
    for (const auto &Loc : CSLocs) {
      OS << WSMP << "\t\tLoc " << Idx << ": ";
      switch (Loc.Type) {
      case Location::Unprocessed:
        OS << "<Unprocessed operand>";
        break;
      case Location::Register:
        OS << "Register ";
        if (TRI)
          OS << TRI->getName(Loc.Reg);
        else
          OS << Loc.Reg;
        break;
      case Location::Direct:
        OS << "Direct ";
        if (TRI)
          OS << TRI->getName(Loc.Reg);
        else
          OS << Loc.Reg;
        if (Loc.Offset)
          OS << " + " << Loc.Offset;
        break;
      case Location::Indirect:
        OS << "Indirect ";
        if (TRI)
          OS << TRI->getName(Loc.Reg);
        else
          OS << Loc.Reg;
        OS << " + " << Loc.Offset;
        break;
      case Location::Constant:
        OS << "Constant " << Loc.Offset;
        break;
      case Location::ConstantIndex:
        OS << "Constant Index " << Loc.Offset;
        break;
      }
      OS << ", pointer? " << Loc.Ptr << ", alloca? " << Loc.Alloca
         << ", duplicate? " << Loc.Duplicate;

      unsigned TypeAndFlags =
        TYPE_AND_FLAGS(Loc.Type, Loc.Ptr, Loc.Alloca, Loc.Duplicate);

      OS << "\t[encoding: .byte " << TypeAndFlags << ", .byte " << Loc.Size
         << ", .short " << Loc.Reg << ", .int " << Loc.Offset
         << ", .uint " << Loc.AllocaSize << "]\n";
      Idx++;
    }

    OS << WSMP << "\thas " << LiveOuts.size() << " live-out registers\n";

    Idx = 0;
    for (const auto &LO : LiveOuts) {
      OS << WSMP << "\t\tLO " << Idx << ": ";
      if (TRI)
        OS << TRI->getName(LO.Reg);
      else
        OS << LO.Reg;
      OS << "\t[encoding: .short " << LO.DwarfRegNum << ", .byte 0, .byte "
         << LO.Size << "]\n";
      Idx++;
    }

    OS << WSMP << "\thas " << Values.size() << " arch-specific live values\n";

    Idx = 0;
    for (const auto &V : Values) {
      const Location &Loc = V.first;
      const Operation &Op = V.second;

      OS << WSMP << "\t\tArch-Val " << Idx << ": ";
      switch(Loc.Type) {
      case Location::Register:
        OS << "Register ";
        if (TRI)
          OS << TRI->getName(Loc.Reg);
        else
          OS << Loc.Reg;
        break;
      case Location::Indirect:
        OS << "Indirect ";
        if (TRI)
          OS << TRI->getName(Loc.Reg);
        else
          OS << Loc.Reg;
        if (Loc.Offset)
          OS << " + " << Loc.Offset;
        break;
      default:
        OS << "<Unknown live value type>";
        break;
      }

      OS << ", " << MachineGeneratedVal::ValueGenInst::InstTypeStr[Op.InstType]
         << " ";
      switch(Op.OperandType) {
      case Location::Register:
        OS << "register ";
        if (TRI)
          OS << TRI->getName(Op.DwarfReg);
        else
          OS << Op.DwarfReg;
        break;
      case Location::Direct:
        OS << "register ";
        if (TRI)
          OS << TRI->getName(Op.DwarfReg);
        else
          OS << Op.DwarfReg;
        if (Op.Constant)
          OS << " + " << Op.Constant;
        break;
      case Location::Constant:
        if(Op.isSymbol)
          OS << "address of " << Op.Symbol->getName();
        else {
          OS << "immediate ";
          OS.write_hex(Op.Constant);
        }
        break;
      default:
        OS << "<Unknown operand type>";
        break;
      }

      unsigned TypeAndFlags = ARCH_TYPE_AND_FLAGS(Loc.Type, Loc.Ptr);
      unsigned OpType = ARCH_OP_TYPE(Op.InstType, Op.OperandType);
      OS << "\t[encoding: .byte " << TypeAndFlags << ", .byte " << Loc.Size
         << ", .short " << Loc.Reg << ", .int " << Loc.Offset
         << ", .byte " << OpType << ", .byte " << Op.Size << ", .short "
         << Op.DwarfReg << ", .int64 " << (Op.isSymbol ? 0 : Op.Constant)
         << "]\n";
    }
  }
}

/// Create a live-out register record for the given register Reg.
StackMaps::LiveOutReg
StackMaps::createLiveOutReg(unsigned Reg, const TargetRegisterInfo *TRI) const {
  unsigned DwarfRegNum = getDwarfRegNum(Reg, TRI);
  unsigned Size = TRI->getMinimalPhysRegClass(Reg)->getSize();
  return LiveOutReg(Reg, DwarfRegNum, Size);
}

/// Parse the register live-out mask and return a vector of live-out registers
/// that need to be recorded in the stackmap.
StackMaps::LiveOutVec
StackMaps::parseRegisterLiveOutMask(const uint32_t *Mask) const {
  assert(Mask && "No register mask specified");
  const TargetRegisterInfo *TRI = AP.MF->getSubtarget().getRegisterInfo();
  LiveOutVec LiveOuts;

  // Create a LiveOutReg for each bit that is set in the register mask.
  for (unsigned Reg = 0, NumRegs = TRI->getNumRegs(); Reg != NumRegs; ++Reg)
    if ((Mask[Reg / 32] >> Reg % 32) & 1)
      LiveOuts.push_back(createLiveOutReg(Reg, TRI));

  // We don't need to keep track of a register if its super-register is already
  // in the list. Merge entries that refer to the same dwarf register and use
  // the maximum size that needs to be spilled.

  std::sort(LiveOuts.begin(), LiveOuts.end(),
            [](const LiveOutReg &LHS, const LiveOutReg &RHS) {
              // Only sort by the dwarf register number.
              return LHS.DwarfRegNum < RHS.DwarfRegNum;
            });

  for (auto I = LiveOuts.begin(), E = LiveOuts.end(); I != E; ++I) {
    for (auto II = std::next(I); II != E; ++II) {
      if (I->DwarfRegNum != II->DwarfRegNum) {
        // Skip all the now invalid entries.
        I = --II;
        break;
      }
      I->Size = std::max(I->Size, II->Size);
      if (TRI->isSuperRegister(I->Reg, II->Reg))
        I->Reg = II->Reg;
      II->Reg = 0; // mark for deletion.
    }
  }

  LiveOuts.erase(
      std::remove_if(LiveOuts.begin(), LiveOuts.end(),
                     [](const LiveOutReg &LO) { return LO.Reg == 0; }),
      LiveOuts.end());

  return LiveOuts;
}

/// Convert a list of instructions used to generate an architecture-specific
/// live value into multiple individual records.
void StackMaps::genArchValsFromInsts(ArchValues &AV,
                                     const Location &Loc,
                                     const MachineLiveVal &MLV) {
  assert(MLV.isGenerated() && "Invalid live value type");

  typedef MachineGeneratedVal::ValueGenInst::InstType InstType;
  typedef MachineGeneratedVal::ValueGenInst::OpType OpType;
  typedef MachineGeneratedVal::RegInstructionBase RegInstruction;
  typedef MachineGeneratedVal::ImmInstructionBase ImmInstruction;
  typedef MachineGeneratedVal::PseudoInstructionBase PseudoInstruction;

  const MachineGeneratedVal &MGV = (const MachineGeneratedVal &)MLV;
  const MachineGeneratedVal::ValueGenInstList &I = MGV.getInstructions();
  const TargetRegisterInfo *TRI = AP.MF->getSubtarget().getRegisterInfo();
  const MachineFrameInfo *MFI = AP.MF->getFrameInfo();
  const unsigned FBP = getDwarfRegNum(TRI->getFrameRegister(*AP.MF), TRI);
  const unsigned FBPOff = AP.getFBPOffset();
  Operation Op;

  for(auto &Inst : I) {
    const RegInstruction *RI;
    const ImmInstruction *II;
    const PseudoInstruction *PI;

    switch(Inst->type()) {
    case MachineGeneratedVal::ValueGenInst::StackSlot:
      PI = (const PseudoInstruction *)Inst.get();
      assert((PI->getGenType() == InstType::Add ||
              PI->getGenType() == InstType::Set) &&
             "Invalid frame object reference");

      Op.InstType = PI->getGenType();
      Op.OperandType = Location::Direct;
      Op.Size = AP.MF->getDataLayout().getPointerSizeInBits() / 8;
      Op.DwarfReg = FBP;
      Op.isSymbol = false;
      Op.Constant = MFI->getObjectOffset(PI->getData()) + FBPOff;
      break;
    case MachineGeneratedVal::ValueGenInst::ConstantPool:
      PI = (const PseudoInstruction *)Inst.get();
      assert(PI->getGenType() == InstType::Set &&
             "Invalid constant pool entry reference");

      Op.InstType = PI->getGenType();
      Op.OperandType = Location::Constant;
      Op.Size = AP.MF->getDataLayout().getPointerSizeInBits() / 8;
      Op.DwarfReg = 0;
      Op.isSymbol = true;
      Op.Symbol = AP.GetCPISymbol(PI->getData());
    default:
      Op.InstType = Inst->type();
      Op.isSymbol = false;
      switch(Inst->opType()) {
      case OpType::Register:
        RI = (const RegInstruction *)Inst.get();
        Op.OperandType = Location::Register;
        Op.Size = AP.MF->getDataLayout().getPointerSizeInBits() / 8;
        Op.DwarfReg = getDwarfRegNum(RI->getReg(), TRI);
        Op.Constant = 0;
        break;
      case OpType::Immediate:
        II = (const ImmInstruction *)Inst.get();
        Op.OperandType = Location::Constant;
        Op.Size = II->getImmSize();
        Op.DwarfReg = 0;
        Op.Constant = II->getImm();
        break;
      default: llvm_unreachable("Invalid operand type"); break;
      }
      break;
    }
    AV.emplace_back(ArchValue(Loc, Op));
  }
}

/// Add architecture-specific locations for the stackmap
void StackMaps::addArchLiveVals(const CallInst *SM, ArchValues &AV) {
  const TargetRegisterInfo *TRI = AP.MF->getSubtarget().getRegisterInfo();
  const MachineFrameInfo *MFI = AP.MF->getFrameInfo();

  if(AP.MF->hasSMArchSpecificLocations(SM)) {
    const ArchLiveValues &Vals = AP.MF->getSMArchSpecificLocations(SM);
    const unsigned FBPOff = AP.getFBPOffset();

    for(auto &Val : Vals) {
      Location Loc; Loc.Ptr = false;
      Operation Op;

      // Parse the location
      if(Val.first->isReg()) {
        const MachineLiveReg &MR = (const MachineLiveReg &)*Val.first;
        unsigned Offset, DwarfRegNum;
        const TargetRegisterClass *RC =
          TRI->getMinimalPhysRegClass(MR.getReg());
        getRegLocation(MR.getReg(), DwarfRegNum, Offset);

        Loc.Type = Location::Register;
        Loc.Size = RC->getSize();
        Loc.Reg = DwarfRegNum;
        Loc.Offset = Offset;
      }
      else if(Val.first->isStackSlot()) {
        const MachineLiveStackSlot &MSS =
          (const MachineLiveStackSlot &)*Val.first;
        int StackSlot = MSS.getStackSlot();

        Loc.Type = Location::Indirect;
        Loc.Size = MFI->getObjectSize(StackSlot);
        Loc.Reg = getDwarfRegNum(TRI->getFrameRegister(*AP.MF), TRI);
        Loc.Offset = MFI->getObjectOffset(StackSlot) + FBPOff;
      }
      else llvm_unreachable("Invalid architecture-specific live value");

      // Parse the operation
      if(Val.second->isImm()) {
        const MachineImmediate &MI = (const MachineImmediate &)*Val.second;
        Op.InstType = MachineGeneratedVal::ValueGenInst::Set;
        Op.OperandType = Location::Constant;
        Op.Size = MI.getSize();
        Op.DwarfReg = 0;
        Op.Constant = MI.getValue();
        AV.emplace_back(ArchValue(Loc, Op));
      }
      else if(Val.second->isReference()) {
        const MachineReference &MR = (const MachineReference &)*Val.second;
        Loc.Ptr = true;
        Op.InstType = MachineGeneratedVal::ValueGenInst::Set;
        Op.OperandType = Location::Constant;
        Op.Size = AP.MF->getDataLayout().getPointerSizeInBits() / 8;
        Op.DwarfReg = 0;
        Op.isSymbol = true;
        Op.Symbol = AP.OutContext.lookupSymbol(MR.getSymbol());
        AV.emplace_back(ArchValue(Loc, Op));
      }
      // TODO generated vals may point to allocas, should we also mark them as
      // pointers in order to do runtime checking?
      else if(Val.second->isGenerated())
        genArchValsFromInsts(AV, Loc, *Val.second);
      else llvm_unreachable("Invalid architecture-specific live value");
    }
  }
}

void StackMaps::recordStackMapOpers(const MachineInstr &MI, uint64_t ID,
                                    MachineInstr::const_mop_iterator MOI,
                                    MachineInstr::const_mop_iterator MOE,
                                    bool recordResult) {

  MCContext &OutContext = AP.OutStreamer->getContext();
  MCSymbol *MILabel = OutContext.createTempSymbol();
  AP.OutStreamer->EmitLabel(MILabel);
  User::const_op_iterator Op = nullptr;

  LocationVec Locations;
  LiveOutVec LiveOuts;
  ArchValues Constants;

  if (recordResult) {
    assert(PatchPointOpers(&MI).hasDef() && "Stackmap has no return value.");
    parseOperand(MI.operands_begin(), std::next(MI.operands_begin()), Locations,
                 LiveOuts, Op);
  }

  // Find the IR stackmap instruction which corresponds to MI so we can emit
  // type information along with the value's location
  const BasicBlock *BB = MI.getParent()->getBasicBlock();
  const IntrinsicInst *IRSM = nullptr;
  const std::string SMName("llvm.experimental.stackmap");
  for(auto BBI = BB->begin(), BBE = BB->end(); BBI != BBE; BBI++)
  {
    const IntrinsicInst *II;
    if((II = dyn_cast<IntrinsicInst>(&*BBI)) &&
       II->getCalledFunction()->getName() == SMName &&
       cast<ConstantInt>(II->getArgOperand(0))->getZExtValue() == ID)
    {
      IRSM = cast<IntrinsicInst>(&*BBI);
      break;
    }
  }
  assert(IRSM && "Could not find associated stackmap instruction");

  // Parse operands.
  Op = std::next(IRSM->op_begin(), 2);
  while (MOI != MOE) {
    MOI = parseOperand(MOI, MOE, Locations, LiveOuts, Op);
  }
  assert(Op == (IRSM->op_end() - 1) && "did not lower all stackmap operands");

  // Add architecture-specific live values
  addArchLiveVals(IRSM, Constants);

  // Move large constants into the constant pool.
  for (auto &Loc : Locations) {
    // Constants are encoded as sign-extended integers.
    // -1 is directly encoded as .long 0xFFFFFFFF with no constant pool.
    if (Loc.Type == Location::Constant && !isInt<32>(Loc.Offset)) {
      Loc.Type = Location::ConstantIndex;
      // ConstPool is intentionally a MapVector of 'uint64_t's (as
      // opposed to 'int64_t's).  We should never be in a situation
      // where we have to insert either the tombstone or the empty
      // keys into a map, and for a DenseMap<uint64_t, T> these are
      // (uint64_t)0 and (uint64_t)-1.  They can be and are
      // represented using 32 bit integers.
      assert((uint64_t)Loc.Offset != DenseMapInfo<uint64_t>::getEmptyKey() &&
             (uint64_t)Loc.Offset !=
                 DenseMapInfo<uint64_t>::getTombstoneKey() &&
             "empty and tombstone keys should fit in 32 bits!");
      auto Result = ConstPool.insert(std::make_pair(Loc.Offset, Loc.Offset));
      Loc.Offset = Result.first - ConstPool.begin();
    }
  }

  // Create an expression to calculate the offset of the callsite from function
  // entry.
  const MCExpr *CSOffsetExpr = MCBinaryExpr::createSub(
      MCSymbolRefExpr::create(MILabel, OutContext),
      MCSymbolRefExpr::create(AP.CurrentFnSymForSize, OutContext), OutContext);

  CSInfos.emplace_back(AP.CurrentFnSym, CSOffsetExpr, ID,
                       std::move(Locations), std::move(LiveOuts),
                       std::move(Constants));

  // Record the stack size of the current function.
  const MachineFrameInfo *MFI = AP.MF->getFrameInfo();
  const TargetRegisterInfo *RegInfo = AP.MF->getSubtarget().getRegisterInfo();
  bool HasDynamicFrameSize =
      MFI->hasVarSizedObjects() || RegInfo->needsStackRealignment(*(AP.MF));
  FnStackSize[AP.CurrentFnSym] =
      HasDynamicFrameSize ? UINT64_MAX : MFI->getStackSize();
}

void StackMaps::recordStackMap(const MachineInstr &MI) {
  assert(MI.getOpcode() == TargetOpcode::STACKMAP && "expected stackmap");

  int64_t ID = MI.getOperand(0).getImm();
  recordStackMapOpers(MI, ID, std::next(MI.operands_begin(), 2),
                      MI.operands_end());
}

void StackMaps::recordPatchPoint(const MachineInstr &MI) {
  assert(MI.getOpcode() == TargetOpcode::PATCHPOINT && "expected patchpoint");

  PatchPointOpers opers(&MI);
  int64_t ID = opers.getMetaOper(PatchPointOpers::IDPos).getImm();

  auto MOI = std::next(MI.operands_begin(), opers.getStackMapStartIdx());
  recordStackMapOpers(MI, ID, MOI, MI.operands_end(),
                      opers.isAnyReg() && opers.hasDef());

#ifndef NDEBUG
  // verify anyregcc
  auto &Locations = CSInfos.back().Locations;
  if (opers.isAnyReg()) {
    unsigned NArgs = opers.getMetaOper(PatchPointOpers::NArgPos).getImm();
    for (unsigned i = 0, e = (opers.hasDef() ? NArgs + 1 : NArgs); i != e; ++i)
      assert(Locations[i].Type == Location::Register &&
             "anyreg arg must be in reg.");
  }
#endif
}
void StackMaps::recordStatepoint(const MachineInstr &MI) {
  assert(MI.getOpcode() == TargetOpcode::STATEPOINT && "expected statepoint");

  StatepointOpers opers(&MI);
  // Record all the deopt and gc operands (they're contiguous and run from the
  // initial index to the end of the operand list)
  const unsigned StartIdx = opers.getVarIdx();
  recordStackMapOpers(MI, opers.getID(), MI.operands_begin() + StartIdx,
                      MI.operands_end(), false);
}

/// Emit the stackmap header.
///
/// Header {
///   uint8  : Stack Map Version (currently 1)
///   uint8  : Reserved (expected to be 0)
///   uint16 : Reserved (expected to be 0)
/// }
/// uint32 : NumFunctions
/// uint32 : NumConstants
/// uint32 : NumRecords
void StackMaps::emitStackmapHeader(MCStreamer &OS) {
  // Header.
  OS.EmitIntValue(StackMapVersion, 1); // Version.
  OS.EmitIntValue(0, 1);               // Reserved.
  OS.EmitIntValue(0, 2);               // Reserved.

  // Num functions.
  DEBUG(dbgs() << WSMP << "#functions = " << FnStackSize.size() << '\n');
  OS.EmitIntValue(FnStackSize.size(), 4);
  // Num constants.
  DEBUG(dbgs() << WSMP << "#constants = " << ConstPool.size() << '\n');
  OS.EmitIntValue(ConstPool.size(), 4);
  // Num callsites.
  DEBUG(dbgs() << WSMP << "#callsites = " << CSInfos.size() << '\n');
  OS.EmitIntValue(CSInfos.size(), 4);
}

/// Emit the function frame record for each function.
///
/// StkSizeRecord[NumFunctions] {
///   uint64 : Function Address
///   uint64 : Stack Size
///   uint32 : Number of Unwinding Entries
///   uint32 : Offset into Unwinding Section
/// }
void StackMaps::emitFunctionFrameRecords(MCStreamer &OS,
                                         const UnwindInfo *UI) {
  // Function Frame records.
  DEBUG(dbgs() << WSMP << "functions:\n");
  for (auto const &FR : FnStackSize) {
    DEBUG(dbgs() << WSMP << "function addr: " << FR.first
                 << " frame size: " << FR.second);
    OS.EmitSymbolValue(FR.first, 8);
    OS.EmitIntValue(FR.second, 8);

    if(UI) {
      const UnwindInfo::FuncUnwindInfo &FUI = UI->getUnwindInfo(FR.first);
      DEBUG(dbgs() << " unwind info start: " << FUI.SecOffset
                   << " (" << FUI.NumUnwindRecord << " entries)\n");
      OS.EmitIntValue(FUI.NumUnwindRecord, 4);
      OS.EmitIntValue(FUI.SecOffset, 4);
    }
    else OS.EmitIntValue(0, 8);
  }
}

/// Emit the constant pool.
///
/// int64  : Constants[NumConstants]
void StackMaps::emitConstantPoolEntries(MCStreamer &OS) {
  // Constant pool entries.
  DEBUG(dbgs() << WSMP << "constants:\n");
  for (const auto &ConstEntry : ConstPool) {
    DEBUG(dbgs() << WSMP << ConstEntry.second << '\n');
    OS.EmitIntValue(ConstEntry.second, 8);
  }
}

/// Emit the callsite info for each callsite.
///
/// StkMapRecord[NumRecords] {
///   uint64 : PatchPoint ID
///   uint32 : Index of Function Record
///   uint32 : Instruction Offset
///   uint16 : Reserved (record flags)
///   uint16 : NumLocations
///   Location[NumLocations] {
///     uint8 (4 bits) : Register | Direct | Indirect | Constant | ConstantIndex
///     uint8 (1 bit)  : Padding
///     uint8 (1 bit)  : Is it a pointer?
///     uint8 (1 bit)  : Is it an alloca?
///     uint8 (1 bit)  : Is it a duplicate record for the same live value?
///     uint8          : Size in Bytes
///     uint16         : Dwarf RegNum
///     int32          : Offset
///     uint32         : Size of pointed-to alloca data
///   }
///   uint16 : Padding
///   uint16 : NumLiveOuts
///   LiveOuts[NumLiveOuts] {
///     uint16 : Dwarf RegNum
///     uint8  : Reserved
///     uint8  : Size in Bytes
///   }
///   uint16 : Padding
///   uint16 : NumArchValues
///   ArchValues[NumArchValues] {
///     Location {
///       uint8 (4 bits) : Register | Indirect
///       uint8 (3 bits) : Padding
///       uint8 (1 bit)  : Is it a pointer?
///       uint8          : Size in Bytes
///       uint16         : Dwarf RegNum
///       int32          : Offset
///     }
///     Value {
///       uint8_t (4 bits) : Instruction
///       uint8_t (4 bits) : Register | Direct | Constant
///       uint8_t          : Size
///       uint16_t         : Dwarf RegNum
///       int64_t          : Offset or Constant
///     }
///   }
///   uint32 : Padding (only if required to align to 8 byte)
/// }
///
/// Location Encoding, Type, Value:
///   0x1, Register, Reg                 (value in register)
///   0x2, Direct, Reg + Offset          (frame index)
///   0x3, Indirect, [Reg + Offset]      (spilled value)
///   0x4, Constant, Offset              (small constant)
///   0x5, ConstIndex, Constants[Offset] (large constant)
void StackMaps::emitCallsiteEntries(MCStreamer &OS) {
  DEBUG(print(dbgs()));
  // Callsite entries.
  for (const auto &CSI : CSInfos) {
    const LocationVec &CSLocs = CSI.Locations;
    const LiveOutVec &LiveOuts = CSI.LiveOuts;
    const ArchValues &Values = CSI.Vals;

    // Verify stack map entry. It's better to communicate a problem to the
    // runtime than crash in case of in-process compilation. Currently, we do
    // simple overflow checks, but we may eventually communicate other
    // compilation errors this way.
    if (CSLocs.size() > UINT16_MAX || LiveOuts.size() > UINT16_MAX ||
        Values.size() > UINT16_MAX) {
      OS.EmitIntValue(UINT64_MAX, 8); // Invalid ID.
      OS.EmitIntValue(UINT32_MAX, 4); // Invalid index.
      OS.EmitValue(CSI.CSOffsetExpr, 4);
      OS.EmitIntValue(0, 2); // Reserved.
      OS.EmitIntValue(0, 2); // 0 locations.
      OS.EmitIntValue(0, 2); // padding.
      OS.EmitIntValue(0, 2); // 0 live-out registers.
      OS.EmitIntValue(0, 2); // padding.
      OS.EmitIntValue(0, 2); // 0 arch-specific values.
      OS.EmitIntValue(0, 4); // padding.
      continue;
    }

    OS.EmitIntValue(CSI.ID, 8);
    OS.EmitIntValue(FnStackSize.find(CSI.Func) - FnStackSize.begin(), 4);
    OS.EmitValue(CSI.CSOffsetExpr, 4);

    // Reserved for flags.
    OS.EmitIntValue(0, 2);
    OS.EmitIntValue(CSLocs.size(), 2);

    for (const auto &Loc : CSLocs) {
      uint8_t TypeAndFlags =
        TYPE_AND_FLAGS(Loc.Type, Loc.Ptr, Loc.Alloca, Loc.Duplicate);
      OS.EmitIntValue(TypeAndFlags, 1);
      OS.EmitIntValue(Loc.Size, 1);
      OS.EmitIntValue(Loc.Reg, 2);
      OS.EmitIntValue(Loc.Offset, 4);
      OS.EmitIntValue(Loc.AllocaSize, 4);
    }

    // Num live-out registers and padding to align to 4 byte.
    OS.EmitIntValue(0, 2);
    OS.EmitIntValue(LiveOuts.size(), 2);

    for (const auto &LO : LiveOuts) {
      OS.EmitIntValue(LO.DwarfRegNum, 2);
      OS.EmitIntValue(0, 1);
      OS.EmitIntValue(LO.Size, 1);
    }

    // Num arch-specific constants and padding to align to 4 bytes.
    OS.EmitIntValue(0, 2);
    OS.EmitIntValue(Values.size(), 2);

    for (const auto &C : Values) {
      const Location &Loc = C.first;
      const Operation &Op = C.second;

      uint8_t TypeAndFlags = ARCH_TYPE_AND_FLAGS(Loc.Type, Loc.Ptr);
      OS.EmitIntValue(TypeAndFlags, 1);
      OS.EmitIntValue(Loc.Size, 1);
      OS.EmitIntValue(Loc.Reg, 2);
      OS.EmitIntValue(Loc.Offset, 4);

      assert(!MachineGeneratedVal::ValueGenInst::PseudoInst[Op.InstType] &&
             "Generated values should be lowered to non-pseudo instructions");
      uint8_t OpType = ARCH_OP_TYPE(Op.InstType, Op.OperandType);
      OS.EmitIntValue(OpType, 1);
      OS.EmitIntValue(Op.Size, 1);
      OS.EmitIntValue(Op.DwarfReg, 2);
      if(Op.isSymbol) OS.EmitSymbolValue(Op.Symbol, 8);
      else OS.EmitIntValue(Op.Constant, 8);
    }

    // Emit alignment to 8 byte.
    OS.EmitValueToAlignment(8);
  }
}

/// Serialize the stackmap data.
void StackMaps::serializeToStackMapSection(const UnwindInfo *UI) {
  (void)WSMP;
  // Bail out if there's no stack map data.
  assert((!CSInfos.empty() || (CSInfos.empty() && ConstPool.empty())) &&
         "Expected empty constant pool too!");
  assert((!CSInfos.empty() || (CSInfos.empty() && FnStackSize.empty())) &&
         "Expected empty function record too!");
  if (CSInfos.empty())
    return;

  MCContext &OutContext = AP.OutStreamer->getContext();
  MCStreamer &OS = *AP.OutStreamer;

  // Create the section.
  MCSection *StackMapSection =
      OutContext.getObjectFileInfo()->getStackMapSection();
  OS.SwitchSection(StackMapSection);

  // Emit a dummy symbol to force section inclusion.
  OS.EmitLabel(OutContext.getOrCreateSymbol(Twine("__LLVM_StackMaps")));

  // Serialize data.
  DEBUG(dbgs() << "********** Stack Map Output **********\n");
  emitStackmapHeader(OS);
  emitFunctionFrameRecords(OS);
  emitConstantPoolEntries(OS);
  //emitCallsiteEntries(OS);
  OS.AddBlankLine();

  // Clean up.
  CSInfos.clear();
  ConstPool.clear();
}
