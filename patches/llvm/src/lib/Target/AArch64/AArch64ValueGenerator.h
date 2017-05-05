//===- AArch64TargetValueGenerator.cpp - AArch64 specific value generator -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/TargetValueGenerator.h"

namespace llvm {

class AArch64ValueGenerator final : public TargetValueGenerator {
public:
  AArch64ValueGenerator() {}
  virtual MachineLiveValPtr getMachineValue(const MachineInstr *MI) const;

private:
  void genADDInstructions(const MachineInstr *MI,
                          MachineGeneratedVal::ValueGenInstList &IL) const;
};

}

