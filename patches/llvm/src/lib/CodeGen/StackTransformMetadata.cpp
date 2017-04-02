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
#include "llvm/Target/TargetValueGenerator.h"
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

  /* Types */

  /// A bundle tying together a stackmap IR instruction, the generated stackmap
  /// machine instruction and the call machine instruction that caused the
  /// stackmap to be emitted in the IR, respectively
  typedef std::tuple<const CallInst *,
                     const MachineInstr *,
                     const MachineInstr *> SMInstBundle;

  /// Getters for individual elements of instruction bundles
  static inline const
  CallInst *getIRSM(const SMInstBundle &B) { return std::get<0>(B); }
  static inline const
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
  typedef std::pair<const MachineInstr *, RegValsMap> SMVregsPair;
  typedef std::map<const MachineInstr *, RegValsMap> SMVregsMap;

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

  /* Data */

  /// LLVM-provided analysis & metadata
  MachineFunction *MF;
  const MachineFrameInfo *MFI;
  const MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
  const TargetRegisterInfo *TRI;
  const TargetValueGenerator *TVG;
  const LiveIntervals *LI;
  const LiveStacks *LS;
  const SlotIndexes *Indexes;
  const VirtRegMap *VRM;

  /// Stackmap/call instructions, mapping of virtual registers & stack slots to
  /// IR values, list of instructions that copy to/from the stack
  SmallVector<SMInstBundle, 32> SM;
  SMVregsMap SMVregs;
  SMStackSlotMap SMStackSlots;
  StackSlotCopies SSUses;

  /* Functions */

  // Reset the analysis for a new function
  void reset() {
    SM.clear();
    SMVregs.clear();
    SMStackSlots.clear();
    SSUses.clear();
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

  /// Is a virtual register live across the machine instruction?
  bool isVregLiveAcrossInstr(unsigned Vreg, const MachineInstr *MI) const;

  /// Is a stack slot live across the machine instruction?
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
                        std::queue<unsigned> &work);

  /// Find all alternate locations for virtual registers in a stackmap, and add
  /// them to the metadata to be generated.
  void findAlternateVregLocs(const SMInstBundle &SM);

  /// Find stackmap operands that have been spilled to alternate locations
  void findAlternateOpLocs();

  /// Analyze a machine instruction to find the value being used
  MachineLiveValPtr getTargetValue(const MachineInstr *MI) const;

  /// Find architecture-specific live values added by the backend
  void findArchSpecificLiveVals();

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
  if(fn.getFrameInfo()->hasStackMap()) {
    MF = &fn;
    MFI = MF->getFrameInfo();
    MRI = &MF->getRegInfo();
    TII = MF->getSubtarget().getInstrInfo();
    TRI = MF->getSubtarget().getRegisterInfo();
    TVG = MF->getSubtarget().getValueGenerator();
    Indexes = &getAnalysis<SlotIndexes>();
    LI = &getAnalysis<LiveIntervals>();
    LS = &getAnalysis<LiveStacks>();
    VRM = &getAnalysis<VirtRegMap>();
    reset();

    DEBUG(
      dbgs() << "\n********** STACK TRANSFORMATION METADATA **********\n"
             << "********** Function: " << MF->getName() << '\n';
      VRM->dump();
    );

    findStackmapsAndStackSlotCopies();
    findAlternateOpLocs();
    findArchSpecificLiveVals();
    warnUnhandled();
  }

  return false;
}

/// Print information about a virtual register and it's associated IR value
void StackTransformMetadata::dumpReg(unsigned Reg, const Value *IRVal) const {
  IRVal->printAsOperand(dbgs());
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
  IRVal->printAsOperand(dbgs());
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
          DEBUG(
            dbgs() << "WARNING: stackmap " << ID << " ";
            IRSM->printAsOperand(dbgs());
            dbgs() << ": could not find associated call instruction "
                      "(lowered to a native instruction?)\n";
          );
          continue;
        }

        SM.push_back(SMInstBundle(IRSM, &*MI, MCI));
      }
      else {
        // See if instruction copies to/from stack slot
        StackSlotCopies::iterator it;
        CopyLocPtr loc;

        if(!(loc = getCopyLocation(&*MI))) continue;
        enum CopyLoc::Type type = loc->getType();
        if(type == CopyLoc::STACK_LOAD || type == CopyLoc::STACK_STORE) {
          StackCopyLoc *SCL = (StackCopyLoc *)loc.get();
          if((it = SSUses.find(SCL->StackSlot)) == SSUses.end()) {
            StackSlotCopyPair ins(SCL->StackSlot,
                                  CopyLocVecPtr(new CopyLocVec));
            it = SSUses.insert(std::move(ins)).first;
          }
          it->second->push_back(loc);
        }
      }
    }
  }
}

/// Find all virtual register/stack slot operands in a stackmap and collect
/// virtual register/stack slot <-> IR value mappings
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

      assert(IRVal && "Invalid stackmap IR operand");
      assert(MFI->getObjectIndexBegin() <= FrameIdx &&
             FrameIdx <= MFI->getObjectIndexEnd() && "Invalid frame index");
      assert(!MFI->isDeadObjectIndex(FrameIdx) && "Dead frame index");
      DEBUG(dumpStackSlot(FrameIdx, IRVal););

      // Update the list of IR values mapped to the stack slot (multiple IR
      // values may be mapped to a single stack slot).
      SSIt = SMStackSlots[MISM].find(FrameIdx);
      if(SSIt == SMStackSlots[MISM].end()) {
        StackValsPair OpMap(FrameIdx, ValueVecPtr(new ValueVec));
        SSIt = SMStackSlots[MISM].insert(std::move(OpMap)).first;
      }
      SSIt->second->push_back(IRVal);
    }
    else if(MOit->isReg()) { // Map IR values to virtual registers
      const Value *IRVal = IRit->get();
      unsigned Reg = MOit->getReg();

      assert(IRVal && "Invalid stackmap IR operand");
      assert(TargetRegisterInfo::isVirtualRegister(Reg) &&
             "Should not have been converted to physical registers yet");
      DEBUG(dumpReg(Reg, IRVal););

      // Update the list of IR values mapped to the virtual register (multiple
      // IR values may be mapped to a single virtual register).
      VregIt = SMVregs[MISM].find(Reg);
      if(VregIt == SMVregs[MISM].end()) {
        RegValsPair OpMap(Reg, ValueVecPtr(new ValueVec));
        VregIt = SMVregs[MISM].insert(std::move(OpMap)).first;
      }
      VregIt->second->push_back(IRVal);
    } else {
      llvm_unreachable("Unrecognized stackmap operand type.");
    }
  }
  assert(IRit == (IRSM->op_end() - 1) && "Did not search all stackmap operands");
}

/// Is a virtual register live across the machine instruction?
bool
StackTransformMetadata::isVregLiveAcrossInstr(unsigned Vreg,
                                              const MachineInstr *MI) const {
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
bool
StackTransformMetadata::isSSLiveAcrossInstr(int SS,
                                            const MachineInstr *MI) const {
  if(LS->hasInterval(SS)) {
    const LiveInterval &TheLI = LS->getInterval(SS);
    SlotIndex InstrIdx = Indexes->getInstructionIndex(MI);
    LiveInterval::const_iterator Seg = TheLI.find(InstrIdx);
    if(Seg != TheLI.end() && Seg->contains(InstrIdx))
      return true;
  }
  return false;
}

/// Add duplicate location information for a virtual register.
bool StackTransformMetadata::addVregMetadata(unsigned Vreg,
                                             ValueVecPtr IRVals,
                                             const SMInstBundle &SM) {
  const CallInst *IRSM = getIRSM(SM);
  const MachineInstr *MISM = getMISM(SM);
  const MachineInstr *MICall = getMICall(SM);
  RegValsMap &Vregs = SMVregs[MISM];

  assert(TargetRegisterInfo::isVirtualRegister(Vreg) && VRM->hasPhys(Vreg) &&
         "Cannot add virtual register metadata -- invalid virtual register");

  if(Vregs.find(Vreg) == Vregs.end() &&
     (isVregLiveAcrossInstr(Vreg, MISM) || isVregLiveAcrossInstr(Vreg, MICall)))
  {
    unsigned Phys = VRM->getPhys(Vreg);
    for(size_t sz = 0; sz < IRVals->size(); sz++) {
      DEBUG(dumpReg(Vreg, IRVals->operator[](sz)););
      MF->addSMOpLocation(IRSM, IRVals->operator[](sz), MachineLiveReg(Phys));
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
  const MachineInstr *MISM = getMISM(SM);
  const MachineInstr *MICall = getMICall(SM);
  StackValsMap &SSlots = SMStackSlots[MISM];

  assert(!MFI->isDeadObjectIndex(SS) &&
         "Cannot add stack slot metadata -- invalid stack slot");

  if(SSlots.find(SS) == SSlots.end() &&
     (isSSLiveAcrossInstr(SS, MISM) || isSSLiveAcrossInstr(SS, MICall)))
  {
    for(size_t sz = 0; sz < IRVals->size(); sz++) {
      DEBUG(dumpStackSlot(SS, IRVals->operator[](sz)););
      MF->addSMOpLocation(IRSM, IRVals->operator[](sz),
                          MachineLiveStackSlot(SS));
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
                                       std::queue<unsigned> &work) {
  StackSlotCopies::const_iterator Copies;
  CopyLocVecPtr CL;
  CopyLocVec::const_iterator Copy, CE;

  if((Copies = SSUses.find(SS)) != SSUses.end()) {
    CL = Copies->second;
    for(Copy = CL->begin(), CE = CL->end(); Copy != CE; Copy++) {
      unsigned Vreg = (*Copy)->Vreg;
      const MachineInstr *Instr = (*Copy)->Instr;

      if(!Visited.count(Instr) && addVregMetadata(Vreg, IRVals, SM)) {
        Visited.insert(Instr);
        work.push(Vreg);
      }
    }
  }
}

/// Find all alternate locations for virtual registers in a stackmap, and add
/// them to the metadata to be generated.
void
StackTransformMetadata::findAlternateVregLocs(const SMInstBundle &SM) {
  RegValsMap &Vregs = SMVregs[getMISM(SM)];
  std::queue<unsigned> work;
  SmallPtrSet<const MachineInstr *, 32> Visited;
  StackCopyLoc *SCL;
  RegCopyLoc *RCL;

  DEBUG(dbgs() << "\nDuplicate operand locations:\n\n";);

  // Iterate over all vregs in the stackmap
  for(RegValsMap::iterator it = Vregs.begin(), end = Vregs.end();
      it != end; it++) {
    unsigned origVreg = it->first;
    ValueVecPtr IRVals = it->second;
    Visited.clear();

    // Follow data flow to search for all duplicate locations, including stack
    // slots and other registers.  It's a duplicate location if the following
    // are true:
    //
    //   1. It's a copy-like instruction, e.g., a register move or a load
    //      from/store to stack slot
    //   2. The alternate location (virtual register/stack slot) is live across
    //      either the machine call instruction or the stackmap
    //
    work.push(origVreg);
    while(!work.empty()) {
      unsigned cur, vreg;
      int ss;

      // Walk over definitions
      cur = work.front();
      work.pop();
      for(auto instr = MRI->def_instr_begin(cur), ei = MRI->def_instr_end();
          instr != ei;
          instr++) {
        CopyLocPtr loc = getCopyLocation(&*instr);
        if(!loc) continue;

        switch(loc->getType()) {
        case CopyLoc::VREG:
          RCL = (RegCopyLoc *)loc.get();
          vreg = RCL->SrcVreg;
          if(!Visited.count(&*instr) && addVregMetadata(vreg, IRVals, SM)) {
            Visited.insert(&*instr);
            work.push(vreg);
          }
          break;
        case CopyLoc::STACK_LOAD:
          SCL = (StackCopyLoc *)loc.get();
          ss = SCL->StackSlot;
          if(!Visited.count(&*instr) && addSSMetadata(ss, IRVals, SM)) {
            Visited.insert(&*instr);
            searchStackSlotCopies(ss, IRVals, SM, Visited, work);
          }
          break;
        default: llvm_unreachable("Unknown/invalid location type"); break;
        }
      }

      // Walk over uses
      for(auto instr = MRI->use_instr_begin(cur), ei = MRI->use_instr_end();
          instr != ei; instr++) {
        CopyLocPtr loc = getCopyLocation(&*instr);
        if(!loc) continue;

        switch(loc->getType()) {
        case CopyLoc::VREG:
          RCL = (RegCopyLoc *)loc.get();
          vreg = RCL->Vreg;
          if(!Visited.count(&*instr) && addVregMetadata(vreg, IRVals, SM)) {
            Visited.insert(&*instr);
            work.push(vreg);
          }
          break;
        case CopyLoc::STACK_STORE:
          SCL = (StackCopyLoc *)loc.get();
          ss = SCL->StackSlot;
          if(!Visited.count(&*instr) && addSSMetadata(ss, IRVals, SM)) {
            Visited.insert(&*instr);
            searchStackSlotCopies(ss, IRVals, SM, Visited, work);
          }
          break;
        default: llvm_unreachable("Unknown/invalid location type"); break;
        }
      }
    }
  }
}

/// Find alternate storage locations for stackmap operands
void StackTransformMetadata::findAlternateOpLocs() {
  RegValsMap::iterator vregIt, vregEnd;

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++) {
    const CallInst *IRSM = getIRSM(*S);
    const MachineInstr *MISM = getMISM(*S);

    DEBUG(
      dbgs() << "\nStackmap " << MISM->getOperand(0).getImm() << ":\n";
      MISM->dump();
      dbgs() << "\n";
    );

    // Get all virtual register/stack slot operands & their associated IR
    // values
    mapOpsToIR(IRSM, MISM);

    // Find alternate locations for vregs in stack map
    findAlternateVregLocs(*S);

    // TODO Find alternate locations for stack slots in stack map
  }
}

/// Analyze a machine instruction to find the value being used
MachineLiveValPtr
StackTransformMetadata::getTargetValue(const MachineInstr *MI) const {
  if(!MI) return MachineLiveValPtr(nullptr);

  // Immediates can be handled in an architecture-agnostic way
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
    return MachineLiveValPtr(new MachineImmediate(Size, Value, MI));
  }
  else return TVG->getMachineValue(MI);
}

/// Find architecture-specific live values added by the backend
void StackTransformMetadata::findArchSpecificLiveVals() {
  DEBUG(dbgs() << "\n*** Finding architecture-specific live values ***\n\n";);

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++)
  {
    const MachineInstr *MISM = getMISM(*S);
    const MachineInstr *MICall = getMICall(*S);
    const CallInst *IRSM = getIRSM(*S);
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
      MachineLiveValPtr MC;
      MachineLiveReg MLR(0);

      // Detect virtual registers live across but not included in the stackmap
      if(VRM->hasPhys(Vreg) && isVregLiveAcrossInstr(Vreg, MICall) &&
         CurVregs.find(Vreg) == CurVregs.end()) {
        DEBUG(dbgs() << "    + vreg" << i
                     << " is live in register but not in stackmap\n";);

        assert(++(MRI->def_instr_begin(Vreg)) == MRI->def_instr_end() &&
               "Multiple definitions for virtual register");
        MC = getTargetValue(&*MRI->def_instr_begin(Vreg));
        if(MC) {
          DEBUG(
            dbgs() << "      Defining instruction: ";
            MC->getDefiningInst()->print(dbgs());
            dbgs() << "      Value: " << MC->toString() << "\n";
          );

          MLR.setReg(VRM->getPhys(Vreg));
          MF->addSMArchSpecificLocation(IRSM, MLR, *MC);
          CurVregs.insert(RegValsPair(Vreg, ValueVecPtr(nullptr)));
        }
      }
      // Detect virtual registers mapped to stack slots not in stackmap
      // Note: we can't detect if this vreg is actually in use judging by the
      // value returned from VRM->getStackSlot.  Therefore, manually check by
      // seeing if there are any definitions.
      else if(VRM->getStackSlot(Vreg) != VirtRegMap::NO_STACK_SLOT &&
              (MRI->def_instr_begin(Vreg) != MRI->def_instr_end()) &&
              isVregLiveAcrossInstr(Vreg, MICall) &&
              CurVregs.find(Vreg) == CurVregs.end()) {
        DEBUG(dbgs() << "    + vreg" << i
                     << " is live in stack slot but not in stackmap\n");
        // TODO handle vreg spilled to stack slot
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
      }
    }

    DEBUG(dbgs() << "\n";);
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
  assert(CI && F && "Invalid arguments");

  const Function *CurF = CI->getParent()->getParent();
  if(F->hasName()) {
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

  for(auto S = SM.begin(), SE = SM.end(); S != SE; S++)
  {
    const MachineInstr *MISM = getMISM(*S);
    const MachineInstr *MICall = getMICall(*S);
    const RegValsMap &CurVregs = SMVregs.at(MISM);
    const StackValsMap &CurSS = SMStackSlots.at(MISM);
    IRCall = findCalledFunc(getIRSM(*S));
    CalledFunc = IRCall->getCalledFunction();

    assert(IRCall && CalledFunc && "No call instruction for stackmap");

    // Search for virtual registers not handled by the stackmap
    for(unsigned i = 0; i < MRI->getNumVirtRegs(); i++) {
      unsigned Vreg = TargetRegisterInfo::index2VirtReg(i);

      // Virtual register allocated to physical register
      if(VRM->hasPhys(Vreg) && isVregLiveAcrossInstr(Vreg, MICall) &&
         CurVregs.find(Vreg) == CurVregs.end()) {
        Msg = "Unhandled register ";
        Msg += TRI->getName(VRM->getPhys(Vreg));
        displayWarning(Msg, IRCall, CalledFunc);
      }
      // Virtual register spilled to the stack
      else if(VRM->getStackSlot(Vreg) != VirtRegMap::NO_STACK_SLOT &&
              isVregLiveAcrossInstr(Vreg, MICall) &&
              CurVregs.find(Vreg) == CurVregs.end()) {
        auto def = MRI->def_instr_begin(Vreg), end = MRI->def_instr_end();
        if(def != end) {
          Msg = "Unhandled virtual register ";
          Msg += std::to_string(Vreg);
          Msg += " in stack slot ";
          Msg += std::to_string(VRM->getStackSlot(Vreg));
          displayWarning(Msg, IRCall, CalledFunc);
        }
      }
    }

    // Search for all stack slots not handled by the stackmap
    for(int SS = MFI->getObjectIndexBegin(), e = MFI->getObjectIndexEnd();
        SS < e; SS++) {
      if(!MFI->isDeadObjectIndex(SS) &&
         isSSLiveAcrossInstr(SS, MICall) && CurSS.find(SS) == CurSS.end()) {
        Msg = "Unhandled stack slot ";
        Msg += std::to_string(SS);
        displayWarning(Msg, IRCall, CalledFunc);
      }
    }
  }
}

