//===--------- TargetValues.cpp - Target value generator helpers ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetValues.h"

#define DEBUG_TYPE "stacktransform"

using namespace llvm;

bool TargetValues::isSymbolValueConstant(const MachineOperand &MO) {
  const GlobalValue *GV;
  const GlobalVariable *GVar;

  switch(MO.getType()) {
  case MachineOperand::MO_GlobalAddress:
    GV = MO.getGlobal();
    if(isa<Function>(GV)) return true;
    else if((GVar = dyn_cast<GlobalVariable>(GV)) && GVar->isConstant())
      return true;
    break;
  case MachineOperand::MO_ExternalSymbol:
    // TODO
    break;
  case MachineOperand::MO_MCSymbol:
    // TODO
    break;
  default:
    DEBUG(dbgs() << "Unhandled reference type\n");
    break;
  }
  return false;
}

