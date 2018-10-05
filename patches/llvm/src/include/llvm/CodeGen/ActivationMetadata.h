//===--------- ActivationMetadata.h - ActivationMetadata --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Generate metadata describing function activation layouts.  This allows an
// instrumentation framework to adjust the layout at runtime.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_ACTIVATIONMETADATA_H
#define LLVM_CODEGEN_ACTIVATIONMETADATA_H

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/Debug.h"
#include <map>

namespace llvm {

class MachineFrameInfo;
class TargetFrameLowering;
class TargetRegisterInfo;
class MCSymbol;
class MCExpr;

class ActivationMetadata {
public:
  /// Stack slots
  struct StackSlot {
    int64_t Offset; // Offset from frame register
    unsigned BaseReg; // Frame register used for offset (DWARF encoding)
    uint32_t Size; // Size of stack slot
    uint32_t Alignment; // Alignment of stack slot
  };

  typedef SmallVector<StackSlot, 8> StackSlots;
  typedef std::pair<const MCSymbol *, StackSlots> FuncStackSlotPair;
  typedef std::map<const MCSymbol *, StackSlots> FuncStackSlotMap;

  /// Callee-saved registers
  struct CalleeSavedReg {
    uint32_t DwarfReg; // Register saved on the stack
    int32_t Offset; // Offset at which register was saved
    CalleeSavedReg() : DwarfReg(UINT32_MAX), Offset(INT32_MAX) {}
    CalleeSavedReg(uint32_t DwarfReg, int32_t Offset)
      : DwarfReg(DwarfReg), Offset(Offset) {}
  };

  typedef SmallVector<CalleeSavedReg, 16> CalleeSavedRegs;
  typedef std::pair<const MCSymbol *, CalleeSavedRegs> FuncCalleePair;
  typedef std::map<const MCSymbol *, CalleeSavedRegs> FuncCalleeMap;

  /// Information for referencing a block of contiguous entries contained in
  /// another section.
  struct ExternalEntriesInfo {
    uint32_t SecOffset; // Offset into the external section
    uint32_t NumEntries; // Number of entries
    ExternalEntriesInfo() : SecOffset(UINT32_MAX), NumEntries(0) {}
    ExternalEntriesInfo(uint32_t SecOffset, uint32_t NumEntries)
      : SecOffset(SecOffset), NumEntries(NumEntries) {}
  };

  /// References to other sections for all emitted metadata.
  struct FunctionMetadata {
    const MCExpr *FuncSize;
    uint64_t StackSize;
    ExternalEntriesInfo StackSlotInfo;
    ExternalEntriesInfo CalleeSavedInfo;
    FunctionMetadata() : StackSize(UINT64_MAX) {}
  };

  typedef std::pair<const MCSymbol *, FunctionMetadata> FuncMetaPair;
  typedef std::map<const MCSymbol *, FunctionMetadata> FuncMetaMap;

  ActivationMetadata() = delete;
  ActivationMetadata(AsmPrinter &AP)
    : AP(AP), OutContext(AP.OutStreamer->getContext()), Emitted(false) {};

  void reset() {
    FuncStackSlotInfo.clear();
    FuncCalleeSavedInfo.clear();
    FuncMetadata.clear();
    Emitted = false;
  }

  /// Return whether or not we should record metadata for the given function.
  static bool needToRecordMetadata(const MachineFunction &MF);

  /// Record all activation metadata.
  void recordActivationMetadata(const MachineFunction &MF);

  /// Add a register restore offset for a function.  MachineReg will get
  /// converted to a DWARF register internally.
  void addRegisterUnwindInfo(const MachineFunction &MF,
                             uint32_t MachineReg,
                             int32_t Offset);

  /// Add an expression that calculates a function's size to the metadata.
  void addFunctionSize(const MachineFunction &MF, const MCExpr *FuncSize);

  /// Emit activation metadata.
  void serializeToActivationMetadataSection();

  /// Get activation metadata for a function
  const FunctionMetadata &getMetadata(const MCSymbol *Func) const;
  const ExternalEntriesInfo &getStackSlotInfo(const MCSymbol *Func) const;
  const ExternalEntriesInfo &getCalleeSavedInfo(const MCSymbol *Func) const;

private:
  AsmPrinter &AP;
  MCContext &OutContext;
  FuncStackSlotMap FuncStackSlotInfo;
  FuncCalleeMap FuncCalleeSavedInfo;
  FuncMetaMap FuncMetadata;
  bool Emitted;

  /// Information-providing objects used for generating metadata -- only valid
  /// inside call to recordActivationMetadata()
  const MachineFrameInfo *MFI;
  const TargetFrameLowering *TFL;
  const TargetRegisterInfo *TRI;
  const MCSymbol *FuncSym;

  const static FunctionMetadata EmptyMD;
  const static ExternalEntriesInfo EmptySSI;

  /// Record each type of metadata
  void recordCalleeSavedRegs(const MachineFunction &MF);
  void recordStackSlots(const MachineFunction &MF);

  /// Emit the stack slot info for all functions.
  void emitStackSlotInfo(MCStreamer &OS);

  /// Emit the callee-saved register locations for all functions.
  void emitCalleeSavedLocInfo(MCStreamer &OS);

  /// Emit the top-level function metadata for all functions.
  void emitFunctionMetadata(MCStreamer &OS);

  void print(raw_ostream &OS);
  void debug() { print(dbgs()); }
};
}

#endif
