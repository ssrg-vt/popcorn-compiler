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
// Note: the dataflow analysis in this implementation assumes the ISA does not
// allow memory-to-memory copies.
//
//===----------------------------------------------------------------------===//

#include <queue>
#include "llvm/CodeGen/LiveIntervalAnalysis.h"
#include "llvm/CodeGen/LiveStackAnalysis.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/StackTransformTypes.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetValues.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "stacktransform"

static cl::opt<bool>
NoWarnings("no-sm-warn", cl::desc("Don't issue warnings about stackmaps"),
           cl::init(false), cl::Hidden);

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

  /* Types */

  /// A bundle tying together a stackmap IR instruction, the generated stackmap
  /// machine instruction and the call machine instruction that caused the
  /// stackmap to be emitted in the IR, respectively
  typedef std::tuple<const CallInst *,
                     MachineInstr *,
                     const MachineInstr *> SMInstBundle;

  /// Getters for individual elements of instruction bundles
  static inline const
  CallInst *getIRSM(const SMInstBundle &B) { return std::get<0>(B); }
  static inline
  MachineInstr *getMISM(const SMInstBundle &B) { return std::get<1>(B); }
  static inline const
  MachineInstr *getMICall(const SMInstBundle &B) { return std::get<2>(B); }

  /// A vector of IR values.  Used when mapping from registers/stack slots to
  /// IR values.
  typedef SmallVector<const Value *, 4> ValueVec;
  typedef std::shared_ptr<ValueVec> ValueVecPtr;

  /// Mapping between virtual registers and IR operands
  typedef std::pair<unsigned, ValueVecPtr> RegValsPair;
  typedef std::map<unsigned, ValueVecPtr> RegValsMap;

  /// Mapping between stackmaps and virtual registers referenced by the stackmap
  typedef std::pair<const MachineInstr *, RegValsMap> SMRegPair;
  typedef std::map<const MachineInstr *, RegValsMap> SMRegMap;

  /// Mapping between stack slots and IR operands
  typedef std::pair<int, ValueVecPtr> StackValsPair;
  typedef std::map<int, ValueVecPtr> StackValsMap;

  /// Mapping between stackmaps and stack slots referenced by the stackmap
  typedef std::pair<const MachineInstr *, StackValsMap> SMStackSlotPair;
  typedef std::map<const MachineInstr *, StackValsMap> SMStackSlotMap;

  /// A value's spill location
  class CopyLoc {
  public:
    enum Type { NONE, VREG, STACK_LOAD, STACK_STORE };
    unsigned Vreg;
    const MachineInstr *Instr;
    CopyLoc() : Vreg(VirtRegMap::NO_PHYS_REG), Instr(nullptr) {}
    CopyLoc(unsigned Vreg, const MachineInstr *Instr) :
      Vreg(Vreg), Instr(Instr) {}
    virtual CopyLoc *copy() const = 0;
    virtual ~CopyLoc() {}
    virtual Type getType() const = 0;
  };
  typedef std::shared_ptr<CopyLoc> CopyLocPtr;

  /// A spill to a stack slot
  class StackCopyLoc : public CopyLoc {
  public:
    int StackSlot;
    StackCopyLoc() : StackSlot(VirtRegMap::NO_STACK_SLOT) {}
    StackCopyLoc(unsigned Vreg, int StackSlot, const MachineInstr *Instr) :
      CopyLoc(Vreg, Instr), StackSlot(StackSlot) {}
    virtual CopyLoc *copy() const = 0;
    virtual Type getType() const = 0;
  };

  /// A load from a stack slot
  class StackLoadLoc : public StackCopyLoc {
  public:
    StackLoadLoc() {}
    StackLoadLoc(unsigned Vreg, int StackSlot, const MachineInstr *Instr) :
      StackCopyLoc(Vreg, StackSlot, Instr) {}
    virtual CopyLoc *copy() const
    { return new StackLoadLoc(Vreg, StackSlot, Instr); }
    virtual Type getType() const { return CopyLoc::STACK_LOAD; }
  };

  /// A store to a stack slot
  class StackStoreLoc : public StackCopyLoc {
  public:
    StackStoreLoc() {}
    StackStoreLoc(unsigned Vreg, int StackSlot, const MachineInstr *Instr) :
      StackCopyLoc(Vreg, StackSlot, Instr) {}
    virtual CopyLoc *copy() const
    { return new StackStoreLoc(Vreg, StackSlot, Instr); }
    virtual Type getType() const { return CopyLoc::STACK_STORE; }
  };

  /// A spill to another register
  class RegCopyLoc : public CopyLoc {
  public:
    unsigned SrcVreg;
    RegCopyLoc() : SrcVreg(VirtRegMap::NO_PHYS_REG) {}
    RegCopyLoc(unsigned DefVreg, unsigned SrcVreg, const MachineInstr *Instr) :
      CopyLoc(DefVreg, Instr), SrcVreg(SrcVreg) {}
    virtual CopyLoc *copy() const
    { return new RegCopyLoc(Vreg, SrcVreg, Instr); }
    virtual Type getType() const { return CopyLoc::VREG; }
  };

  /// Mapping between stack slots and copy locations (e.g., load from or store
  /// to the stack slot)
  typedef SmallVector<CopyLocPtr, 8> CopyLocVec;
  typedef std::shared_ptr<CopyLocVec> CopyLocVecPtr;
  typedef std::pair<int, CopyLocVecPtr> StackSlotCopyPair;
  typedef std::map<int, CopyLocVecPtr> StackSlotCopies;

  /// A work item to analyze in dataflow analysis.  Can selectively enable
  /// traversing definitions.
  struct WorkItem {
    WorkItem() : Vreg(0), TraverseDefs(false) {}
    WorkItem(unsigned Vreg, bool TraverseDefs)
      : Vreg(Vreg), TraverseDefs(TraverseDefs) {}

    unsigned Vreg;
    bool TraverseDefs;
  };

  /* Data */

  /// LLVM-provided analysis & metadata
  MachineFunction *MF;
  const MachineFrameInfo *MFI;
  const MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
  const TargetRegisterInfo *TRI;
  const TargetValues *TVG;
  LiveIntervals *LI;
  const LiveStacks *LS;
  const SlotIndexes *Indexes;
  const VirtRegMap *VRM;

  /// Stackmap/call instructions, mapping of virtual registers & stack slots to
  /// IR values, stack slots used in the function, list of instructions that
  /// copy to/from the stack
  SmallVector<SMInstBundle, 32> SM;
  SMRegMap SMRegs;
  SMStackSlotMap SMStackSlots;
  SmallSet<int, 32> UsedSS;
  StackSlotCopies SSCopies;

  /* Functions */

  // Reset the analysis for a new function
  void reset() {
    SM.clear();
    SMRegs.clear();
    SMStackSlots.clear();
    UsedSS.clear();
    SSCopies.clear();
  }

  /// Print information about a virtual register and it's associated IR value
  void dumpReg(unsigned Reg, const Value *IRVal) const;

  /// Print information about a stack slot and it's associated IR value
  void dumpStackSlot(int SS, const Value *IRVal) const;

  /// Analyze a machine instruction to see if a value is getting copied from
  /// another location such as a stack slot or register.
  CopyLocPtr getCopyLocation(const MachineInstr *MI) const;

  /// Gather stackmap machine instructions, the IR instructions which generated
  /// the stackmaps, and their associated call machine instructions.  Also,
  /// find copies to/from stack slots (since there's no other mechanism to
  /// find/traverse them).
  void findStackmapsAndStackSlotCopies();

  /// Find all virtual register/stack slot operands in a stackmap and collect
  /// virtual register/stack slot <-> IR value mappings
  void mapOpsToIR(const CallInst *IRSM, const MachineInstr *MISM);

  /// Extend the live range for a register to include an instruction.
  void updateRegisterLiveInterval(MachineOperand &Src,
                                  const MachineInstr *Inst);

  /// Rather than modifying the backend machinery to prevent hoisting code
  /// between the stackmap and call site, unwind instructions in order to get
  /// real live value locations at the function call.
  bool unwindToCallSite(MachineInstr *SM, const MachineInstr *Call);

  /// Is a virtual register live across the machine instruction?
  /// Note: returns false if the MI is the last instruction for which the
  /// virtual register is alive
  bool isVregLiveAcrossInstr(unsigned Vreg, const MachineInstr *MI) const;

  /// Is a stack slot live across the machine instruction?
  /// Note: returns false if the MI is the last instruction for which the stack
  /// slot is alive
  bool isSSLiveAcrossInstr(int SS, const MachineInstr *MI) const;

  /// Add duplicate location information for a virtual register.  Return true
  /// if metadata was added, or false if the virtual register is not live
  /// across the call instruction/stackmap.
  bool addVregMetadata(unsigned Vreg,
                       ValueVecPtr IRVals,
                       const SMInstBundle &SM);

  /// Add duplicate location information for a stack slot.  Return true if
  /// metadata was added, or false if the stack slot is not live across the
  /// call instruction/stackmap.
  bool addSSMetadata(int SS, ValueVecPtr IRVals, const SMInstBundle &SM);

  /// Search stack slot copies for additional virtual registers which are live
  /// across the stackmap.  Will check to see if the copy instructions have
  /// already been visited, and if appropriate, will add virtual registers to
  /// work queue.
  void inline
  searchStackSlotCopies(int SS,
                        ValueVecPtr IRVals,
                        const SMInstBundle &SM,
                        SmallPtrSet<const MachineInstr *, 32> &Visited,
                        std::queue<WorkItem> &work,
                        bool TraverseDefs);

  /// Find all alternate locations for virtual registers in a stackmap, and add
  /// them to the metadata to be generated.
  void findAlternateVregLocs(const SMInstBundle &SM);

  /// Find stackmap operands that have been spilled to alternate locations
  bool findAlternateOpLocs();

  /// Ensure virtual registers used to generate architecture-specific values
  /// are handled by the stackmap & convert to physical registers
  void sanitizeVregs(MachineLiveValPtr &LV, const MachineInstr *SM) const;

  /// Find architecture-specific live values added by the backend
  void findArchSpecificLiveVals();

  /// Find locations of arguments marshaled into registers and onto the stack
  void findMarshaledArguments();

  /// Warn about unhandled registers & stack slots
  void warnUnhandled() const;

public:
  static char ID;
  static const std::string SMName;

  StackTransformMetadata() : MachineFunctionPass(ID) {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const override;

  virtual bool runOnMachineFunction(MachineFunction&) override;

};

} // end anonymous namespace

char &llvm::StackTransformMetadataID = StackTransformMetadata::ID;
const std::string StackTransformMetadata::SMName("llvm.experimental.stackmap");

INITIALIZE_PASS_BEGIN(StackTransformMetadata, "stacktransformmetadata",
  "Gather stack transformation metadata", false, false)
INITIALIZE_PASS_DEPENDENCY(SlotIndexes)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_DEPENDENCY(LiveStacks)
INITIALIZE_PASS_DEPENDENCY(VirtRegMap)
INITIALIZE_PASS_END(StackTransformMetadata, "stacktransformmetadata",
  "Gather stack transformation metadata", false, false)

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
  bool Changed = false;

  if(fn.getFrameInfo()->hasStackMap()) {
    MF = &fn;
    MFI = MF->getFrameInfo();
    MRI = &MF->getRegInfo();
    TII = MF->getSubtarget().getInstrInfo();
    TRI = MF->getSubtarget().getRegisterInfo();
    TVG = MF->getSubtarget().getValues();
    Indexes = &getAnalysis<SlotIndexes>();
    LI = &getAnalysis<LiveIntervals>();
    LS = &getAnalysis<LiveStacks>();
    VRM = &getAnalysis<VirtRegMap>();
    reset();

    DEBUG(
      dbgs() << "\n********** STACK TRANSFORMATION METADATA **********\n"
             << "********** Function: " << MF->getName() << "\n";
      VRM->dump();
    );

    findStackmapsAndStackSlotCopies();
    Changed = findAlternateOpLocs();
    findArchSpecificLiveVals();
    findMarshaledArguments();
    if(!NoWarnings) warnUnhandled();
  }

  return Changed;
}

/// Print information about a virtual register and it's associated IR value
void StackTransformMetadata::dumpReg(unsigned Reg, const Value *IRVal) const {
  if(IRVal) IRVal->printAsOperand(dbgs());
  if(TargetRegisterInfo::isPhysicalRegister(Reg))
    dbgs() << ": in register " << PrintReg(Reg, TRI);
  else {
    assert(VRM->hasPhys(Reg) && "Invalid virtual register");
    unsigned Phys = VRM->getPhys(Reg);
    dbgs() << ": in register " << PrintReg(Phys, TRI)
           << " (vreg " << TargetRegisterInfo::virtReg2Index(Reg) << ")";
  }
  dbgs() << "\n";
}

/// Print information about a stack slot and it's associated IR value
void StackTransformMetadata::dumpStackSlot(int SS, const Value *IRVal) const {
  assert(!MFI->isDeadObjectIndex(SS) && "Invalid stack slot");
  if(IRVal) IRVal->printAsOperand(dbgs());
  dbgs() << ": in stack slot " << SS << " (size: " << MFI->getObjectSize(SS)
         << ")\n";
}

/// Analyze a machine instruction to see if a value is getting copied from
/// another location such as a stack slot or register.
StackTransformMetadata::CopyLocPtr
StackTransformMetadata::getCopyLocation(const MachineInstr *MI) const {
  unsigned SrcVreg = 0;
  unsigned DefVreg = 0;
  int SS;

  assert(MI && "Invalid machine instruction");

  // Is it a copy from another register?
  if(MI->isCopyLike()) {
    for(unsigned i = 0, e = MI->getNumOperands(); i != e; i++) {
      const MachineOperand &MO = MI->getOperand(i);
      if(MO.isReg()) {
        if(MO.isDef()) DefVreg = MO.getReg();
        else SrcVreg = MO.getReg();
      }
    }

    // TODO does it have to be a virtual register or can it be a physical one?
    // Liveness analysis seems to apply only to virtual registers.
    if(TargetRegisterInfo::isVirtualRegister(SrcVreg) &&
       TargetRegisterInfo::isVirtualRegister(DefVreg))
      return CopyLocPtr(new RegCopyLoc(DefVreg, SrcVreg, MI));
  }

  // Is it a load from the stack?
  if((DefVreg = TII->isLoadFromStackSlot(MI, SS)) &&
     TargetRegisterInfo::isVirtualRegister(DefVreg))
    return CopyLocPtr(new StackLoadLoc(DefVreg, SS, MI));

  // Is it a store to the stack?
  if((SrcVreg = TII->isStoreToStackSlot(MI, SS)) &&
     TargetRegisterInfo::isVirtualRegister(SrcVreg))
    return CopyLocPtr(new StackStoreLoc(SrcVreg, SS, MI));

  // A non-copylike instruction
  return CopyLocPtr(nullptr);
}

/// Gather stackmap machine instructions, the IR instructions which generated
/// the stackmaps, and their associated call machine instructions.  Also,
/// find copies to/from stack slots (since there's no other mechanism to
/// find/traverse them).
void StackTransformMetadata::findStackmapsAndStackSlotCopies() {
  for(auto MBB = MF->begin(), MBBE = MF->end(); MBB != MBBE; MBB++) {
    for(auto MI = MBB->instr_begin(), ME = MBB->instr_end(); MI != ME; MI++) {
      if(MI->getOpcode() == TargetOpcode::STACKMAP) {
        // Find the stackmap IR instruction
        assert(MI->getOperand(0).isImm() && "Invalid stackmap ID");
        int64_t ID = MI->getOperand(0).getImm();
        const BasicBlock *BB = MI->getParent()->getBasicBlock();
        const CallInst *IRSM = nullptr;
        for(auto I = BB->begin(), IE = BB->end(); I != IE; I++)
        {
          const IntrinsicInst *II;
          if((II = dyn_cast<IntrinsicInst>(&*I)) &&
             II->getCalledFunction()->getName() == SMName &&
             cast<ConstantInt>(II->getArgOperand(0))->getSExtValue() == ID) {
            IRSM = cast<CallInst>(II);
            break;
          }
        }
        assert(IRSM && "Could not find stackmap IR instruction");

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
          DEBUG(dbgs() << "NOTE: stackmap " << ID << " ";
                IRSM->print(dbgs());
                dbgs() << ": could not find associated call instruction "
                          "(lowered to a native instruction?)\n");
          continue;
        }

        SM.push_back(SMInstBundle(IRSM, &*MI, MCI));
      }
      else {
        // Record all stack slots that are actually used.  Note that this is
        // necessary because analysis maintained in MachineFrameInfo/LiveStacks
        // may denote stack slots as live even though register allocation
        // actually all references to them.
        const PseudoSourceValue *PSV;
        const FixedStackPseudoSourceValue *FI;
        for(auto MemOp : MI->memoperands()) {
          PSV = MemOp->getPseudoValue();
          if(PSV && PSV->isFixed) {
            FI = cast<FixedStackPseudoSourceValue>(PSV);
            UsedSS.insert(FI->getFrameIndex());
          }
        }

        // See if instruction copies to/from stack slot
        StackSlotCopies::iterator it;
        CopyLocPtr loc;
        if(!(loc = getCopyLocation(&*MI))) continue;
        enum CopyLoc::Type type = loc->getType();
        if(type == CopyLoc::STACK_LOAD || type == CopyLoc::STACK_STORE) {
          StackCopyLoc *SCL = (StackCopyLoc *)loc.get();
          if((it = SSCopies.find(SCL->StackSlot)) == SSCopies.end())
            it = SSCopies.emplace(SCL->StackSlot,
                                  CopyLocVecPtr(new CopyLocVec)).first;
          it->second->push_back(loc);
        }
      }
    }
  }

  DEBUG(
    dbgs() << "\n*** Stack slot copies ***\n\n";
    for(auto SC = SSCopies.begin(), SCe = SSCopies.end(); SC != SCe; SC++) {
      dbgs() << "Stack slot " << SC->first << ":\n";
      for(size_t i = 0, e = SC->second->size(); i < e; i++) {
        (*SC->second)[i]->Instr->dump();
      }
    }
  );
}

/// Find all virtual register/stack slot operands in a stackmap and collect
/// virtual register/stack slot <-> IR value mappings
void StackTransformMetadata::mapOpsToIR(const CallInst *IRSM,
                                        const MachineInstr *MISM) {
  RegValsMap::iterator RegIt;
  StackValsMap::iterator SSIt;
  MachineInstr::const_mop_iterator MOit;
  int64_t SMID = cast<ConstantInt>(IRSM->getArgOperand(0))->getSExtValue();
  unsigned NumMO;

  // Initialize new storage location/IR map objects (i.e., for virtual
  // registers & stack slots) for the stackmap
  SMRegs.emplace(MISM, RegValsMap());
  SMStackSlots.emplace(MISM, StackValsMap());

  // Loop over all operands
  MOit = std::next(MISM->operands_begin(), 2);
  for(size_t i = 2; i < IRSM->getNumArgOperands(); i++) {
    const Value *IRVal = IRSM->getArgOperand(i);
    assert(IRVal && "Invalid stackmap IR operand");

    // Legalization may have changed how many machine operands map to the IR
    // value.  Loop over all relevant machine operands.
    NumMO = MF->getNumLegalizedOps(SMID, MOit - MISM->operands_begin());
    for(size_t j = 0; j < NumMO; j++) {
      if(MOit->isImm()) { // Map IR values to stack slots
        int FrameIdx = INT32_MAX;
        switch(MOit->getImm()) {
        case StackMaps::DirectMemRefOp:
          MOit++;
          assert(MOit->isFI() && "Invalid operand type");
          FrameIdx = MOit->getIndex();
          MOit = std::next(MOit, 2);
          break;
        case StackMaps::IndirectMemRefOp:
          MOit = std::next(MOit, 2);
          assert(MOit->isFI() && "Invalid operand type");
          FrameIdx = MOit->getIndex();
          MOit = std::next(MOit, 2);
          break;
        case StackMaps::ConstantOp: MOit = std::next(MOit, 2); continue;
        default: llvm_unreachable("Unrecognized stackmap operand type"); break;
        }

        assert(MFI->getObjectIndexBegin() <= FrameIdx &&
               FrameIdx <= MFI->getObjectIndexEnd() && "Invalid frame index");
        assert(!MFI->isDeadObjectIndex(FrameIdx) && "Dead frame index");
        DEBUG(dumpStackSlot(FrameIdx, IRVal););

        // Update the list of IR values mapped to the stack slot (multiple IR
        // values may be mapped to a single stack slot).
        SSIt = SMStackSlots[MISM].find(FrameIdx);
        if(SSIt == SMStackSlots[MISM].end())
          SSIt = SMStackSlots[MISM].emplace(FrameIdx,
                                            ValueVecPtr(new ValueVec)).first;
        SSIt->second->push_back(IRVal);
      }
      else if(MOit->isReg()) { // Map IR values to virtual registers
        unsigned Reg = MOit->getReg();
        MOit++;

        DEBUG(dumpReg(Reg, IRVal););

        // Update the list of IR values mapped to the virtual register
        // (multiple IR values may be mapped to a single virtual register).
        RegIt = SMRegs[MISM].find(Reg);
        if(RegIt == SMRegs[MISM].end())
          RegIt = SMRegs[MISM].emplace(Reg, ValueVecPtr(new ValueVec)).first;
        RegIt->second->push_back(IRVal);
      } else {
        llvm_unreachable("Unrecognized stackmap operand type.");
      }
    }
  }
}

/// Extend the live range for a register to include an instruction.
void
StackTransformMetadata::updateRegisterLiveInterval(MachineOperand &Src,
                                                   const MachineInstr *SM) {
  typedef LiveInterval::Segment Segment;

  assert(Src.isReg() && "Cannot update live range for non-register operand");

  unsigned Vreg = Src.getReg();
  bool hasRegUnit = false;
  SlotIndex Slots[2] = {
    Indexes->getInstructionIndex(Src.getParent()).getRegSlot(),
    Indexes->getInstructionIndex(SM).getRegSlot()
  };

  // Find the segment ending at or containing the call instruction.  Note that
  // we search using the insruction's base index, as the interval may end at
  // the register index (and the end of the range is non-inclusive).
  LiveInterval &Reg = LI->getInterval(Vreg);
  LiveInterval::iterator Seg = Reg.find(Slots[0].getBaseIndex());
  assert(Seg != Reg.end() && Seg->contains(Slots[0].getBaseIndex()) &&
         "Invalid live interval");

  if(Seg->end < Slots[1]) {
    // Update the segment to include the stackmap
    Seg = Reg.addSegment(Segment(Seg->start, Slots[1], Seg->valno));
    DEBUG(dbgs() << "    -> Updated register live interval: "; Seg->dump());

    // We also need to update the physical register's register unit (RU) live
    // range because LiveIntervals::addKillFlags() will use the RU's live range
    // to avoid marking a physical register dead if two virtual registers
    // (mapped to that physical register) have overlapping live ranges.
    MCRegUnitIterator Outer(VRM->getPhys(Vreg), TRI);
    for(MCRegUnitIterator Unit(Outer); Unit.isValid(); ++Unit) {
      LiveRange &RURange = LI->getRegUnit(*Unit);
      LiveRange::iterator RUS;

      for(size_t i = 0; i < 2; i++, RUS = RURange.end()) {
        RUS = RURange.find(Slots[i]);
        if(RUS != RURange.end() && RUS->contains(Slots[i])) break;
      }

      if(RUS != RURange.end()) {
        hasRegUnit = true;
        Seg = RURange.addSegment(
          Segment(RUS->start, Slots[1].getNextIndex(), RUS->valno));
        DEBUG(
          dbgs() << "    -> Updated segment for register unit "
                 << *Unit << ": ";
          Seg->dump();
        );
        break;
      }
    }

    // If we can't extend one of the current RU ranges, add a new range.
    if(!hasRegUnit) {
      LiveRange &RURange = LI->getRegUnit(*Outer);
      VNInfo *Valno = RURange.getNextValue(Slots[0], LI->getVNInfoAllocator());
      Seg = RURange.addSegment(
        Segment(Slots[0], Slots[1].getNextIndex(), Valno));
      DEBUG(
        dbgs() << "    -> Added segment for register unit "
               << *Outer << ": ";
        Seg->dump();
      );
    }
  }
}

/// Rather than modifying the backend machinery to prevent hoisting code
/// between the stackmap and call site, unwind instructions in order to get
/// real live value locations at the function call.
bool StackTransformMetadata::unwindToCallSite(MachineInstr *SM,
                                              const MachineInstr *Call) {
  bool Changed = false, Found;
  MachineOperand *SrcOp;
  MachineInstr *InB = SM;
  RegValsMap::iterator VregIt, SrcIt;
  StackValsMap::iterator SSIt;
  CopyLocPtr C;
  RegCopyLoc *RCL;
  StackCopyLoc *SCL;
  TemporaryValuePtr Tmp;

  // Note: anything named or related to "Src" refers to the source of the
  // copy operation, i.e., the originating location for the value

  DEBUG(dbgs() << "\nUnwinding stackmap back to call site:\n\n");
  while((InB = InB->getPrevNode()) && InB != Call) {
    if((C = getCopyLocation(InB))) {
      DEBUG(dbgs() << "  + Copy instruction: "; InB->dump());

      switch(C->getType()) {
      default: DEBUG(dbgs() << "    Unhandled copy type\n"); break;
      case CopyLoc::VREG:
        RCL = (RegCopyLoc *)C.get();
        SrcOp = &InB->getOperand(InB->findRegisterUseOperandIdx(RCL->SrcVreg));

        // Replace current vreg with source
        Found = false;
        for(size_t i = 2; i < SM->getNumOperands(); i++) {
          MachineOperand &MO = SM->getOperand(i);
          if(MO.isReg() && MO.getReg() == RCL->Vreg) {
            MO.ChangeToRegister(RCL->SrcVreg, false, false, SrcOp->isKill(),
                                SrcOp->isDead(), false, false);
            InB->clearRegisterKills(RCL->SrcVreg, TRI);
            InB->clearRegisterDeads(RCL->SrcVreg);
            updateRegisterLiveInterval(*SrcOp, SM);
            Found = true;
          }
        }

        // Update operand -> IR mapping to source vreg
        if(Found) {
          assert(SMRegs[SM].count(RCL->Vreg) &&
                 "Unhandled register operand in stackmap!");
          VregIt = SMRegs[SM].find(RCL->Vreg);
          SrcIt = SMRegs[SM].find(RCL->SrcVreg);
          if(SrcIt != SMRegs[SM].end()) {
            for(auto IRVal : *VregIt->second)
              SrcIt->second->push_back(IRVal);
          }
          else SMRegs[SM].emplace(RCL->SrcVreg, VregIt->second);
          SMRegs[SM].erase(RCL->Vreg);
          Changed = true;
        }

        break;
      case CopyLoc::STACK_LOAD:
        SCL = (StackCopyLoc *)C.get();

        // Replace current vreg with stack slot.
        // Note: stack slots don't have liveness information to fix up
        Found = false;
        for(size_t i = 2; i < SM->getNumOperands(); i++) {
          MachineOperand &MO = SM->getOperand(i);
          if(MO.isReg() && MO.getReg() == SCL->Vreg) {
            // There's not a great way to add new operands, so trash all
            // trailing operands up to and including the Vreg, add the spill
            // slot, and finally add the trailing operands back.
            SmallVector<MachineOperand, 4> TrailOps(std::next(&MO),
                                                    SM->operands_end());
            while(SM->getNumOperands() > (i + 1)) SM->RemoveOperand(i);
            MachineInstrBuilder Worker(*MF, SM);
            Worker.addImm(StackMaps::IndirectMemRefOp);
            Worker.addImm(MFI->getObjectSize(SCL->StackSlot));
            Worker.addFrameIndex(SCL->StackSlot);
            Worker.addImm(0);
            for(auto Trailing : TrailOps) Worker.addOperand(Trailing);
            Found = true;
          }
        }

        // Update operand -> IR mapping to source stack slot
        if(Found) {
          assert(SMRegs[SM].count(SCL->Vreg) &&
                 "Unhandled register operand in stackmap!");
          SSIt = SMStackSlots[SM].find(SCL->StackSlot);
          VregIt = SMRegs[SM].find(SCL->Vreg);
          if(SSIt != SMStackSlots[SM].end()) {
            for(auto IRVal : *VregIt->second)
              SSIt->second->push_back(IRVal);
          }
          else SMStackSlots[SM].emplace(SCL->StackSlot, VregIt->second);
          SMRegs[SM].erase(SCL->Vreg);
          Changed = true;
        }

        break;
      case CopyLoc::STACK_STORE:
        SCL = (StackCopyLoc *)C.get();
        SrcOp = &InB->getOperand(InB->findRegisterUseOperandIdx(SCL->Vreg));

        // Replace current stack slot with vreg
        // Note: this *must* be an indirect memory reference (spill slot)
        // since we're copying to a register!
        Found = false;
        for(size_t i = 2; i < SM->getNumOperands(); i++) {
          MachineOperand &MO = SM->getOperand(i);
          if(MO.isFI() && MO.getIndex() == SCL->StackSlot) {
            // TODO if the sibling register is killed/dead in the intervening
            // instruction we probably need to propagate that to the stackmap
            // and remove it from the other instruction.
            unsigned StartIdx = i - 2;
            SM->getOperand(StartIdx).ChangeToRegister(SCL->Vreg, false);
            SM->RemoveOperand(StartIdx + 1); // Size
            SM->RemoveOperand(StartIdx + 1); // Frame index
            SM->RemoveOperand(StartIdx + 1); // Frame pointer offset
            Found = true;
          }
        }

        // Update operand -> IR mapping to source vreg
        if(Found) {
          assert(SMStackSlots[SM].count(SCL->StackSlot) &&
                 "Unhandled stack slot operand in stackmap!");

          // Update liveness information to include the stackmap
          InB->clearRegisterKills(SCL->Vreg, TRI);
          InB->clearRegisterDeads(SCL->Vreg);
          updateRegisterLiveInterval(*SrcOp, SM);

          VregIt = SMRegs[SM].find(SCL->Vreg);
          SSIt = SMStackSlots[SM].find(SCL->StackSlot);
          if(VregIt != SMRegs[SM].end()) {
            for(auto IRVal : *SSIt->second)
              VregIt->second->push_back(IRVal);
          }
          else SMRegs[SM].emplace(SCL->Vreg, SSIt->second);
          SMStackSlots[SM].erase(SCL->StackSlot);
          Changed = true;
        }

        break;
      }
    }
    else if((Tmp = TVG->getTemporaryValue(InB, VRM))) {
      DEBUG(dbgs() << "  - Temporary for stackmap: "; InB->dump());
      assert(Tmp->Type == TemporaryValue::StackSlotRef &&
             "Unhandled temporary value");

      // Replace current vreg with stack slot reference.
      // Note: stack slots don't have liveness information to fix up
      Found = false;
      for(size_t i = 2; i < SM->getNumOperands(); i++) {
        MachineOperand &MO = SM->getOperand(i);
        if(MO.isReg() && MO.getReg() == Tmp->Vreg) {
          // There's not a great way to add new operands, so trash all trailing
          // operands up to and including the Vreg, add the metadata, and
          // finally add the trailing operands back.
          SmallVector<MachineOperand, 4> TrailOps(std::next(&MO),
                                                  SM->operands_end());
          while(SM->getNumOperands() > (i + 1)) SM->RemoveOperand(i);
          MachineInstrBuilder Worker(*MF, SM);
          Worker.addImm(StackMaps::TemporaryOp);
          Worker.addImm(Tmp->Size);
          Worker.addImm(Tmp->Offset);
          Worker.addImm(StackMaps::DirectMemRefOp);
          Worker.addFrameIndex(Tmp->StackSlot);
          Worker.addImm(0);
          for(auto Trailing : TrailOps) Worker.addOperand(Trailing);
          Found = true;
        }
      }

      // Update operand -> IR mapping to source stack slot
      if(Found) {
        assert(SMRegs[SM].count(Tmp->Vreg) &&
               "Unhandled register operand in stackmap!");
        SSIt = SMStackSlots[SM].find(Tmp->StackSlot);
        VregIt = SMRegs[SM].find(Tmp->Vreg);
        if(SSIt != SMStackSlots[SM].end()) {
          for(auto IRVal : *VregIt->second)
            SSIt->second->push_back(IRVal);
        }
        else SMStackSlots[SM].emplace(Tmp->StackSlot, VregIt->second);
        SMRegs[SM].erase(Tmp->Vreg);
        Changed = true;
      }
    }
    else DEBUG(dbgs() << "  - Skipping "; InB->dump());
  }

  if(Changed) DEBUG(dbgs() << "\n  Transformed stackmap: "; SM->dump());
  return Changed;
}

/// Is a virtual register live across the machine instruction?
/// Note: returns false if the MI is the last instruction for which the virtual
/// register is alive
bool
StackTransformMetadata::isVregLiveAcrossInstr(unsigned Vreg,
                                              const MachineInstr *MI) const {
  assert(TRI->isVirtualRegister(Vreg) && "Invalid virtual register");

  if(LI->hasInterval(Vreg)) {
    const LiveInterval &TheLI = LI->getInterval(Vreg);
    SlotIndex InstrIdx = Indexes->getInstructionIndex(MI);
    LiveInterval::const_iterator Seg = TheLI.find(InstrIdx);
    if(Seg != TheLI.end() && Seg->contains(InstrIdx) &&
       InstrIdx.getInstrDistance(Seg->end) != 0)
      return true;
  }
  return false;
}

/// Is a stack slot live across the machine instruction?
/// Note: returns false if the MI is the last instruction for which the stack
/// slot is alive
bool
StackTransformMetadata::isSSLiveAcrossInstr(int SS,
                                            const MachineInstr *MI) const {
  if(LS->hasInterval(SS)) {
    const LiveInterval &TheLI = LS->getInterval(SS);
    SlotIndex InstrIdx = Indexes->getInstructionIndex(MI);
    LiveInterval::const_iterator Seg = TheLI.find(InstrIdx);
    if(Seg != TheLI.end() && Seg->contains(InstrIdx) &&
       InstrIdx.getInstrDistance(Seg->end) != 0)
      return true;
  }
  return false;
}

/// Add duplicate location information for a virtual register.
bool StackTransformMetadata::addVregMetadata(unsigned Vreg,
                                             ValueVecPtr IRVals,
                                             const SMInstBundle &SM) {
  const CallInst *IRSM = getIRSM(SM);
  const MachineInstr *MICall = getMICall(SM);
  RegValsMap &Vregs = SMRegs[getMISM(SM)];

  assert(TargetRegisterInfo::isVirtualRegister(Vreg) && VRM->hasPhys(Vreg) &&
         "Cannot add virtual register metadata -- invalid virtual register");

  if(Vregs.find(Vreg) == Vregs.end() && isVregLiveAcrossInstr(Vreg, MICall))
  {
    unsigned Phys = VRM->getPhys(Vreg);
    for(size_t sz = 0; sz < IRVals->size(); sz++) {
      DEBUG(dumpReg(Vreg, (*IRVals)[sz]););
      MF->addSMOpLocation(IRSM, (*IRVals)[sz], MachineLiveReg(Phys));
    }
    Vregs[Vreg] = IRVals;
    return true;
  }
  else return false;
}

/// Add duplicate location information for a stack slot.
bool StackTransformMetadata::addSSMetadata(int SS,
                                           ValueVecPtr IRVals,
                                           const SMInstBundle &SM) {
  const CallInst *IRSM = getIRSM(SM);
  const MachineInstr *MICall = getMICall(SM);
  StackValsMap &SSlots = SMStackSlots[getMISM(SM)];

  assert(!MFI->isDeadObjectIndex(SS) &&
         "Cannot add stack slot metadata -- invalid stack slot");

  if(SSlots.find(SS) == SSlots.end() && isSSLiveAcrossInstr(SS, MICall))
  {
    for(size_t sz = 0; sz < IRVals->size(); sz++) {
      DEBUG(dumpStackSlot(SS, (*IRVals)[sz]););
      MF->addSMOpLocation(IRSM, (*IRVals)[sz], MachineLiveStackSlot(SS));
    }
    SSlots[SS] = IRVals;
    return true;
  }
  else return false;
}

/// Search stack slot copies for additional virtual registers which are live
/// across the stackmap.  Will check to see if the copy instructions have
/// already been visited, and if appropriate, will add virtual registers to
/// work queue.
void inline
StackTransformMetadata::searchStackSlotCopies(int SS,
                                 ValueVecPtr IRVals,
                                 const SMInstBundle &SM,
                                 SmallPtrSet<const MachineInstr *, 32> &Visited,
                                 std::queue<WorkItem> &work,
                                 bool TraverseDefs) {
  StackSlotCopies::const_iterator Copies;
  CopyLocVecPtr CL;
  CopyLocVec::const_iterator Copy, CE;

  if((Copies = SSCopies.find(SS)) != SSCopies.end()) {
    CL = Copies->second;
    for(Copy = CL->begin(), CE = CL->end(); Copy != CE; Copy++) {
      unsigned Vreg = (*Copy)->Vreg;
      const MachineInstr *Instr = (*Copy)->Instr;

      if(!Visited.count(Instr)) {
        addVregMetadata(Vreg, IRVals, SM);
        Visited.insert(Instr);
        work.emplace(Vreg, TraverseDefs);
      }
    }
  }
}

/// Find all alternate locations for virtual registers in a stackmap, and add
/// them to the metadata to be generated.
void
StackTransformMetadata::findAlternateVregLocs(const SMInstBundle &SM) {
  RegValsMap &Regs = SMRegs[getMISM(SM)];
  std::queue<WorkItem> work;
  SmallPtrSet<const MachineInstr *, 32> Visited;
  StackCopyLoc *SCL;
  RegCopyLoc *RCL;

  DEBUG(dbgs() << "\nDuplicate operand locations:\n\n";);

  // Iterate over all vregs in the stackmap
  for(RegValsMap::iterator it = Regs.begin(), end = Regs.end();
      it != end; it++) {
    unsigned origVreg = it->first;
    ValueVecPtr IRVals = it->second;
    Visited.clear();

    // Follow data flow to search for all duplicate locations, including stack
    // slots and other registers.  It's a duplicate if the following are true:
    //
    //   1. It's a copy-like instruction, e.g., a register move or a load
    //      from/store to stack slot
    //   2. The alternate location (virtual register/stack slot) is live across
    //      the machine call instruction
    //
    // Note: we *must* search exhaustively (i.e., across copies from registers
    // that are *not* live across the call) because the following can happen:
    //
    //   STORE vreg0, <fi#0>
    //   ...
    //   COPY vreg0, vreg1
    //   ...
    //   STACKMAP 0, 0, vreg1
    //
    // Here, vreg0 is *not* live across the stackmap, but <fi#0> *is*
    work.emplace(origVreg, true);
    while(!work.empty()) {
      WorkItem cur;
      unsigned vreg;
      int ss;

      // Walk over definitions
      cur = work.front();
      work.pop();
      if(cur.TraverseDefs) {
        for(auto instr = MRI->def_instr_begin(cur.Vreg),
                 ei = MRI->def_instr_end();
            instr != ei; instr++) {

          if(Visited.count(&*instr)) continue;
          CopyLocPtr loc = getCopyLocation(&*instr);
          if(!loc) continue;

          switch(loc->getType()) {
          case CopyLoc::VREG:
            RCL = (RegCopyLoc *)loc.get();
            vreg = RCL->SrcVreg;
            addVregMetadata(vreg, IRVals, SM);
            Visited.insert(&*instr);
            work.emplace(vreg, true);
            break;
          case CopyLoc::STACK_LOAD:
            SCL = (StackCopyLoc *)loc.get();
            ss = SCL->StackSlot;
            if(addSSMetadata(ss, IRVals, SM)) {
              Visited.insert(&*instr);
              searchStackSlotCopies(ss, IRVals, SM, Visited, work, true);
            }
            break;
          default: llvm_unreachable("Unknown/invalid location type"); break;
          }
        }
      }

      // Walk over uses
      for(auto instr = MRI->use_instr_begin(cur.Vreg),
               ei = MRI->use_instr_end();
          instr != ei; instr++) {

        if(Visited.count(&*instr)) continue;
        CopyLocPtr loc = getCopyLocation(&*instr);
        if(!loc) continue;

        // Note: in traversing uses of the given vreg, we *don't* want to
        // traverse definitions of sibling vregs.  Because we're in pseudo-SSA,
        // it's possible we could be defining a register in separate dataflow
        // paths, e.g.:
        //
        // BB A:
        //   %vreg3<def> = COPY %vreg1
        //   JMP <BB C>
        //
        // BB B:
        //   %vreg3<def> = COPY %vreg2
        //   JMP <BB C>
        //
        // ...
        //
        // If we discovered block A through vreg 1, we don't want to explore
        // through block B in which vreg 3 is defined with a different value.
        switch(loc->getType()) {
        case CopyLoc::VREG:
          RCL = (RegCopyLoc *)loc.get();
          vreg = RCL->Vreg;
          addVregMetadata(vreg, IRVals, SM);
          Visited.insert(&*instr);
          work.emplace(vreg, false);
          break;
        case CopyLoc::STACK_STORE:
          SCL = (StackCopyLoc *)loc.get();
          ss = SCL->StackSlot;
          if(addSSMetadata(ss, IRVals, SM)) {
            Visited.insert(&*instr);
            searchStackSlotCopies(ss, IRVals, SM, Visited, work, false);
          }
          break;
        default: llvm_unreachable("Unknown/invalid location type"); break;
        }
      }
    }
  }
}

/// Find alternate storage locations for stackmap operands
bool StackTransformMetadata::findAlternateOpLocs() {
  bool Changed = false;
  RegValsMap::iterator vregIt, vregEnd;

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++) {
    const CallInst *IRSM = getIRSM(*S);
    const MachineInstr *MICall = getMICall(*S);
    MachineInstr *MISM = getMISM(*S);

    DEBUG(
      dbgs() << "\nStackmap " << MISM->getOperand(0).getImm() << ":\n";
      MISM->dump();
      dbgs() << "\n";
    );

    // Get all virtual register/stack slot operands & their associated IR
    // values
    mapOpsToIR(IRSM, MISM);

    // Because the CodeGen machinery is wily (and may hoist instructions above
    // the stackmap), unwind copies until the call site.
    Changed |= unwindToCallSite(MISM, MICall);

    // Find alternate locations for vregs in stack map.  Note we don't need to
    // find alternate stack slot locations, as allocas *should* already be in
    // the stackmap, so the remaining stack slots are spilled registers (which
    // are covered here).
    findAlternateVregLocs(*S);
  }

  return Changed;
}

/// Ensure virtual registers used to generate architecture-specific values are
/// handled by the stackmap & convert to physical registers
void StackTransformMetadata::sanitizeVregs(MachineLiveValPtr &LV,
                                           const MachineInstr *SM) const {
  if(!LV) return;
  if(LV->isGenerated()) {
    MachineGeneratedVal *MGV = (MachineGeneratedVal *)LV.get();
    const ValueGenInstList &Inst = MGV->getInstructions();
    for(size_t i = 0, num = Inst.size(); i < num; i++) {
      if(Inst[i]->opType() == ValueGenInst::OpType::Register) {
        RegInstructionBase *RI = (RegInstructionBase *)Inst[i].get();
        if(!TRI->isVirtualRegister(RI->getReg())) {
          if(RI->getReg() == TRI->getFrameRegister(*MF)) continue;
          // TODO walk through stackmap and see if physical register in
          // instruction is contained in stackmap
          LV.reset(nullptr);
          return;
        }
        else if(!SMRegs.at(SM).count(RI->getReg())) {
          DEBUG(dbgs() << "WARNING: vreg "
                       << TargetRegisterInfo::virtReg2Index(RI->getReg())
                       << " used to generate value not handled in stackmap\n");
          LV.reset(nullptr);
          return;
        }
        else {
          assert(VRM->hasPhys(RI->getReg()) && "Invalid virtual register");
          RI->setReg(VRM->getPhys(RI->getReg()));
        }
      }
    }
  }
}

/// Filter out register definitions we've previously seen.
static void
getUnseenDefinitions(MachineRegisterInfo::def_instr_iterator DefIt,
                     const SmallPtrSet<const MachineInstr *, 4> &Seen,
                     SmallPtrSet<const MachineInstr *, 4> &NewDefs) {
  NewDefs.clear();
  do { if(!Seen.count(&*DefIt)) NewDefs.insert(&*DefIt);
  } while((++DefIt) != MachineRegisterInfo::def_instr_end());
}

/// Try to find the best defining instruction.
static const MachineInstr *
tryToBreakDefMITie(const MachineInstr *MICall,
                   const SmallPtrSet<const MachineInstr *, 4> &Definitions) {
  // First heuristic -- find closest preceding defining instruction in the same
  // machine basic block.
  const MachineInstr *Cur, *BestDef = nullptr;
  unsigned Distance, Best = UINT32_MAX;
  SmallVector<std::pair<const MachineInstr *, unsigned>, 4> SearchDefs;
  for(auto Def : Definitions) {
    Cur = MICall;
    Distance = 1;
    while((Cur = Cur->getPrevNode())) {
      if(Cur == Def) {
        SearchDefs.emplace_back(Def, Distance);
        break;
      }
      Distance++;
    }
  }

  for(auto Pair : SearchDefs) {
    if(Pair.second < Best) {
      BestDef = Pair.first;
      Best = Pair.second;
    }
  }

  if(BestDef)
    DEBUG(dbgs() << "Choosing defining instruction"; BestDef->dump());
  return BestDef;
}

/// Find architecture-specific live values added by the backend
void StackTransformMetadata::findArchSpecificLiveVals() {
  DEBUG(dbgs() << "\n*** Finding architecture-specific live values ***\n\n";);

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++)
  {
    const MachineInstr *MISM = getMISM(*S);
    const MachineInstr *MICall = getMICall(*S);
    const CallInst *IRSM = getIRSM(*S);
    RegValsMap &CurVregs = SMRegs[MISM];
    StackValsMap &CurSS = SMStackSlots[MISM];

    DEBUG(
      MISM->dump();
      dbgs() << "  -> Call instruction SlotIndex ";
      Indexes->getInstructionIndex(MICall).print(dbgs());
      dbgs() << ", searching vregs 0 -> " << MRI->getNumVirtRegs()
             << " and stack slots " << MFI->getObjectIndexBegin() << " -> "
             << MFI->getObjectIndexEnd() << "\n";
    );

    // Include any mandatory architecture-specific live values
    TVG->addRequiredArchLiveValues(MF, MISM, IRSM);

    // Search for virtual registers not handled by the stackmap.  Registers
    // spilled to the stack should have been converted to frame index
    // references by now.
    for(unsigned i = 0, numVregs = MRI->getNumVirtRegs(); i < numVregs; i++) {
      unsigned Vreg = TargetRegisterInfo::index2VirtReg(i);
      MachineLiveValPtr MLV;
      MachineLiveReg MLR(0);

      if(VRM->hasPhys(Vreg) && isVregLiveAcrossInstr(Vreg, MICall) &&
         CurVregs.find(Vreg) == CurVregs.end()) {
        DEBUG(dbgs() << "    + vreg" << i
                     << " is live in register but not in stackmap\n";);

        // Walk the use-def chain to see if we can find a valid value.  Note we
        // keep track of seen definitions because even though we're supposed to
        // be in SSA form it's possible to find definition cycles.
        const MachineInstr *DefMI;
        unsigned ChainVreg = Vreg;
        SmallPtrSet<const MachineInstr *, 4> SeenDefs, NewDefs;
        do {
          getUnseenDefinitions(MRI->def_instr_begin(ChainVreg),
                               SeenDefs, NewDefs);

          // Try to find a suitable defining instruction
          if(NewDefs.size() == 0) {
            DEBUG(dbgs() << "WARNING: no unseen definition\n");
            break;
          }
          else if(NewDefs.size() == 1) DefMI = *NewDefs.begin();
          else if(!(DefMI = tryToBreakDefMITie(MICall, NewDefs))) {
            // No suitable defining instruction, not much we can do...
            DEBUG(
              dbgs() << "WARNING: multiple definitions for virtual "
                        "register, missed in live-value analysis?\n";
              for(auto d = MRI->def_instr_begin(ChainVreg),
                  e = MRI->def_instr_end(); d != e; d++)
                d->dump();
            );
            break;
          }

          SeenDefs.insert(DefMI);
          MLV = TVG->getMachineValue(DefMI);
          sanitizeVregs(MLV, MISM);

          if(MLV) break; // We got a value!
          else {
            // Couldn't get a value, follow the use-def chain
            CopyLocPtr Copy = getCopyLocation(DefMI);
            if(Copy) {
              switch(Copy->getType()) {
              default: ChainVreg = 0; break;
              case CopyLoc::VREG:
                ChainVreg = ((RegCopyLoc *)Copy.get())->SrcVreg;
                break;
              }
            }
            else ChainVreg = 0;
          }
        } while(TargetRegisterInfo::isVirtualRegister(ChainVreg));

        if(MLV) {
          DEBUG(dbgs() << "      Defining instruction: ";
                MLV->getDefiningInst()->print(dbgs());
                dbgs() << "      Value: " << MLV->toString() << "\n");

          MLR.setReg(VRM->getPhys(Vreg));
          MF->addSMArchSpecificLocation(IRSM, MLR, *MLV);
          CurVregs.emplace(Vreg, ValueVecPtr(nullptr));
        }
        else {
          DEBUG(
            DefMI = &*MRI->def_instr_begin(Vreg);
            StringRef BBName = DefMI->getParent()->getName();
            dbgs() << "      Unhandled defining instruction in basic block "
                   << BBName << ":";
            DefMI->print(dbgs());
          );
        }
      }
    }

    // Search for stack slots not handled by the stackmap
    for(int SS = MFI->getObjectIndexBegin(), e = MFI->getObjectIndexEnd();
        SS < e; SS++) {
      if(UsedSS.count(SS) && !MFI->isDeadObjectIndex(SS) &&
         isSSLiveAcrossInstr(SS, MICall) && CurSS.find(SS) == CurSS.end()) {
        DEBUG(dbgs() << "    + stack slot " << SS
                     << " is live but not in stackmap\n";);
        // TODO add arch-specific stack slot information to machine function
      }
    }

    DEBUG(dbgs() << "\n";);
  }
}

void StackTransformMetadata::findMarshaledArguments() {
  unsigned opIt, baseReg, size;
  int64_t argSpace;
  std::vector<unsigned> PhysRegs;
  std::set<int64_t> Offsets;
  MachineLiveReg regLoc;
  MachineLiveStackAddr stackLoc;

  DEBUG(dbgs() << "*** Finding argument passing locations ***\n\n");

  // TODO the following is only implemented for X86Values, need to implement
  // for other architectures

  baseReg = TVG->getArgSpaceBaseReg();
  for(auto S = SM.begin(); S != SM.end(); S++) {
    // Find the IR call which triggered inserting the stackmap
    const CallInst *IRSM = getIRSM(*S);
    BasicBlock::const_reverse_iterator it(IRSM);
    for(; it != IRSM->getParent()->rend(); it++)
      if(isa<CallInst>(&*it)) break;
    if(it == IRSM->getParent()->rend()) continue;
    const CallInst *Call = cast<CallInst>(&*it);

    // Find the arguments passed in registers
    const MachineInstr *MICall = getMICall(*S);

    DEBUG(Call->dump(); getMISM(*S)->dump(); MICall->dump(););

    TVG->getArgRegs(MICall, PhysRegs);

    assert(PhysRegs.size() <= (Call->getNumOperands() - 1) &&
           "Too many registers for passing arguments");

    // TODO this matching doesn't work if we have floating-point arguments

    // Add argument-passing registers to the stackmap if they contain pointers
    // and thus may need to be reified.
    // Note: reifying arguments in registers is only required for Chameleon --
    // for Popcorn, we assume we only migrate at calls to check_migrate(),
    // which have no pointer-to-stack arguments that need to be reified
    // TODO turn off if not compiling for chameleon
    DEBUG(dbgs() << "\nRegister arguments\n");
    for(opIt = 0; opIt < PhysRegs.size(); opIt++) {
      if(Call->getOperand(opIt)->getType()->isPointerTy()) {
        DEBUG(dbgs() << " -> register " << PrintReg(PhysRegs[opIt], TRI)
              << " is of pointer type\n");
        regLoc = MachineLiveReg(PhysRegs[opIt]);
        regLoc.setIsPtr(true);
        MF->addSMArgLocation(IRSM, regLoc);
      }
    }

    // Find the arguments passed in stack slots
    argSpace = TVG->getArgSlots(MICall, Offsets);
    DEBUG(dbgs() << "frame size for argument space: " << argSpace << "\n");

    assert((Call->getNumOperands() - 1 - PhysRegs.size()) == Offsets.size() &&
           "Number of on-stack arguments does not match number of offsets");
    assert(PhysRegs.size() + Offsets.size() == (Call->getNumOperands() - 1) &&
           "Found too  many arguments?");

    // Add argument passing stack slots to stackmap.  This is always required
    // as stack arguments may be accessed throughout the called function (LLVM
    // may not have pulled them into registers).  Additionally, these slots may
    // contain pointers that may need to be reified; mark if so.
    DEBUG(dbgs() << "\nOn-stack arguments:\n");
    std::set<int64_t>::const_iterator offsetIt = Offsets.begin();
    for(opIt = PhysRegs.size();
        opIt < Call->getNumOperands() - 1;
        opIt++, offsetIt++) {
      DEBUG(Call->getOperand(opIt)->dump());

      // Calculate the size of the slot
      auto nextOffset = offsetIt; nextOffset++;
      if(nextOffset == Offsets.end()) size = argSpace - *offsetIt;
      else size = *nextOffset - *offsetIt;

      // Add the metadata for parsing during stackmap creation
      stackLoc = MachineLiveStackAddr(*offsetIt, baseReg, size);
      if(Call->getOperand(opIt)->getType()->isPointerTy()) {
        DEBUG(dbgs() << " -> is of pointer type\n");
        stackLoc.setIsPtr(true);
      }
      MF->addSMArgLocation(IRSM, stackLoc);
    }

    DEBUG(dbgs() << "\n");
  }
}

/// Find IR call instruction which generated the stackmap
static inline const CallInst *findCalledFunc(const llvm::CallInst *IRSM) {
  const Instruction *Func = IRSM->getPrevNode();
  while(Func && !isa<CallInst>(Func)) Func = Func->getPrevNode();
  return dyn_cast<CallInst>(Func);
}

/// Display a warning about unhandled values
static inline void displayWarning(std::string &Msg,
                                  const CallInst *CI,
                                  const Function *F) {
  assert(CI && "Invalid arguments");

  // Note: it may be possible for us to not have a called function, for example
  // if we call a function using a function pointer
  const Function *CurF = CI->getParent()->getParent();
  const std::string &Triple = CurF->getParent()->getTargetTriple();
  Msg = "(" + Triple + ") " + Msg;
  if(F && F->hasName()) {
    Msg += " across call to ";
    Msg += F->getName();
  }
  DiagnosticInfoOptimizationFailure DI(*CurF, CI->getDebugLoc(), Msg);
  CurF->getContext().diagnose(DI);
}

/// Warn about unhandled registers & stack slots
void StackTransformMetadata::warnUnhandled() const {
  std::string Msg;
  const CallInst *IRCall;
  const Function *CalledFunc;
  bool unhandled;

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++)
  {
    const MachineInstr *MISM = getMISM(*S);
    const MachineInstr *MICall = getMICall(*S);
    const RegValsMap &CurVregs = SMRegs.at(MISM);
    const StackValsMap &CurSS = SMStackSlots.at(MISM);
    unhandled = false;
    IRCall = findCalledFunc(getIRSM(*S));
    CalledFunc = IRCall->getCalledFunction();
    assert(IRCall && "No call instruction for stackmap");

    // Search for virtual registers not handled by the stackmap
    for(unsigned i = 0; i < MRI->getNumVirtRegs(); i++) {
      unsigned Vreg = TargetRegisterInfo::index2VirtReg(i);

      // Virtual register allocated to physical register
      if(VRM->hasPhys(Vreg) && isVregLiveAcrossInstr(Vreg, MICall) &&
         CurVregs.find(Vreg) == CurVregs.end()) {
        Msg = "Stack transformation: unhandled register ";
        Msg += TRI->getName(VRM->getPhys(Vreg));
        displayWarning(Msg, IRCall, CalledFunc);
        unhandled = true;
      }
    }

    // Search for all stack slots not handled by the stackmap
    for(int SS = MFI->getObjectIndexBegin(), e = MFI->getObjectIndexEnd();
        SS < e; SS++) {
      if(UsedSS.count(SS) && !MFI->isDeadObjectIndex(SS) &&
         isSSLiveAcrossInstr(SS, MICall) && CurSS.find(SS) == CurSS.end()) {
        Msg = "Stack transformation: unhandled stack slot ";
        Msg += std::to_string(SS);
        displayWarning(Msg, IRCall, CalledFunc);
        unhandled = true;
      }
    }

    if(unhandled) MF->setSMHasUnhandled(getIRSM(*S));
  }
}

