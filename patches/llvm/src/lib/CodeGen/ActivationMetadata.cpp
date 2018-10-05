//===----------------------- ActivationMetadata.cpp -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/ActivationMetadata.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

using namespace llvm;

#define DEBUG_TYPE "activationmetadata"

static cl::opt<bool> EmitMetadata(
  "act-metadata", cl::init(false),
  cl::desc("Emit metadata describing function activation layouts"));

static const char *AMDbg = "Activation metadata: ";

bool ActivationMetadata::needToRecordMetadata(const MachineFunction &MF) {
  return EmitMetadata || MF.getFrameInfo()->hasStackMap();
}

void ActivationMetadata::recordCalleeSavedRegs(const MachineFunction &MF) {
  bool Emit = true;
  unsigned FrameReg;
  CalleeSavedRegs CSRegInfo;
  CalleeSavedReg CSReg;

  DEBUG(dbgs() << AMDbg << "recording callee-saved register information\n");

  if(!MFI->isCalleeSavedInfoValid()) {
    DEBUG(dbgs() << AMDbg << "cannot emit callee-saved register information - "
                             "callee-saved information isn't valid");
    Emit = false;
  }

  if(Emit) {
    const std::vector<CalleeSavedInfo> &CSI = MFI->getCalleeSavedInfo();

    DEBUG(dbgs() << AMDbg << CSI.size() << " saved register(s)\n");

    // Get DWARF register number and FBP offset for a callee-saved register
    // using callee saved information
    CSRegInfo.reserve(CSI.size());
    for(unsigned i = 0; i < CSI.size(); i++) {
      CSReg.DwarfReg = TRI->getDwarfRegNum(CSI[i].getReg(), false);
      CSReg.Offset =
        TFL->getFrameIndexReferenceFromFP(MF, CSI[i].getFrameIdx(), FrameReg);

      // TODO need to modify this to not assume FBP -- larger frame sizes may
      // force backend to *always* return offset using SP
      DEBUG(dbgs() << AMDbg << "Register " << PrintReg(CSI[i].getReg(), TRI)
                   << " (DWARF=" << CSReg.DwarfReg << ") at register "
                   << PrintReg(FrameReg, TRI) << " + " << CSReg.Offset << "\n");
      assert(FrameReg == TRI->getFrameRegister(MF) &&
             "Invalid register used as offset for unwinding information");

      CSRegInfo.push_back(std::move(CSReg));
    }
  }

  FuncCalleeSavedInfo.emplace(FuncSym, std::move(CSRegInfo));
}

void ActivationMetadata::recordStackSlots(const MachineFunction &MF) {
  bool Emit = true;
  unsigned FrameReg;
  int LastFI;
  StackSlots SlotInfo;
  StackSlot Slot;

  DEBUG(dbgs() << AMDbg << "recording stack slot information\n");

  if(MFI->hasVarSizedObjects()) {
    DEBUG(dbgs() << AMDbg << "cannot emit stack slot metadata - frames with "
                             "variable-sized objects not supported\n");
    Emit = false;
  }
  if(MFI->getStackProtectorIndex() != -1) {
    DEBUG(dbgs() << AMDbg << "cannot emit stack slot metadata - frames with "
                             "stack protectors not supported\n");
    Emit = false;
  }
  if(MFI->hasOpaqueSPAdjustment()) {
    DEBUG(dbgs() << AMDbg << "cannot emit stack slot metadata - "
                             "frame-adjusting code not supported\n");
    Emit = false;
  }

  if(Emit) {
    LastFI = MFI->getObjectIndexEnd();

    DEBUG(dbgs() << AMDbg << LastFI
                 << " stack slot(s) (not all may be alive)\n");

    // Walk through all stack slots we can adjust at runtime (i.e., index >= 0)
    // to record their metadata
    SlotInfo.reserve(LastFI);
    for(int FI = 0; FI < LastFI; FI++) {
      if(MFI->isDeadObjectIndex(FI)) {
        DEBUG(dbgs() << AMDbg << "Skipping slot " << FI << ", is dead\n");
        continue;
      }
      Slot.Offset = TFL->getFrameIndexReferenceFromFP(MF, FI, FrameReg);
      Slot.BaseReg = TRI->getDwarfRegNum(FrameReg, false);
      Slot.Size = MFI->getObjectSize(FI);
      Slot.Alignment = MFI->getObjectAlignment(FI);

      DEBUG(
        dbgs() << AMDbg << "Slot " << FI << ": "
               << PrintReg(FrameReg, TRI) << " + " << Slot.Offset
               << ", size = " << Slot.Size
               << ", align = " << Slot.Alignment << "\n";
      );

      SlotInfo.push_back(std::move(Slot));
    }
  }

  FuncStackSlotInfo.emplace(FuncSym, std::move(SlotInfo));
}

void ActivationMetadata::recordActivationMetadata(const MachineFunction &MF) {
  MFI = MF.getFrameInfo();
  if(!needToRecordMetadata(MF)) return;

  DEBUG(dbgs() << "*** " << AMDbg << "Analyzing " << MF.getName() << " ***\n");

  const llvm::TargetSubtargetInfo &SubTarget = MF.getSubtarget();
  TFL = SubTarget.getFrameLowering();
  TRI = SubTarget.getRegisterInfo();
  FuncSym = OutContext.lookupSymbol(MF.getName());
  assert(TFL && TRI && FuncSym && "Could not get function/target information");

  recordCalleeSavedRegs(MF);
  recordStackSlots(MF);
  // TODO add callee-saved slots for architectures that don't include
  // callee-saved registers in stack slot metadata

  // Record the stack frame size
  // TODO we don't support dynamically-sized frames
  bool HasDynamicFrameSize = MF.getFrameInfo()->hasVarSizedObjects() ||
                             TRI->needsStackRealignment(MF);
  FuncMetadata[FuncSym].StackSize =
    HasDynamicFrameSize ? UINT64_MAX : MFI->getStackSize();
}

void ActivationMetadata::addRegisterUnwindInfo(const MachineFunction &MF,
                                               uint32_t MachineReg,
                                               int32_t Offset) {
  if(!needToRecordMetadata(MF)) return;

  DEBUG(dbgs() << AMDbg << "adding callee-saved register "
               << PrintReg(MachineReg, TRI) << " at offset " << Offset
               << " for " << MF.getName() << "\n");

  const MCSymbol *Sym = OutContext.lookupSymbol(MF.getName());
  assert(Sym && "Could not find symbol for machine function");
  assert(FuncCalleeSavedInfo.find(Sym) != FuncCalleeSavedInfo.end() &&
         "Cannot add register restore information -- function not found");
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  FuncCalleeSavedInfo[Sym].push_back(
    CalleeSavedReg(TRI->getDwarfRegNum(MachineReg, false), Offset));
}

void ActivationMetadata::addFunctionSize(const MachineFunction &MF,
                                         const MCExpr *FuncSize) {
  if(!needToRecordMetadata(MF)) return;

  assert(FuncSize && "Invalid size expression");
  const MCSymbol *Sym = OutContext.lookupSymbol(MF.getName());
  assert(Sym && "Could not find symbol for machine function");
  FuncMetadata[Sym].FuncSize = FuncSize;
}

/// Emit the stack slot information.
///
/// StackSlotRecords[NumRecords] {
///   uint16 : DWARF-encoded base register
///   int16  : Offset from base register
///   uint32 : Size of stack slot
///   uint32 : Alignment of stack slot
/// }
void ActivationMetadata::emitStackSlotInfo(MCStreamer &OS) {
  unsigned CurIdx = 0;

  DEBUG(dbgs() << "********** Stack Slot Output **********\n");

  for(auto SSI : FuncStackSlotInfo) {
    const MCSymbol *Sym = SSI.first;
    const StackSlots &Slots = SSI.second;

    DEBUG(dbgs() << AMDbg << "Function " << Sym->getName()
                 << " (offset = " << CurIdx << ", "
                 << Slots.size() << " entries):\n");

    for(auto Slot : Slots) {
      assert(INT32_MIN <= Slot.Offset && Slot.Offset <= INT32_MAX &&
             "Out-of-range offset");
      assert(Slot.BaseReg <= UINT16_MAX && "Out-of-range base register");
      DEBUG(dbgs() << AMDbg << "  Stack slot at register " << Slot.BaseReg
                   << " + " << Slot.Offset
                   << ", size = " << Slot.Size
                   << ", align = " << Slot.Alignment << "\n");

      OS.EmitIntValue(Slot.BaseReg, 2);
      OS.EmitIntValue(Slot.Offset, 2);
      OS.EmitIntValue(Slot.Size, 4);
      OS.EmitIntValue(Slot.Alignment, 4);
    }

    FunctionMetadata &MD = FuncMetadata[Sym];
    MD.StackSlotInfo.SecOffset = CurIdx;
    MD.StackSlotInfo.NumEntries = Slots.size();
    CurIdx += Slots.size();
  }
}

/// Emit the callee-saved register location information.
///
/// CalleeSavedRegisterLoc[NumRecords] {
///   uint16 : DWARF-encoded callee-saved register
///   int16  : Offset from base register
/// }
void ActivationMetadata::emitCalleeSavedLocInfo(MCStreamer &OS) {
  unsigned CurIdx = 0;

  DEBUG(dbgs() << "********** Callee Saved Register Output **********\n");

  for(auto CSRI : FuncCalleeSavedInfo) {
    const MCSymbol *Sym = CSRI.first;
    const CalleeSavedRegs &CSRegs = CSRI.second;

    if(CSRegs.size() < 2)
      DEBUG(dbgs() << "WARNING: should have at least 2 registers to restore "
                      "(return address & saved FBP");

    DEBUG(dbgs() << AMDbg << "Function " << Sym->getName()
                 << " (offset " << CurIdx << ", "
                 << CSRegs.size() << " entries):\n");

    for(auto CSReg : CSRegs) {
      assert(CSReg.DwarfReg < UINT16_MAX &&
             "Register number too large for resolution");
      assert(INT16_MIN < CSReg.Offset && CSReg.Offset < INT16_MAX &&
             "Register save offset too large for resolution");

      DEBUG(dbgs() << AMDbg << "  Register " << CSReg.DwarfReg
                   << " saved at FBP + " << CSReg.Offset << "\n");

      OS.EmitIntValue(CSReg.DwarfReg, 2);
      OS.EmitIntValue(CSReg.Offset, 2);
    }

    FunctionMetadata &MD = FuncMetadata[Sym];
    MD.CalleeSavedInfo.SecOffset = CurIdx;
    MD.CalleeSavedInfo.NumEntries = CSRegs.size();
    CurIdx += CSRegs.size();
  }
}

/// Emit the function metadata.
///
/// FunctionRecord[NumRecords] {
///   uint64 : Function address
///   uint32 : Size of function's code
///   uint32 : Size of function's stack frame
///   uint16 : Number of callee-saved register locations
///   uint64 : Offset in callee-saved register location section
///   uint16 : Number of stack slot records
///   uint64 : Offset in stack slot record section
/// }
void ActivationMetadata::emitFunctionMetadata(MCStreamer &OS) {
  DEBUG(dbgs() << "********** Function Metadata Output **********\n");

  for(auto FM : FuncMetadata) {
    const MCSymbol *Func = FM.first;
    const FunctionMetadata &Refs = FM.second;

    DEBUG(dbgs() << AMDbg << "Function " << Func->getName()
                 << ": stack size = " << Refs.StackSize << ", "
                 << Refs.CalleeSavedInfo.NumEntries
                 << " callee-saved register(s) (offset="
                 << Refs.CalleeSavedInfo.SecOffset << "), "
                 << Refs.StackSlotInfo.NumEntries
                 << " stack slot(s) (offset="
                 << Refs.StackSlotInfo.SecOffset << ")\n");

    OS.EmitSymbolValue(Func, 8);
    OS.EmitValue(Refs.FuncSize, 4);
    OS.EmitIntValue(Refs.StackSize, 4);
    OS.EmitIntValue(Refs.CalleeSavedInfo.NumEntries, 2);
    OS.EmitIntValue(Refs.CalleeSavedInfo.SecOffset, 8);
    OS.EmitIntValue(Refs.StackSlotInfo.NumEntries, 2);
    OS.EmitIntValue(Refs.StackSlotInfo.SecOffset, 8);
  }
}

/// Serialize the unwinding information.
void ActivationMetadata::serializeToActivationMetadataSection() {
  // FIXME: we only support ELF object files for now
  MCStreamer &OS = *AP.OutStreamer;

  DEBUG(dbgs() << "********** Activation Metadata Info Output **********\n");

  // Emit stack slot records
  if(!FuncStackSlotInfo.empty()) {
    // Switch to the correct output section.
    MCSection *ActStackSlots =
      OutContext.getObjectFileInfo()->getActStackSlotSection();
    OS.SwitchSection(ActStackSlots);

    // Emit a dummy symbol to force section inclusion.
    OS.EmitLabel(
      OutContext.getOrCreateSymbol(Twine("__StackTransform_StackSlotInfo")));

    // Serialize data.
    emitStackSlotInfo(OS);
    OS.AddBlankLine();
  }

  // Emit callee saved register locations
  if(!FuncCalleeSavedInfo.empty()) {
    MCSection *UnwindInfoSection =
      OutContext.getObjectFileInfo()->getUnwindInfoSection();
    OS.SwitchSection(UnwindInfoSection);
    OS.EmitLabel(
      OutContext.getOrCreateSymbol(Twine("__StackTransform_UnwindInfo")));
    emitCalleeSavedLocInfo(OS);
    OS.AddBlankLine();
  }

  // Emit function metadata to reference the above metadata in other sections
  if(!FuncMetadata.empty()) {
    MCSection *FuncMetadataSection =
      OutContext.getObjectFileInfo()->getFuncMetadataSection();
    OS.SwitchSection(FuncMetadataSection);
    OS.EmitLabel(
      OutContext.getOrCreateSymbol(Twine("__StackTransform_FuncMetadata")));
    emitFunctionMetadata(OS);
    OS.AddBlankLine();
  }

  Emitted = true;
}

const ActivationMetadata::FunctionMetadata ActivationMetadata::EmptyMD;
const ActivationMetadata::ExternalEntriesInfo ActivationMetadata::EmptySSI;

const ActivationMetadata::FunctionMetadata &
ActivationMetadata::getMetadata(const MCSymbol *Func) const {
  assert(Emitted && "Have not yet emitted per-function activation metadata");

  FuncMetaMap::const_iterator it = FuncMetadata.find(Func);
  if(it == FuncMetadata.end()) {
    DEBUG(dbgs() << "WARNING: could not find metadata for "
                 << Func->getName() << "\n");
    return EmptyMD;
  }
  else return it->second;
}

const ActivationMetadata::ExternalEntriesInfo &
ActivationMetadata::getStackSlotInfo(const MCSymbol *Func) const {
  const FunctionMetadata &MD = getMetadata(Func);
  if(MD.StackSize == UINT64_MAX) return EmptySSI;
  else return MD.StackSlotInfo;
}

const ActivationMetadata::ExternalEntriesInfo &
ActivationMetadata::getCalleeSavedInfo(const MCSymbol *Func) const {
  const FunctionMetadata &MD = getMetadata(Func);
  if(MD.StackSize == UINT64_MAX) return EmptySSI;
  else return MD.CalleeSavedInfo;
}

void ActivationMetadata::print(raw_ostream &OS) {
  OS << AMDbg << "Stack slot information\n";
  for(auto const Slots : FuncStackSlotInfo) {
    OS << AMDbg << "Function - " << Slots.first->getName() << "\n";
    for(auto const Slot : Slots.second) {
      OS << AMDbg << "  Stack slot at register " << Slot.BaseReg
                  << " + " << Slot.Offset
                  << ", size = " << Slot.Size
                  << ", alignment = " << Slot.Alignment << "\n";
    }
  }

  OS << AMDbg << "Callee-saved register location information\n";
  for(auto const CSRegs : FuncCalleeSavedInfo) {
    OS << AMDbg << "Function - " << CSRegs.first->getName() << "\n";
    for(auto const CSReg : CSRegs.second) {
      OS << AMDbg << "Register " << CSReg.DwarfReg
                  << " at offset " << CSReg.Offset << "\n";
    }
  }
}

