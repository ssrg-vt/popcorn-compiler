//===----- X86TargetValueGenerator.cpp - X86 specific value generator -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/TargetValueGenerator.h"

namespace llvm {

class X86ValueGenerator final : public TargetValueGenerator {
public:
  X86ValueGenerator() {}
  virtual MachineLiveValPtr getMachineValue(const MachineInstr *MI) const;

private:
  void genLEAInstructions(const MachineInstr *LEA,
                          MachineGeneratedVal::ValueGenInstList &IL) const;
};

}

