//===----- AArch64TargetValues.cpp - AArch64 specific value generator -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/TargetValues.h"

namespace llvm {

class AArch64Values final : public TargetValues {
public:
  AArch64Values() {}
  virtual MachineLiveValPtr getMachineValue(const MachineInstr *MI) const;

private:
  MachineLiveVal *genADDInstructions(const MachineInstr *MI) const;
  MachineLiveVal *genBitfieldInstructions(const MachineInstr *MI) const;
};

}

