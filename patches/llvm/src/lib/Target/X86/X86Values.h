//===--------- X86TargetValues.cpp - X86 specific value generator ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/TargetValues.h"

namespace llvm {

class X86Values final : public TargetValues {
public:
  X86Values() {}
  virtual TemporaryValuePtr getTemporaryValue(const MachineInstr *MI,
                                              const VirtRegMap *VRM) const;
  virtual MachineLiveValPtr getMachineValue(const MachineInstr *MI) const;

private:
  MachineLiveVal *genLEAInstructions(const MachineInstr *LEA) const;
};

}

