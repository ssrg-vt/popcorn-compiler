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
  virtual void
  getValueGenInstr(const MachineInstr *MI,
                   MachineGeneratedVal::ValueGenInstList &IL) const;
  MachineLiveValPtr getMachineValue(const MachineInstr *MI) const;
};

}

