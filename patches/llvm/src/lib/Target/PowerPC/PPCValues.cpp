//===--------- PPCTargetValues.cpp - PPC specific value generator ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PPCFrameLowering.h"
#include "PPCValues.h"
#include "PPC.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "stacktransform"

using namespace llvm;

MachineLiveValPtr PPCValues::getMachineValue(const MachineInstr *MI) const {
  // TODO
  return nullptr;
}

void PPCValues::addRequiredArchLiveValues(MachineFunction *MF,
                                          const MachineInstr *MIStackMap,
                                          const CallInst *IRStackMap) const {
  if(!MF->getRegInfo().use_empty(PPC::X2)) {
    MachineOperand TOCRef = MachineOperand::CreateES(".TOC.");
    MachineSymbolRef TOCSym(TOCRef, MIStackMap, true);

    DEBUG(dbgs() << "   + Setting R2 to be TOC pointer\n");
    MachineLiveReg TOCPtr(PPC::X2);
    MF->addSMArchSpecificLocation(IRStackMap, TOCPtr, TOCSym);

    // Per the ELFv2 ABI, the TOC Pointer Doubleword save area is at SP + 24
    DEBUG(dbgs() << "   + Setting TOC pointer save slot to be TOC pointer\n");
    const PPCFrameLowering *PFL =
      (const PPCFrameLowering *)MF->getSubtarget().getFrameLowering();
    MachineLiveStackAddr TOCSS(PFL->getTOCSaveOffset(), PPC::X1, 8);
    MF->addSMArchSpecificLocation(IRStackMap, TOCSS, TOCSym);
  }
}

