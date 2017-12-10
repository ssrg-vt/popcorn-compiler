//===--------- PPCTargetValues.cpp - PPC specific value generator ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/TargetValues.h"

namespace llvm {

class PPCValues final : public TargetValues {
public:
  PPCValues() {}
  virtual MachineLiveValPtr getMachineValue(const MachineInstr *MI) const;
  virtual void addRequiredArchLiveValues(MachineFunction *MF,
                                         const MachineInstr *MIStackMap,
                                         const CallInst *IRStackMap) const;
};

}

