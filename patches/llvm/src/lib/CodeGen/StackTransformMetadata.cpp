//=== llvm/CodeGen/StackTransformMetadata.cpp - Stack Transformation Metadata ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file accumulates additional data from machine functions needed to do
// correct and complete stack transformation.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/LiveIntervalAnalysis.h"
#include "llvm/CodeGen/LiveStackAnalysis.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/StackTransformTypes.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "stacktransform"

//===----------------------------------------------------------------------===//
//                          StackTransformMetadata
//===----------------------------------------------------------------------===//
//
// Run analyses over machine functions (before virtual register rewriting) to
// glean additional information about live values.  This analysis finds
// duplicate locations for live values (including backing stack slots and other
// registers) and architecture-specific live values that must be materialized.
//
//===----------------------------------------------------------------------===//
namespace {
class StackTransformMetadata : public MachineFunctionPass {
  MachineFunction *MF;
  const MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
  const LiveIntervals *LI;
  const LiveStacks *LS;
  const SlotIndexes *Indexes;
  const VirtRegMap *VRM;

  /// A bundle tying together a stackmap IR instruction, the generated stackmap
  /// machine instruction and the call machine instruction that caused the
  /// stackmap to be emitted in the IR, respectively
  typedef std::tuple<const CallInst *,
                     const MachineInstr *,
                     const MachineInstr *> SMInstBundle;

  /// Getters for individual elements of instruction bundles
  static inline const CallInst *getIRSM(const SMInstBundle &B) { return std::get<0>(B); }
  static inline const MachineInstr *getMISM(const SMInstBundle &B) { return std::get<1>(B); }
  static inline const MachineInstr *getMICall(const SMInstBundle &B) { return std::get<2>(B); }

  /// Stackmap instructions & the associated call machine instruction
  SmallVector<SMInstBundle, 32> SM;

  /// A vector of IR values.  Used when mapping from registers/stack slots to
  /// IR values.
  typedef SmallVector<const Value *, 4> ValueVec;

  /// Mapping between virtual registers and IR operands
  typedef std::pair<unsigned, ValueVec> RegValsPair;
  typedef std::map<unsigned, ValueVec > RegValsMap;

  /// Mapping between stackmaps and virtual registers referenced by the stackmap
  typedef std::pair<const MachineInstr *, RegValsMap> SMVregsPair;
  typedef std::map<const MachineInstr *, RegValsMap> SMVregsMap;
  SMVregsMap SMVregs;

  /// Mapping between stack slots and IR operands
  typedef std::pair<int, ValueVec> StackValsPair;
  typedef std::map<int, ValueVec> StackValsMap;

  /// Mapping between stackmaps and stack slots referenced by the stackmap
  typedef std::pair<const MachineInstr *, StackValsMap> SMStackSlotPair;
  typedef std::map<const MachineInstr *, StackValsMap> SMStackSlotMap;
  SMStackSlotMap SMStackSlots;

  /// A value's spill location
  class SpillLoc {
  public:
    enum Type { NONE, VREG, STACK_LOAD, STACK_STORE };
    unsigned Vreg;
    SpillLoc() : Vreg(VirtRegMap::NO_PHYS_REG) {}
    SpillLoc(unsigned Vreg) : Vreg(Vreg) {}
    virtual SpillLoc *copy() const = 0;
    virtual ~SpillLoc() {}
    virtual Type getType() const = 0;
  };

  /// A spill to a stack slot
  class StackSpillLoc : public SpillLoc {
  public:
    int StackSlot;
    StackSpillLoc() : StackSlot(VirtRegMap::NO_STACK_SLOT) {}
    StackSpillLoc(unsigned Vreg, int StackSlot) :
                      SpillLoc(Vreg), StackSlot(StackSlot) {}
    virtual SpillLoc *copy() const = 0;
    virtual Type getType() const = 0;
  };

  /// A load from a stack slot
  class StackLoadLoc : public StackSpillLoc {
  public:
    StackLoadLoc() {}
    StackLoadLoc(unsigned Vreg, int StackSlot) :
                     StackSpillLoc(Vreg, StackSlot) {}
    virtual SpillLoc *copy() const {
      return new StackLoadLoc(Vreg, StackSlot);
    }
    virtual Type getType() const { return SpillLoc::STACK_LOAD; }
  };

  /// A store to a stack slot
  class StackStoreLoc : public StackSpillLoc {
  public:
    StackStoreLoc() {}
    StackStoreLoc(unsigned Vreg, int StackSlot) :
                      StackSpillLoc(Vreg, StackSlot) {}
    virtual SpillLoc *copy() const {
      return new StackStoreLoc(Vreg, StackSlot);
    }
    virtual Type getType() const { return SpillLoc::STACK_STORE; }
  };

  /// A spill to another register
  class RegSpillLoc : public SpillLoc {
  public:
    unsigned SrcVreg;
    RegSpillLoc() : SrcVreg(VirtRegMap::NO_PHYS_REG) {}
    RegSpillLoc(unsigned DefVreg, unsigned SrcVreg) :
                    SpillLoc(DefVreg), SrcVreg(SrcVreg) {}
    virtual SpillLoc *copy() const {
      return new RegSpillLoc(Vreg, SrcVreg);
    }
    virtual Type getType() const { return SpillLoc::VREG; }
  };

  /// Gather stackmap machine instructions, the IR instructions which generated
  /// the stackmaps, and their associated call machine instructions
  void bundleStackmaps();

  /// Find stackmap operands that have been spilled to alternate locations
  void findAlternateOpLocs();

  /// Find architecture-specific live values added by the backend
  void findArchSpecificLiveVals();

  /// Find all virtual register/stack slot operands in a stackmap and collect
  /// virtual register/stack slot <-> IR value mappings
  void mapOpsToIR(const CallInst *IRSM, const MachineInstr *MISM);

  /// Unwind live value movement in the series of instructions between a call
  /// and a stackmap
  void unwindToCall(const SMInstBundle &SM);

  /// Analyze a machine instruction to see if a value is getting restored
  /// from a spill location.
  SpillLoc *getSpillLocation(const MachineInstr *MI) const;

  /// Analyze a machine instruction to find the value being used
  MachineConstant *getTargetValue(const MachineInstr *MI) const;

  /// Return whether or not a virtual registers is defined within a range of
  /// machine instructions, inclusive
  const MachineInstr *definedInRange(const MachineInstr *Start,
                                     const MachineInstr *End,
                                     unsigned Vreg) const;

  /// Is a virtual register live across the machine instruction?
  bool isVregLiveAcrossInstr(unsigned Vreg, const MachineInstr *MI);

  /// Is a stack slot live across the machine instruction?
  bool isSSLiveAcrossInstr(int SS, const MachineInstr *MI);

  /// Find if a virtual register is backed by a stack slot
  int findBackingStackSlots(unsigned Vreg, const MachineInstr *SM);
public:
  static char ID;
  static const std::string SMName;

  StackTransformMetadata() : MachineFunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnMachineFunction(MachineFunction&) override;

};

} // end anonymous namespace

char &llvm::StackTransformMetadataID = StackTransformMetadata::ID;
const std::string StackTransformMetadata::SMName("llvm.experimental.stackmap");

INITIALIZE_PASS_BEGIN(StackTransformMetadata, "stacktransformmetadata",
  "Analyze functions for additional stack transformation metadata", false, true)
INITIALIZE_PASS_DEPENDENCY(SlotIndexes)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_DEPENDENCY(LiveStacks)
INITIALIZE_PASS_DEPENDENCY(VirtRegMap)
INITIALIZE_PASS_END(StackTransformMetadata, "stacktransformmetadata",
  "Analyze functions for additional stack transformation metadata", false, true)

char StackTransformMetadata::ID = 0;

void StackTransformMetadata::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<LiveIntervals>();
  AU.addRequired<LiveStacks>();
  AU.addRequired<SlotIndexes>();
  AU.addRequired<VirtRegMap>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

bool StackTransformMetadata::runOnMachineFunction(MachineFunction &fn) {
  MF = &fn;
  TII = MF->getSubtarget().getInstrInfo();
  MRI = &MF->getRegInfo();
  Indexes = &getAnalysis<SlotIndexes>();
  LI = &getAnalysis<LiveIntervals>();
  LS = &getAnalysis<LiveStacks>();
  VRM = &getAnalysis<VirtRegMap>();
  SM.clear();
  SMVregs.clear();
  SMStackSlots.clear();

  if(MF->getFrameInfo()->hasStackMap()) {
    DEBUG(
      dbgs() << "\n********** STACK TRANSFORMATION METADATA **********\n"
             << "********** Function: " << MF->getName() << '\n';
      VRM->dump();
    );

    bundleStackmaps();
    findAlternateOpLocs();
    findArchSpecificLiveVals();
  }

  return false;
}

/// Gather stackmap machine instructions, the IR instructions which generated
/// the stackmaps, and their associated call machine instructions
void
StackTransformMetadata::bundleStackmaps() {
  for(auto MBB = MF->begin(), MBBE = MF->end(); MBB != MBBE; MBB++) {
    for(auto MI = MBB->instr_begin(), MIE = MBB->instr_end();
        MI != MIE;
        MI++) {
      if(MI->getOpcode() == TargetOpcode::STACKMAP) {
        // Find the stackmap IR instruction
        assert(MI->getOperand(0).isImm() && "Invalid stackmap ID");
        int64_t ID = MI->getOperand(0).getImm();
        const BasicBlock *BB = MI->getParent()->getBasicBlock();
        const CallInst *SMIR = nullptr;
        for(auto I = BB->begin(), IE = BB->end(); I != IE; I++)
        {
          const IntrinsicInst *II;
          if((II = dyn_cast<IntrinsicInst>(&*I)) &&
             II->getCalledFunction()->getName() == SMName &&
             cast<ConstantInt>(II->getArgOperand(0))->getSExtValue() == ID) {
            SMIR = cast<CallInst>(II);
            break;
          }
        }
        assert(SMIR && "Could not find stackmap IR instruction");

        // Find the call instruction
        const MachineInstr *MCI = MI->getPrevNode();
        while(MCI != nullptr) {
          if(MCI->isCall()) {
            if(MCI->getOpcode() == TargetOpcode::STACKMAP)
              MCI = nullptr;
            break;
          }
          MCI = MCI->getPrevNode();
        }

        if(!MCI) {
          DEBUG(
            dbgs() << "WARNING: stackmap " << ID << " ";
            SMIR->printAsOperand(dbgs());
            dbgs() << ": could not find associated call instruction "
                      "(lowered to a native instruction?)\n";
          );
          continue;
        }

        SM.push_back(SMInstBundle(SMIR, &*MI, MCI));
      }
    }
  }
}

/// Find all virtual register/stack slot operands in a stackmap and map them
/// to the corresponding IR values
void StackTransformMetadata::mapOpsToIR(const CallInst *IRSM,
                                        const MachineInstr *MISM) {
  RegValsMap::iterator VregIt;
  StackValsMap::iterator SSIt;
  MachineInstr::const_mop_iterator MOit;
  CallInst::const_op_iterator IRit;

  // Initialize new storage location/IR map objects (i.e., for virtual
  // registers & stack slots) for the stackmap
  SMVregs.insert(SMVregsPair(MISM, RegValsMap()));
  SMStackSlots.insert(SMStackSlotPair(MISM, StackValsMap()));

  // Loop over all operands
  for(MOit = std::next(MISM->operands_begin(), 2),
      IRit = std::next(IRSM->op_begin(), 2);
      MOit != MISM->operands_end() && IRit != (IRSM->op_end() - 1);
      MOit++, IRit++) {
    if(MOit->isImm()) { // Map IR values to stack slots
      int FrameIdx = INT32_MAX;
      const Value *IRVal = IRit->get();

      switch(MOit->getImm()) {
      case StackMaps::DirectMemRefOp:
        MOit++;
        assert(MOit->isFI() && "Invalid operand type");
        FrameIdx = MOit->getIndex();
        MOit++;
        break;
      case StackMaps::IndirectMemRefOp:
        MOit++; MOit++;
        assert(MOit->isFI() && "Invalid operand type");
        FrameIdx = MOit->getIndex();
        MOit++;
        break;
      case StackMaps::ConstantOp: MOit++; continue;
      default: llvm_unreachable("Unrecognized stackmap operand type"); break;
      }

      DEBUG(
        IRVal->printAsOperand(dbgs());
        dbgs() << ": in stack slot " << FrameIdx << "\n";
      );

      // Update the list of IR values mapped to the stack slot (multiple IR
      // values may be mapped to a single stack slot).
      if((SSIt = SMStackSlots[MISM].find(FrameIdx)) ==
          SMStackSlots[MISM].end())
        SSIt = SMStackSlots[MISM].insert(StackValsPair(FrameIdx,
                                                       ValueVec())).first;
      SSIt->second.push_back(IRVal);
    } else if(MOit->isReg()) { // Map IR values to virtual registers
      const Value *IRVal = IRit->get();
      unsigned Reg = MOit->getReg();

      assert(IRVal && "Invalid stackmap IR operand");
      assert(TargetRegisterInfo::isVirtualRegister(Reg) &&
             "Should not have been converted to physical registers yet");

      DEBUG(
        IRVal->printAsOperand(dbgs());
        dbgs() << ": in vreg" << TargetRegisterInfo::virtReg2Index(Reg) << "\n";
      );

      // Update the list of IR values mapped to the virtual register (multiple
      // IR values may be mapped to a single virtual register).
      if((VregIt = SMVregs[MISM].find(Reg)) == SMVregs[MISM].end())
        VregIt = SMVregs[MISM].insert(RegValsPair(Reg, ValueVec())).first;
      VregIt->second.push_back(IRVal);
    } else {
      llvm_unreachable("Unrecognized stackmap operand type.");
    }
  }
  assert(IRit == (IRSM->op_end() - 1) && "Did not search all stackmap operands");
}

/// Analyze a machine instruction to see if a value is getting restored from a
/// spill location.
StackTransformMetadata::SpillLoc *
StackTransformMetadata::getSpillLocation(const MachineInstr *MI) const {
  unsigned SrcVreg = 0;
  unsigned DefVreg = 0;
  int SS;

  assert(MI && "Invalid machine instruction");

  // Is it a copy from another register?
  if(MI->isCopy()) {
    for(unsigned i = 0, e = MI->getNumOperands(); i != e; i++) {
      const MachineOperand &MO = MI->getOperand(i);
      if(MO.isReg()) {
        if(MO.isDef()) DefVreg = MO.getReg();
        else SrcVreg = MO.getReg();
      }
    }

    if(TargetRegisterInfo::isVirtualRegister(SrcVreg) &&
       TargetRegisterInfo::isVirtualRegister(DefVreg))
      return new RegSpillLoc(DefVreg, SrcVreg);
  }

  // Is it a load from the stack?
  if((DefVreg = TII->isLoadFromStackSlot(MI, SS)) &&
     TargetRegisterInfo::isVirtualRegister(DefVreg))
    return new StackLoadLoc(DefVreg, SS);

  // Is it a store to the stack?
  if((SrcVreg = TII->isStoreToStackSlot(MI, SS)) &&
     TargetRegisterInfo::isVirtualRegister(SrcVreg))
    return new StackStoreLoc(SrcVreg, SS);

  // Something else
  // TODO dump MI in debug?  May pollute output too much...
  return nullptr;
}

/// Return whether or not a machine instruction is defined within a range of
/// machine instructions, inclusive
const MachineInstr *
StackTransformMetadata::definedInRange(const MachineInstr *Start,
                                       const MachineInstr *End,
                                       unsigned Vreg) const {
  assert(Start && End && "Invalid machine instruction");
  assert(TargetRegisterInfo::isVirtualRegister(Vreg) && "Invalid register");
  assert(Start->getParent() == End->getParent() &&
         "Range must be contained within the same basic block");

  // Search over all the vreg's definitions
  for(MachineRegisterInfo::def_instr_iterator DI = MRI->def_instr_begin(Vreg),
      DIE = MRI->def_instr_end();
      DI != DIE; DI++) {
    const MachineInstr *MI = &*DI;

    // Shortcut -- are we the starting or ending instruction of the range?
    if(MI == Start || MI == End) return MI;

    // Search the range of machine instructions
    for(const MachineInstr *Cur = Start->getNextNode();
        Cur != End && Cur != nullptr;
        Cur = Cur->getNextNode())
      if(Cur == MI)
        return MI;
  }

  // None of the definitions were within the range
  return nullptr;
}

/// Unwind live value movement in the series of instructions between a call and
/// a stackmap
void StackTransformMetadata::unwindToCall(const SMInstBundle &SM) {
  unsigned PhysReg;
  const CallInst *IRSM = getIRSM(SM);
  const MachineInstr *MISM = getMISM(SM), *MICall = getMICall(SM), *Cur;
  SpillLoc *Loc, *DefChain;
  StackLoadLoc *SLL;
  RegSpillLoc *RSL;
  RegValsMap &Vregs = SMVregs[MISM];
  RegValsMap::iterator vregIt;
  StackValsMap &SSlots = SMStackSlots[MISM];

  // Walk from call to stackmap
  for(Cur = MICall->getNextNode(); Cur != MISM; Cur = Cur->getNextNode()) {
    Loc = getSpillLocation(Cur);
    if(!Loc) continue;
    if((vregIt = Vregs.find(Loc->Vreg)) == Vregs.end()) {
      delete Loc;
      continue;
    }

    // We may be unwinding a chain of copies to find the original spill
    // location.  There are 2 halting conditions for the while-loop below:
    //
    //  1. Restoring the vreg from a spill slot
    //  2. Restoring the vreg from a callee-saved register whose definition is
    //     NOT between the call and the stackmap
    //
    // The second condition is qualified by the location of the definition of
    // the callee-saved vreg because the backend can generate really
    // unoptimized code, e.g., on x86 with callee-saved register %rbx:
    //
    //    callq ...
    //    mov -0x10(%rbp), %rbx
    //    mov %rbx, %rcx
    //    <stackmap machine instruction>
    DefChain = Loc->copy();
    while(DefChain && DefChain->getType() == SpillLoc::VREG) {
      RSL = (RegSpillLoc *)DefChain;
      const MachineInstr *Def = definedInRange(MICall, MISM, RSL->SrcVreg);

      // Are we in a callee-saved register defined outside of the range of
      // instructions between the call and the stackmap?
      if(!Def && !MF->isCallerSaved(VRM->getPhys(RSL->SrcVreg)))
          break;

      assert(Def && "Invalid virtual register definition");
      DefChain = getSpillLocation(Def);
      delete RSL;
    }

    if(!DefChain) {
      DEBUG(
        dbgs() << "WARNING: couldn't resolve definition chain for vreg"
               << TargetRegisterInfo::virtReg2Index(Loc->Vreg) << "\n"
      );
      delete Loc;
      continue;
    }

    switch(DefChain->getType()) {
    case SpillLoc::VREG:
      RSL = (RegSpillLoc *)DefChain;
      PhysReg = VRM->getPhys(RSL->SrcVreg);
      assert(PhysReg != VirtRegMap::NO_PHYS_REG && "Invalid register");
      assert(!MF->isCallerSaved(PhysReg) && "Invalid register");

      for(size_t sz = 0; sz < vregIt->second.size(); sz++) {
        DEBUG(
          vregIt->second[sz]->printAsOperand(dbgs());
          dbgs() << ": spilled to callee-saved register "
                 << PrintReg(PhysReg, &VRM->getTargetRegInfo()) << " (vreg"
                 << TargetRegisterInfo::virtReg2Index(RSL->SrcVreg) << ")\n";
        );
        MF->addSMOpLocation(IRSM, vregIt->second[sz],
                            MachineLiveReg(PhysReg, true));
      }

      // We need to check the vreg at the head of the copy chain for spill
      // locations in addition to the vreg in the stackmap
      Vregs[RSL->SrcVreg] = vregIt->second;
      break;
    case SpillLoc::STACK_LOAD:
      SLL = (StackLoadLoc *)DefChain;
      assert(SLL->StackSlot != VirtRegMap::NO_STACK_SLOT && "Invalid stack slot");

      for(size_t sz = 0; sz < vregIt->second.size(); sz++) {
        DEBUG(
          vregIt->second[sz]->printAsOperand(dbgs());
          dbgs() << ": spilled to stack slot " << SLL->StackSlot << "\n";
        );
        MF->addSMOpLocation(IRSM, vregIt->second[sz],
                            MachineLiveStackSlot(SLL->StackSlot, true));
      }

      // The vreg is loaded from the stack after the call instruction; the
      // value also lives on the stack, so add it to the stack slot list.
      SSlots[SLL->StackSlot] = vregIt->second;
      break;
    case SpillLoc::STACK_STORE:
    case SpillLoc::NONE:
    default:
      DEBUG(
        dbgs() << "WARNING: ignoring machine instruction:\n";
        Cur->dump();
      );
      break;
    }

    delete Loc;
    delete DefChain;
  }
}

/// Is a virtual register live across the machine instruction?
bool StackTransformMetadata::isVregLiveAcrossInstr(unsigned Vreg,
                                                   const MachineInstr *MI) {
  if(LI->hasInterval(Vreg)) {
    const LiveInterval &TheLI = LI->getInterval(Vreg);
    SlotIndex InstrIdx = Indexes->getInstructionIndex(MI);
    LiveInterval::const_iterator Seg = TheLI.find(InstrIdx);
    if(Seg != TheLI.end() && Seg->contains(InstrIdx))
      return true;
  }
  return false;
}


/// Is a stack slot live across the machine instruction?
bool StackTransformMetadata::isSSLiveAcrossInstr(int SS,
                                                 const MachineInstr *MI) {
  if(LS->hasInterval(SS)) {
    const LiveInterval &TheLI = LS->getInterval(SS);
    SlotIndex InstrIdx = Indexes->getInstructionIndex(MI);
    LiveInterval::const_iterator Seg = TheLI.find(InstrIdx);
    if(Seg != TheLI.end() && Seg->contains(InstrIdx))
      return true;
  }
  return false;
}

/// Find if a virtual register is backed by a stack slot
int StackTransformMetadata::findBackingStackSlots(unsigned Vreg,
                                                  const MachineInstr *SM) {
  int ret = VirtRegMap::NO_STACK_SLOT;
  SpillLoc *Loc;
  StackSpillLoc *SSL;

  for(MachineRegisterInfo::reg_instr_iterator MI = MRI->reg_instr_begin(Vreg),
      MIE = MRI->reg_instr_end();
      MI != MIE; MI++) {
    Loc = getSpillLocation(&*MI);
    if(!Loc) continue;

    switch(Loc->getType()) {
    case SpillLoc::STACK_LOAD:
    case SpillLoc::STACK_STORE:
      // Found a stack load/store associated with the vreg, check if the stack
      // slot is live at the same time as the stackmap
      SSL = (StackSpillLoc*)Loc;
      if(isSSLiveAcrossInstr(SSL->StackSlot, SM)) ret = SSL->StackSlot;
      else {
        DEBUG(
          dbgs() << "WARNING: ignoring when searching for backing slot:\n";
          MI->dump();
        );
      }
      break;
    case SpillLoc::VREG:
    case SpillLoc::NONE:
    default: break;
    }

    delete Loc;
  }

  return ret;
}

/// Find alternate storage locations for stackmap operands
void StackTransformMetadata::findAlternateOpLocs()
{
  RegValsMap::iterator vregIt, vregEnd;

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++) {
    const CallInst *IRSM = getIRSM(*S);
    const MachineInstr *MISM = getMISM(*S);

    DEBUG(
      const MachineInstr *MICall = getMICall(*S);
      dbgs() << "\nStackmap " << MISM->getOperand(0).getImm() << ":\n";
      MISM->dump();
      dbgs() << "\nInstructions between call and stackmap:\n";
      while(MICall != MISM) {
        MICall->dump();
        MICall = MICall->getNextNode();
      }
      dbgs() << '\n';
    );

    // Get all virtual register/stack slot operands & their associated IR
    // values
    mapOpsToIR(IRSM, MISM);

    // TODO the rest of the logic in the loop needs to be replaced with a
    // forward/backward sweep to find equivalent values
    //
    // - For all locations in the map:
    //    1. Do a forward sweep through the vreg/stack slot's uses
    //    2. Do a backward sweep the defining instructions
    //      - Does this apply to stack slots?
    //
    // - Add duplicate locations for each location discovered that:
    //    1. Is equivalent, i.e., is a copy from another vreg or is a
    //       load from/store to a stack slot
    //    2. Is live either across the stackmap or call machine instruction

    // Stackmap machine instructions may not appear directly after function
    // call, and stackmap operands may be created during this interlude.  Walk
    // from the call to the stackmap, reversing stackmap operand movement.
    unwindToCall(*S);

    // Walk through remaining virtual register operands to see if they have a
    // backing stack slot
    for(vregIt = SMVregs[MISM].begin(), vregEnd = SMVregs[MISM].end();
        vregIt != vregEnd;
        vregIt++) {
      int StackSlot = findBackingStackSlots(vregIt->first, MISM);
      if(StackSlot != VirtRegMap::NO_STACK_SLOT) {
        for(size_t sz = 0; sz < vregIt->second.size(); sz++) {
          DEBUG(
            vregIt->second[sz]->printAsOperand(dbgs());
            dbgs() << ": backed by stack slot " << StackSlot << "\n"
          );
          MF->addSMOpLocation(IRSM, vregIt->second[sz],
                              MachineLiveStackSlot(StackSlot, true));

        }

        // Record handling for the backing stack slot
        SMStackSlots[MISM][StackSlot] = vregIt->second;
      }
    }
  }
}

MachineConstant *
StackTransformMetadata::getTargetValue(const MachineInstr *MI) const {
  if(MI->isMoveImmediate()) {
    unsigned Size = 8;
    uint64_t Value = UINT64_MAX;
    for(unsigned i = 0, e = MI->getNumOperands(); i < e; i++) {
      const MachineOperand &MO = MI->getOperand(i);
      if(MO.isImm()) Value = MO.getImm();
      if(MO.isFPImm()) {
        // We need to encode the bits exactly as they are to represent the
        // double, so switch types and read relevant info
        APInt Bits(MO.getFPImm()->getValueAPF().bitcastToAPInt());
        Size = Bits.getBitWidth() / 8;
        Value = Bits.getZExtValue();
      }
    }
    return new MachineImmediate(Size, Value);
  }
  else {
    // TODO there's not any generic interface for detecting instructions like
    // MOVaddr, do we need an architecture-specific instruction filter?
    std::string Symbol("");
    for(unsigned i = 0, e = MI->getNumOperands(); i < e; i++) {
      const MachineOperand &MO = MI->getOperand(i);
      if(MO.isMCSymbol()) Symbol = MO.getMCSymbol()->getName();
      else if(MO.isGlobal()) Symbol = MO.getGlobal()->getName();
      else if(MO.isSymbol()) Symbol = MO.getSymbolName();
    }
    if(Symbol != "") return new MachineSymbol(Symbol);
  }

  return nullptr;
}

/// Find architecture-specific live values added by the backend
void StackTransformMetadata::findArchSpecificLiveVals() {
  DEBUG(
    dbgs() << "\n*** Detecting live values inserted by the backend ***\n\n";
  );

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++)
  {
    const MachineInstr *MISM = getMISM(*S);
    const MachineInstr *MICall = getMICall(*S);
    const CallInst *IRSM = getIRSM(*S);
    const MachineFrameInfo *MFI = MF->getFrameInfo();
    RegValsMap &CurVregs = SMVregs[MISM];
    StackValsMap &CurSS = SMStackSlots[MISM];

    DEBUG(
      MISM->dump();
      dbgs() << "  -> Call instruction SlotIndex ";
      Indexes->getInstructionIndex(MICall).print(dbgs());
      dbgs() << ", searching vregs 0 -> " << MRI->getNumVirtRegs()
             << " and stack slots " << MFI->getObjectIndexBegin() << " -> "
             << MFI->getObjectIndexEnd() << "\n";
    );

    // Search for virtual registers not handled by the stackmap
    for(unsigned i = 0; i < MRI->getNumVirtRegs(); i++) {
      unsigned Vreg = TargetRegisterInfo::index2VirtReg(i);
      MachineConstant *MC;
      MachineLiveReg MLR(0, false);

      // Detect virtual registers live across but not included in the stackmap
      if(VRM->hasPhys(Vreg) && isVregLiveAcrossInstr(Vreg, MICall) &&
         CurVregs.find(Vreg) == CurVregs.end()) {
        DEBUG(dbgs() << "    + vreg" << i
                     << " is live in register but not in stackmap\n";);

        // TODO need to handle when >1 definition
        if(!MRI->hasOneDef(Vreg)) {
          DEBUG( dbgs() << "More than one definition for vreg");
          MC = nullptr;
        }
        else {
          for(auto def = MRI->def_instr_begin(Vreg), end = MRI->def_instr_end();
              def != end; def++) {
            if((MC = getTargetValue(&*def)) != nullptr) {
              DEBUG(dbgs() << "      Defining instruction: "; def->dump(););
              break;
            }
          }
        }

        if(MC) {
          MLR.setReg(VRM->getPhys(Vreg));
          MF->addSMArchSpecificLocation(IRSM, MLR, *MC);
          DEBUG(dbgs() << "      Value: " << MC->toString() << "\n";);
          delete MC;
        }
        else {
          DEBUG(
            dbgs() << "      WARNING: could not get defining value.  "
                      "Defining instructions:\n";
            for(auto def = MRI->def_instr_begin(Vreg), end = MRI->def_instr_end();
                def != end; def++) {
              dbgs() << "      ";
              def->dump();
            }
          );
          DiagnosticInfoOptimizationFailure DI(
            *IRSM->getParent()->getParent(),
            IRSM->getDebugLoc(),
            "Unhandled architecture-specific register "
            "for stack transformation");
          MF->getFunction()->getContext().diagnose(DI);
        }
      }
      // Detect virtual registers mapped to stack slots not in stackmap
      else if(VRM->getStackSlot(Vreg) != VirtRegMap::NO_STACK_SLOT &&
              isVregLiveAcrossInstr(Vreg, MICall) &&
              CurVregs.find(Vreg) == CurVregs.end()) {
        // Note: this path is a little weird in that we can't detect if this
        // vreg is actually used by the value returned from VRM->getStackSlot.
        // Manually check by seeing if there are any definitions.
        auto def = MRI->def_instr_begin(Vreg), end = MRI->def_instr_end();
        if(def != end) {
          dbgs() << "    + vreg" << i
                 << " is live in stack slot but not in stackmap\n";
          DEBUG(
            for(; def != end; def++) {
              dbgs() << "    ";
              def->dump();
            }
          );
          DiagnosticInfoOptimizationFailure DI(
            *IRSM->getParent()->getParent(),
            IRSM->getDebugLoc(),
            "Unhandled architecture-specific stack slot/register "
            "for stack transformation");
          MF->getFunction()->getContext().diagnose(DI);
        }
      }
    }

    // Search for stack slots not handled by the stackmap
    // TODO handle function arguments on the stack (negative stack slots)
    for(int SS = MFI->getObjectIndexBegin(), e = MFI->getObjectIndexEnd();
        SS < e; SS++) {
      if(!MFI->isDeadObjectIndex(SS) &&
         isSSLiveAcrossInstr(SS, MICall) && CurSS.find(SS) == CurSS.end()) {
        DEBUG(dbgs() << "    + stack slot " << SS
                     << " is live but not in stackmap\n";);
        // TODO add arch-specific stack slot information to machine function
        // TODO does this imply an alloca that wasn't captured in the stackmap?
        // I think this is a live value analysis bug...
        DiagnosticInfoOptimizationFailure DI(
          *IRSM->getParent()->getParent(),
          IRSM->getDebugLoc(),
          "Unhandled architecture-specific stack slot "
          "for stack transformation");
        MF->getFunction()->getContext().diagnose(DI);
      }
    }

    DEBUG(dbgs() << "\n";);
  }
}

