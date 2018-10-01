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
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

using namespace llvm;

#define DEBUG_TYPE "activationmetadata"

static cl::opt<bool> EmitMetadata(
  "act-metadata", cl::init(false),
  cl::desc("Emit activation metadata for single-ISA transformations"));

static const char *AMDbg = "Activation metadata: ";

void ActivationMetadata::recordActivationMetadata(const MachineFunction &MF) {
  if(!EmitMetadata) return;

  bool Emit = true;
  const MachineFrameInfo *MFI = MF.getFrameInfo();
  const MCSymbol *FuncSym = OutContext.lookupSymbol(MF.getName());
  StackSlots SlotInfo;
  StackSlot Slot;

  DEBUG(dbgs() << "**** " << AMDbg << "Analyzing " << MF.getName() << " ****\n");

  if(MFI->hasVarSizedObjects()) {
    DEBUG(dbgs() << AMDbg
                 << "frames with variable-sized objects not supported\n");
    Emit = false;
  }
  if(MFI->getStackProtectorIndex() != -1) {
    DEBUG(dbgs() << AMDbg << "frames with stack protectors not supported\n");
    Emit = false;
  }
  if(MFI->hasOpaqueSPAdjustment()) {
    DEBUG(dbgs() << AMDbg << "frame-adjusting code not supported\n");
    Emit = false;
  }

  if(Emit) {
    // Get this function's stack slots
    unsigned FrameReg;
    const TargetFrameLowering *TFL = MF.getSubtarget().getFrameLowering();
    const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

    // Walk through all stack slots we can adjust at runtime (i.e., index >= 0)
    // to record their metadata
    int LastFI = MFI->getObjectIndexEnd();
    SlotInfo.reserve(LastFI);
    for(int FI = 0; FI < LastFI; FI++) {
      if(MFI->isDeadObjectIndex(FI)) continue;
      Slot.Offset = TFL->getFrameIndexReference(MF, FI, FrameReg);
      Slot.BaseReg = TRI->getDwarfRegNum(FrameReg, false);
      Slot.Size = MFI->getObjectSize(FI);
      Slot.Alignment = MFI->getObjectAlignment(FI);

      DEBUG(
        dbgs() << AMDbg << "Slot " << FI << ": "
               << Slot.BaseReg << " + " << Slot.Offset
               << ", size = " << Slot.Size
               << ", align = " << Slot.Alignment << "\n";
      );

      SlotInfo.push_back(std::move(Slot));
    }
  }

  // Save the information for when we emit the section
  assert(FuncSym && "Could not find function symbol");
  StackSlotInfo.insert(FuncStackSlotPair(FuncSym, std::move(SlotInfo)));
}

void ActivationMetadata::emitStackSlotInfo(MCStreamer &OS) {
  unsigned CurIdx = 0;
  for(auto SSI : StackSlotInfo) {
    const MCSymbol *Func = SSI.first;
    const StackSlots &Slots = SSI.second;

    assert(Func && "Invalid machine function");
    DEBUG(dbgs() << "Function " << Func->getName()
                 << " (offset = " << CurIdx << ", "
                 << Slots.size() << " entries)\n");

    for(auto Slot : Slots) {
      assert(INT32_MIN <= Slot.Offset && Slot.Offset <= INT32_MAX &&
             "Out-of-range offset");
      assert(Slot.BaseReg <= UINT16_MAX && "Out-of-range base register");

      DEBUG(dbgs() << "  Stack slot at " << Slot.BaseReg
                   << " + " << Slot.Offset
                   << ", size = " << Slot.Size
                   << ", align = " << Slot.Alignment << "\n");
      OS.EmitIntValue(Slot.Offset, 4);
      OS.EmitIntValue(Slot.BaseReg, 4);
      OS.EmitIntValue(Slot.Size, 4);
      OS.EmitIntValue(Slot.Alignment, 4);
    }
    FuncActivationMetadata FAM(CurIdx, Slots.size());
    FuncMetadata.insert(FuncActivationPair(Func, std::move(FAM)));
    CurIdx += Slots.size();
  }
}

/// Serialize the unwinding information.
void ActivationMetadata::serializeToActivationMetadataSection() {
  if(!StackSlotInfo.empty()) {
    // Emit unwinding record information.
    // FIXME: we only support ELF object files for now

    // Switch to the unwind info section
    MCStreamer &OS = *AP.OutStreamer;
    MCSection *ActStackSlots =
        OutContext.getObjectFileInfo()->getActStackSlotSection();
    OS.SwitchSection(ActStackSlots);

    // Emit a dummy symbol to force section inclusion.
    OS.EmitLabel(OutContext.getOrCreateSymbol(Twine("__StackTransform_StackSlotInfo")));

    // Serialize data.
    DEBUG(dbgs() << "********** Activation Metadata Info Output **********\n");
    emitStackSlotInfo(OS);
    OS.AddBlankLine();
  }

  Emitted = true;
}

ActivationMetadata::FuncActivationMetadata ActivationMetadata::EmptyMD;

const ActivationMetadata::FuncActivationMetadata &
ActivationMetadata::getActivationMetadata(const MCSymbol *Func) const {
  assert(Emitted && "Have not yet emitted per-function activation metadata");

  FuncActivationMap::const_iterator it = FuncMetadata.find(Func);
  if(it == FuncMetadata.end()) {
    DEBUG(dbgs() << "WARNING: could not find metadata for "
                 << Func->getName() << "\n");
    return EmptyMD;
  }
  else return it->second;
}

void ActivationMetadata::print(raw_ostream &OS) {
  OS << "Stack slot information\n";
  for(auto Slots : StackSlotInfo) {
    OS << "Function - " << Slots.first->getName() << "\n";
    for(auto Slot : Slots.second) {
      OS << "  Stack slot at register " << Slot.BaseReg
         << " + " << Slot.Offset
         << ", size = " << Slot.Size
         << ", alignment = " << Slot.Alignment << "\n";
    }
  }
}

