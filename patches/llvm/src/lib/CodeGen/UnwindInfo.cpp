//===--------------------------- UnwindInfo.cpp ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/UnwindInfo.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

using namespace llvm;

#define DEBUG_TYPE "unwindinfo"

static const char *UIDbg = "Unwind Info: ";

void UnwindInfo::recordUnwindInfo(const MachineFunction &MF) {
  // We *only* need this information for functions which have a stackmap, as
  // only those function activations can be unwound during stack
  // transformation.  This may also be a correctness criterion since we record
  // offsets from the FBP, and not all functions may have one (stackmaps are
  // implemented using FBPs, and thus prevent the FP-elimination optimization).
  if(!MF.getFrameInfo()->hasStackMap()) return;

  const MachineFrameInfo *MFI = MF.getFrameInfo();
  assert(MFI->isCalleeSavedInfoValid() && "No callee-saved information!");

  // Get this function's saved registers
  unsigned FrameReg;
  const TargetFrameLowering *TFL = MF.getSubtarget().getFrameLowering();
  const std::vector<CalleeSavedInfo> &CSI = MFI->getCalleeSavedInfo();

  // Get DWARF register number and FBP offset using callee saved information
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  CalleeSavedRegisters SavedRegs(CSI.size());
  for(unsigned i = 0; i < CSI.size(); i++) {
    SavedRegs[i].DwarfReg = TRI->getDwarfRegNum(CSI[i].getReg(), false);
    SavedRegs[i].Offset =
      TFL->getFrameIndexReferenceFromFP(MF, CSI[i].getFrameIdx(), FrameReg);

    assert(FrameReg == TRI->getFrameRegister(MF) &&
           "Invalid register used as offset for unwinding information");
    DEBUG(dbgs() << "Register " << SavedRegs[i].DwarfReg << " at register "
                 << FrameReg << " + " << SavedRegs[i].Offset << "\n");
  }

  // Save the information for when we emit the section
  const MCSymbol *FuncSym = OutContext.lookupSymbol(MF.getName());
  assert(FuncSym && "Could not find function symbol");
  FuncCalleeSaved.insert(FuncCalleePair(FuncSym, std::move(SavedRegs)));
}

void UnwindInfo::addRegisterUnwindInfo(const MachineFunction &MF,
                                       uint32_t MachineReg,
                                       int32_t Offset) {
  if(!MF.getFrameInfo()->hasStackMap()) return;

  const MCSymbol *FuncSym = OutContext.lookupSymbol(MF.getName());
  assert(FuncSym && "Could not find function symbol");
  assert(FuncCalleeSaved.find(FuncSym) != FuncCalleeSaved.end() &&
         "Cannot add register restore information -- function not found");
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  FuncCalleeSaved[FuncSym].push_back(
    RegOffset(TRI->getDwarfRegNum(MachineReg, false), Offset));
}

void UnwindInfo::emitUnwindInfo(MCStreamer &OS) {
  unsigned curIdx = 0;
  unsigned startIdx;
  FuncCalleeMap::const_iterator f, e;
  for(f = FuncCalleeSaved.begin(), e = FuncCalleeSaved.end(); f != e; f++) {
    const MCSymbol *FuncSym = f->first;
    const CalleeSavedRegisters &CSR = f->second;

    assert(FuncSym && "Invalid machine function");
    if(CSR.size() < 2)
      DEBUG(dbgs() << "WARNING: should have at least 2 registers to restore "
                               "(return address & saved FBP");

    DEBUG(dbgs() << UIDbg << "Function " << FuncSym->getName()
                 << " (offset " << curIdx << ", "
                 << CSR.size() << " entries):\n");

    startIdx = curIdx;
    CalleeSavedRegisters::const_iterator cs, cse;
    for(cs = CSR.begin(), cse = CSR.end(); cs != cse; cs++) {
      assert(cs->DwarfReg < UINT16_MAX &&
             "Register number too large for resolution");
      assert(INT16_MIN < cs->Offset && cs->Offset < INT16_MAX &&
             "Register save offset too large for resolution");

      DEBUG(dbgs() << UIDbg << "  Register " << cs->DwarfReg
                   << " saved at " << cs->Offset << "\n";);

      OS.EmitIntValue(cs->DwarfReg, 2);
      OS.EmitIntValue(cs->Offset, 2);
      curIdx++;
    }
    FuncUnwindInfo FUI(startIdx, curIdx - startIdx);
    FuncUnwindMetadata.insert(FuncUnwindPair(FuncSym, std::move(FUI)));
  }
}

void UnwindInfo::emitAddrRangeInfo(MCStreamer &OS) {
  FuncUnwindMap::const_iterator f, e;
  for(f = FuncUnwindMetadata.begin(), e = FuncUnwindMetadata.end();
      f != e;
      f++) {
    const MCSymbol *Func = f->first;
    const FuncUnwindInfo &FUI = f->second;
    OS.EmitSymbolValue(Func, 8);
    OS.EmitIntValue(FUI.NumUnwindRecord, 4);
    OS.EmitIntValue(FUI.SecOffset, 4);
  }
}

/// Serialize the unwinding information.
void UnwindInfo::serializeToUnwindInfoSection() {
  // Bail out if there's no unwind info.
  if(FuncCalleeSaved.empty()) return;

  // Emit unwinding record information.
  // FIXME: we only support ELF object files for now

  // Switch to the unwind info section
  MCStreamer &OS = *AP.OutStreamer;
  MCSection *UnwindInfoSection =
      OutContext.getObjectFileInfo()->getUnwindInfoSection();
  OS.SwitchSection(UnwindInfoSection);

  // Emit a dummy symbol to force section inclusion.
  OS.EmitLabel(OutContext.getOrCreateSymbol(Twine("__StackTransform_UnwindInfo")));

  // Serialize data.
  DEBUG(dbgs() << "********** Unwind Info Output **********\n");
  emitUnwindInfo(OS);
  OS.AddBlankLine();

  // Switch to the unwind address range section & emit section
  MCSection *UnwindAddrRangeSection =
      OutContext.getObjectFileInfo()->getUnwindAddrRangeSection();
  OS.SwitchSection(UnwindAddrRangeSection);
  OS.EmitLabel(OutContext.getOrCreateSymbol(Twine("__StackTransform_UnwindAddrRange")));
  emitAddrRangeInfo(OS);
  OS.AddBlankLine();

  Emitted = true;
}

const UnwindInfo::FuncUnwindInfo &
UnwindInfo::getUnwindInfo(const MCSymbol *Func) const {
  assert(Emitted && "Have not yet calculated per-function unwinding metadata");

  FuncUnwindMap::const_iterator it = FuncUnwindMetadata.find(Func);
  assert(it != FuncUnwindMetadata.end() && "Invalid function");
  return it->second;
}

void UnwindInfo::print(raw_ostream &OS) {
  OS << UIDbg << "Function unwinding information\n";
  FuncCalleeMap::const_iterator b, e;
  for(b = FuncCalleeSaved.begin(), e = FuncCalleeSaved.end();
      b != e;
      b++) {
    OS << UIDbg << "Function - " << b->first->getName() << "\n";
    const CalleeSavedRegisters &CSR = b->second;
    CalleeSavedRegisters::const_iterator br, be;
    for(br = CSR.begin(), be = CSR.end(); br != be; br++) {
      OS << UIDbg << "Register " << br->DwarfReg
                  << " at offset " << br->Offset << "\n";
    }
  }
}

