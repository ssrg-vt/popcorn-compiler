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
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/Debug.h"
#include <map>

namespace llvm {

class ActivationMetadata {
public:
  struct FuncActivationMetadata {
    // TODO other types of metadata, e.g., register liveness information?
    uint32_t StackSlotOffset; // Offset into stack slot section
    uint32_t NumStackSlot; // Number of stack slots

    FuncActivationMetadata() : StackSlotOffset(UINT32_MAX), NumStackSlot(0) {}
    FuncActivationMetadata(uint32_t StackSlotOffset, uint32_t NumStackSlot)
      : StackSlotOffset(StackSlotOffset), NumStackSlot(NumStackSlot) {}
  };

  typedef std::pair<const MCSymbol *, FuncActivationMetadata> FuncActivationPair;
  typedef std::map<const MCSymbol *, FuncActivationMetadata> FuncActivationMap;

  struct StackSlot {
    int64_t Offset; // Offset from frame register
    unsigned BaseReg; // Frame register used for offset (DWARF encoding)
    uint32_t Size; // Size of stack slot
    uint32_t Alignment; // Alignment of stack slot
  };

  typedef SmallVector<StackSlot, 8> StackSlots;
  typedef std::pair<const MCSymbol *, StackSlots> FuncStackSlotPair;
  typedef std::map<const MCSymbol *, StackSlots> FuncStackSlotMap;

  ActivationMetadata() = delete;
  ActivationMetadata(AsmPrinter &AP)
    : AP(AP), OutContext(AP.OutStreamer->getContext()), Emitted(false) {};

  void reset() {
    Emitted = false;
    StackSlotInfo.clear();
    FuncMetadata.clear();
  }

  /// Record stack slot locations, sizes and alignments.
  void recordActivationMetadata(const MachineFunction &MF);

  /// Emit activation metadata.
  void serializeToActivationMetadataSection();

  /// Get activation metadata for a function
  const FuncActivationMetadata &
  getActivationMetadata(const MCSymbol *Func) const;

private:
  AsmPrinter &AP;
  MCContext &OutContext;
  FuncStackSlotMap StackSlotInfo;
  FuncActivationMap FuncMetadata;
  bool Emitted;

  static FuncActivationMetadata EmptyMD;

  /// \brief Emit the unwind info for each function.
  void emitStackSlotInfo(MCStreamer &OS);

  void print(raw_ostream &OS);
  void debug() { print(dbgs()); }
};
}

#endif
