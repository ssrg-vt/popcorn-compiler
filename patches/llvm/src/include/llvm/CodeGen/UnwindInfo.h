//===----------------- UnwindInfo.h - UnwindInfo ---------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Generate unwinding information for stack transformation runtime.  Note that
// this is implemented assuming the function uses a frame base pointer (FBP).
// This requirement is guaranteed to be satisfied if the function has a
// stackmap, which are the only functions for which we want to generate
// unwinding information.
//
//===---------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_UNWINDINFO_H
#define LLVM_CODEGEN_UNWINDINFO_H

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/Debug.h"
#include <map>

namespace llvm {

class UnwindInfo {
public:
  /// Per-function unwinding metadata classes & typedefs
  class FuncUnwindInfo {
  public:
    uint32_t SecOffset; // Offset into unwinding record section
    uint32_t NumUnwindRecord; // Number of unwinding records

    FuncUnwindInfo() : SecOffset(UINT32_MAX), NumUnwindRecord(0) {}
    FuncUnwindInfo(uint32_t SecOffset, uint32_t NumUnwindRecord)
      : SecOffset(SecOffset), NumUnwindRecord(NumUnwindRecord) {}
  };

  typedef std::pair<const MCSymbol *, FuncUnwindInfo> FuncUnwindPair;
  typedef std::map<const MCSymbol *, FuncUnwindInfo> FuncUnwindMap;

  /// Unwinding record classes & typedefs
  class RegOffset {
  public:
    uint32_t DwarfReg;
    int32_t Offset;

    RegOffset() : DwarfReg(0), Offset(0) {}
    RegOffset(uint32_t DwarfReg, int32_t Offset) :
      DwarfReg(DwarfReg), Offset(Offset) {}
  };

  typedef SmallVector<RegOffset, 32> CalleeSavedRegisters;
  typedef std::pair<const MCSymbol *, CalleeSavedRegisters> FuncCalleePair;
  typedef std::map<const MCSymbol *, CalleeSavedRegisters> FuncCalleeMap;

  /// \brief Constructors
  UnwindInfo() = delete;
  UnwindInfo(AsmPrinter &AP)
    : AP(AP), OutContext(AP.OutStreamer->getContext()), Emitted(false) {};

  /// \bried Clear all saved unwinding information
  void reset() {
    Emitted = false;
    FuncCalleeSaved.clear();
    FuncUnwindMetadata.clear();
  }

  /// \brief Store unwinding information for a function
  void recordUnwindInfo(const MachineFunction &MF);

  /// \brief Add a register restore offset for a function.  MachineReg will get
  /// converted to a DWARF register internally.
  void addRegisterUnwindInfo(const MachineFunction &MF,
                             uint32_t MachineReg,
                             int32_t Offset);

  /// Create an unwinding information section and serialize the map info into
  /// it.
  ///
  /// Note: unlike StackMaps::serializeToStackMapSection, this function *does
  /// not* clear out the data structures.  This is so that the stack map
  /// machinery can access per-function unwinding information.
  void serializeToUnwindInfoSection();

  /// Get unwinding section metadata for a function
  const FuncUnwindInfo &getUnwindInfo(const MCSymbol *Func) const;

private:
  AsmPrinter &AP;
  MCContext &OutContext;
  FuncCalleeMap FuncCalleeSaved;
  FuncUnwindMap FuncUnwindMetadata;
  bool Emitted;

  /// \brief Emit the unwind info for each function.
  void emitUnwindInfo(MCStreamer &OS);

  /// \brief Emit the address range info for each function.
  void emitAddrRangeInfo(MCStreamer &OS);

  void print(raw_ostream &OS);
  void debug() { print(dbgs()); }
};
}

#endif
