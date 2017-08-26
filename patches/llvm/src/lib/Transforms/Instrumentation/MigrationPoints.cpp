//===- MigrationPoints.cpp ------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Instrument the code with migration points, which are locations where threads
// make calls to invoke the migration process in addition to any other
// instrumentation (e.g., hardware transactional memory, HTM, stops & starts).
// Migration points only occur at equivalence points, or locations in the
// program code where there is a direct mapping between architecture-specific
// execution state like the registers and stack across different ISAs.  In our
// implementation, every function call site is an equivalence point; hence,
// calls inserted to invoke the migration by definition create equivalence
// points at the migration point.  Thus, all migration points are equivalence
// points, but not all equivalence points are migration points.
//
// By default, the pass only inserts migration points at the beginning and end
// of a function.  More advanced analyses can be used to instrument function
// bodies (in particular, loops) with more migration points.
//
// More details about equivalence points can be found in the paper "A Unified
// Model of Pointwise Migration of Procedural Computations" by von Bank et. al
// (http://dl.acm.org/citation.cfm?id=197402).
//
//===----------------------------------------------------------------------===//

#include <map>
#include <memory>
#include <queue>
#include <set>
#include "llvm/Pass.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "migration-points"

/// Insert more migration points into the body of a function.  Analyze memory
/// usage & attempt to instrument the code to reduce the time until the thread
/// reaches an migration point.  If HTM instrumentation is enabled, analysis is
/// tailored to avoid hardware transactional memory (HTM) capacity aborts.
const static cl::opt<bool>
MoreMigPoints("more-mig-points", cl::Hidden, cl::init(false),
  cl::desc("Add additional migration points into the body of functions"));

/// Cover the application in transactional execution by inserting HTM
/// stop/start instructions at migration points.
const static cl::opt<bool>
HTMExec("htm-execution", cl::NotHidden, cl::init(false),
  cl::desc("Instrument migration points with HTM execution "
           "(only supported on PowerPC 64-bit & x86-64)"));

/// Disable wrapping libc functions which are likely to cause HTM aborts with
/// HTM stop/start intrinsics.  Wrapping happens by default with HTM execution.
const static cl::opt<bool>
NoWrapLibc("htm-no-wrap-libc", cl::Hidden, cl::init(false),
  cl::desc("Disable wrapping libc functions with HTM stop/start"));

/// Disable rollback-only transactions for PowerPC.
const static cl::opt<bool>
NoROTPPC("htm-ppc-no-rot", cl::Hidden, cl::init(false),
  cl::desc("Disable rollback-only transactions in HTM instrumentation "
           "(PowerPC only)"));

/// HTM memory read buffer size for tuning analysis when inserting additional
/// migration points.
const static cl::opt<unsigned>
HTMReadBufSizeArg("htm-buf-read", cl::Hidden, cl::init(8),
  cl::desc("HTM analysis tuning - HTM read buffer size, in kilobytes"),
  cl::value_desc("size"));

/// HTM memory write buffer size for tuning analysis when inserting additional
/// migration points.
const static cl::opt<unsigned>
HTMWriteBufSizeArg("htm-buf-write", cl::Hidden, cl::init(8),
  cl::desc("HTM analysis tuning - HTM write buffer size, in kilobytes"),
  cl::value_desc("size"));

#define KB 1024
#define HTMReadBufSize (HTMReadBufSizeArg * KB)
#define HTMWriteBufSize (HTMWriteBufSizeArg * KB)

STATISTIC(NumMigPoints, "Number of migration points added");
STATISTIC(NumHTMBegins, "Number of HTM begin intrinsics added");
STATISTIC(NumHTMEnds, "Number of HTM end intrinsics added");
STATISTIC(LoopsTransformed, "Number of loops transformed");

namespace {

/// Get the integer size of a value, if statically known.
static int64_t getValueSize(const Value *V) {
  if(isa<ConstantInt>(V)) return cast<ConstantInt>(V)->getSExtValue();
  DEBUG(dbgs() << "Couldn't get size for"; V->dump() );
  return -1;
}

/// Weight metrics.  Child classes implement for different analyses.
class Weight {
public:
  virtual ~Weight() {};
  virtual Weight *copy() const = 0;

  /// Expose types of child implementations.
  virtual bool isHTMWeight() const { return false; }

  /// Analyze an instruction & update accounting.
  virtual void analyze(const Instruction *I, const DataLayout *DL) = 0;

  /// Return whether or not we should add an migration point.
  virtual bool shouldAddMigPoint() const = 0;

  /// Reset internal weights after finding or placing an migration point.
  virtual void reset() = 0;

  /// Merge weights of predecessors to get the maximum starting weight of a
  /// code section being analyzed.
  virtual void mergeMax(const Weight *RHS) = 0;
  virtual void mergeMax(const std::unique_ptr<Weight> &RHS) = 0;

  /// Scale the weight by a factor, e.g., a number of loop iterations.
  virtual void scale(size_t factor) = 0;

  /// Number of times this weight "fits" into a given resource before we need
  /// to place an migration point.  This is used for calculating how many
  /// iterations of a loop can be executed between migration points.
  virtual size_t numIters() const = 0;

  /// Return whether or not the weight is within some percent (0-100) of the
  /// threshold metric for a type of weight.
  virtual bool underPercentOfThreshold(unsigned percent) const = 0;

  /// Return a human-readable string describing weight information.
  virtual std::string toString() const = 0;
};

/// Weight metrics for HTM analysis, which basically depend on the number
/// of bytes loaded & stored.
class HTMWeight : public Weight {
private:
  // The number of bytes loaded & stored, respectively
  size_t LoadBytes, StoreBytes;

  // Statistics about when the weight was reset (i.e., at HTM stop/starts)
  size_t Resets, ResetLoad, ResetStore;

public:
  HTMWeight(size_t LoadBytes = 0, size_t StoreBytes = 0)
    : LoadBytes(LoadBytes), StoreBytes(StoreBytes), Resets(0), ResetLoad(0),
      ResetStore(0) {}
  HTMWeight(const HTMWeight &C)
    : LoadBytes(C.LoadBytes), StoreBytes(C.StoreBytes) {}
  virtual Weight *copy() const { return new HTMWeight(*this); }

  virtual bool isHTMWeight() const { return true; }

  /// Update the number of bytes loaded & stored from memory operations.
  virtual void analyze(const Instruction *I, const DataLayout *DL) {
    Type *Ty;

    // TODO more advanced analysis, e.g., register pressure heuristics?
    // TODO do extractelement, insertelement, shufflevector, extractvalue, or
    // insertvalue read/write memory?
    // TODO Need to handle the following instructions/instrinsics (also see
    // Instruction::mayLoad() / Instruction::mayStore()):
    //   cmpxchg
    //   atomicrmw
    //   llvm.masked.load
    //   llvm.masked.store
    //   llvm.masked.gather
    //   llvm.masked.store
    switch(I->getOpcode()) {
    default: break;
    case Instruction::Load: {
      const LoadInst *LI = cast<LoadInst>(I);
      Ty = LI->getPointerOperand()->getType()->getPointerElementType();
      LoadBytes += DL->getTypeStoreSize(Ty);
      break;
    }

    case Instruction::Store: {
      const StoreInst *SI = cast<StoreInst>(I);
      Ty = SI->getValueOperand()->getType();
      StoreBytes += DL->getTypeStoreSize(Ty);
      break;
    }

    case Instruction::Call: {
      const IntrinsicInst *II = dyn_cast<IntrinsicInst>(I);
      bool Loads = false, Stores = false;
      int64_t Size = 0;

      if(!II) break;

      switch(II->getIntrinsicID()) {
      default: break;
      case Intrinsic::memcpy:
      case Intrinsic::memmove:
        // Arguments: i8* dest, i8* src, i<x> len, i32 align, i1 isvolatile
        Loads = Stores = true;
        Size = getValueSize(II->getArgOperand(2));
        break;
      case Intrinsic::memset:
        // Arguments: i8* dest, i8 val, i<x> len, i32 align, i1 isvolatile
        Stores = true;
        Size = getValueSize(II->getArgOperand(2));
        break;
      }

      // Negative size means we can't statically determine copy size
      // TODO how do we account for unknown sizes?
      if(Size > 0) {
        if(Loads) LoadBytes += Size;
        if(Stores) StoreBytes += Size;
      }

      break;
    }
    }
  }

  /// Return true if we think we're going to overflow the load or store
  /// buffer, false otherwise.
  virtual bool shouldAddMigPoint() const {
    // TODO some tolerance threshold, i.e., load buf size +- 10%?
    if(LoadBytes > HTMReadBufSize || StoreBytes > HTMWriteBufSize) return true;
    else return false;
  }

  virtual void reset() {
    Resets++;
    ResetLoad += LoadBytes;
    ResetStore += StoreBytes;
    LoadBytes = StoreBytes = 0;
  }

  /// The max value for HTM weights of predecessors is the max of potential
  /// load and store bytes over all predecessors.
  virtual void mergeMax(const Weight *RHS) {
    assert(RHS->isHTMWeight() && "Cannot mix weight types");
    const HTMWeight *W = (const HTMWeight *)RHS;
    if(W->LoadBytes > LoadBytes) LoadBytes = W->LoadBytes;
    if(W->StoreBytes > StoreBytes) StoreBytes = W->StoreBytes;
  }

  virtual void mergeMax(const std::unique_ptr<Weight> &RHS)
  { mergeMax(RHS.get()); }

  virtual void scale(size_t factor) {
    LoadBytes *= factor;
    StoreBytes *= factor;
  }

  /// The number of times this weight's load & stores could be executed without
  /// overflowing the HTM buffers.
  virtual size_t numIters() const {
    size_t NumLoadIters = UINT64_MAX, NumStoreIters = UINT64_MAX;
    if(LoadBytes) NumLoadIters = HTMReadBufSize / LoadBytes;
    if(StoreBytes) NumStoreIters = HTMWriteBufSize / StoreBytes;
    return NumLoadIters < NumStoreIters ? NumLoadIters : NumStoreIters;
  }

  virtual bool underPercentOfThreshold(unsigned percent) const {
    assert(percent <= 100 && "Invalid percentage");
    float fppercent = (float)percent / 100.0f;
    if((float)LoadBytes < ((float)HTMReadBufSize * fppercent) &&
       (float)StoreBytes < ((float)HTMWriteBufSize * fppercent))
      return true;
    else return false;
  }

  virtual std::string toString() const {
    return std::to_string(LoadBytes) + " byte(s) loaded, " +
           std::to_string(StoreBytes) + " byte(s) stored";
  }
};

// TODO non-HTM weight implementation

typedef std::unique_ptr<Weight> WeightPtr;

/// MigrationPoints - insert migration points into functions, optionally adding
/// HTM execution.
class MigrationPoints : public FunctionPass
{
public:
  static char ID;

  MigrationPoints() : FunctionPass(ID) {}
  ~MigrationPoints() {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolution>();
  }

  virtual const char *getPassName() const
  { return "Insert migration points"; }

  virtual bool doInitialization(Module &M) {
    bool modified = false;
    DL = &M.getDataLayout();

    // Ensure HTM is supported on this architecture if attempting to instrument
    // with transactional execution, otherwise disable it and warn the user
    if(HTMExec) {
      Triple TheTriple(M.getTargetTriple());
      Arch = TheTriple.getArch();

      if(HTMBegin.find(Arch) == HTMBegin.end()) {
        std::string Msg("HTM instrumentation not supported for '");
        Msg += TheTriple.getArchName();
        Msg += "'";
        DiagnosticInfoInlineAsm DI(Msg, DiagnosticSeverity::DS_Warning);
        M.getContext().diagnose(DI);
        return modified;
      }

      HTMBeginDecl = Intrinsic::getDeclaration(&M, HTMBegin.find(Arch)->second);
      HTMEndDecl = Intrinsic::getDeclaration(&M, HTMEnd.find(Arch)->second);
      HTMTestDecl = Intrinsic::getDeclaration(&M, HTMTest.find(Arch)->second);
      modified = true;
    }

    return modified;
  }

  /// Insert migration points into functions
  virtual bool runOnFunction(Function &F)
  {
    DEBUG(dbgs() << "\n********** ADD MIGRATION POINTS **********\n"
                 << "********** Function: " << F.getName() << "\n\n");

    initializeAnalysis(F);

    // Mark function entry point.  Regardless if we're placing more migration
    // points in the function, we assume that function calls are migration
    // points in caller, so we might as well add one in the callee body.
    DEBUG(dbgs() << "-> Marking function entry as a migration point <-\n");
    markAsMigPoint(F.getEntryBlock().getFirstInsertionPt(), true, true);

    // Some libc functions (e.g., I/O) will cause aborts from system calls.
    // Instrument libc calls to stop & resume transactions afterwards.
    if(DoHTMInstrumentation && !NoWrapLibc) wrapLibcWithHTM(F);

    if(MoreMigPoints) analyzeFunctionBody(F);
    else // Instrument function exit point(s)
      for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++)
        if(isa<ReturnInst>(BB->getTerminator()))
          markAsMigPoint(BB->getTerminator(), true, true);

    // Finally, apply code transformations to marked functions
    addMigrationPoints(F);

    NumMigPoints += NumMigPointAdded;
    NumHTMBegins += NumHTMBeginAdded;
    NumHTMEnds += NumHTMEndAdded;
    return NumMigPointAdded > 0 || NumHTMBeginAdded > 0 || NumHTMEndAdded > 0;
  }

  /// Reset all analysis.
  virtual void initializeAnalysis(const Function &F) {
    NumMigPointAdded = 0;
    NumHTMBeginAdded = 0;
    NumHTMEndAdded = 0;
    BBWeight.clear();
    LoopWeight.clear();
    LoopMigPoints.clear();
    MigPointInsts.clear();
    HTMBeginInsts.clear();
    HTMEndInsts.clear();

    // We've checked at a global scope whether the architecture supports HTM,
    // but we need to check whether the target-specific feature for HTM is
    // enabled for the current function
    if(!F.hasFnAttribute("target-features")) {
      DoHTMInstrumentation = false;
      return;
    }

    Attribute TargetAttr = F.getFnAttribute("target-features");
    assert(TargetAttr.isStringAttribute() && "Invalid target features");
    StringRef AttrVal = TargetAttr.getValueAsString();
    size_t pos = StringRef::npos;

    switch(Arch) {
    case Triple::ppc64le: pos = AttrVal.find("+htm"); break;
    case Triple::x86_64: pos = AttrVal.find("+rtm"); break;
    default: break;
    }
    DoHTMInstrumentation = HTMExec && (pos != StringRef::npos);

    DEBUG(
      if(DoHTMInstrumentation) dbgs() << "-> Enabling HTM instrumentation\n";
      else if(HTMExec) dbgs() << "-> Disabled HTM instrumentation, "
                                 "no target-features support\n";
    );
  }

private:
  //===--------------------------------------------------------------------===//
  // Types & fields
  //===--------------------------------------------------------------------===//

  /// Number of various types of instrumentation added to the function
  size_t NumMigPointAdded;
  size_t NumHTMBeginAdded;
  size_t NumHTMEndAdded;

  /// Should we instrument code with HTM execution?  Set if HTM is enabled on
  /// the command line and if the target is supported
  bool DoHTMInstrumentation;

  /// The current architecture - used to access architecture-specific HTM calls
  Triple::ArchType Arch;
  const DataLayout *DL;

  /// Function declarations for HTM intrinsics
  Value *HTMBeginDecl;
  Value *HTMEndDecl;
  Value *HTMTestDecl;

  /// Per-architecture LLVM intrinsic IDs for HTM begin, HTM end, and testing
  /// if executing transactionally
  typedef std::map<Triple::ArchType, Intrinsic::ID> IntrinsicMap;
  const static IntrinsicMap HTMBegin;
  const static IntrinsicMap HTMEnd;
  const static IntrinsicMap HTMTest;

  /// libc functions which are likely to cause an HTM abort through a syscall
  // TODO LLVM has to have a better way to detect these
  const static StringSet<> LibcIO;

  /// Weight information for basic blocks
  struct BasicBlockWeightInfo {
  public:
    /// Weight of the basic block at the end of its execution.  Note that if
    /// the block is instrumented with an migration point, the weight
    /// information *only* captures the instructions following the migration
    /// point (migration points "reset" the weight).
    WeightPtr BlockWeight;

    BasicBlockWeightInfo(const Weight *BlockWeight)
      : BlockWeight(BlockWeight->copy()) {}
    BasicBlockWeightInfo(const WeightPtr &BlockWeight)
      : BlockWeight(BlockWeight->copy()) {}

    std::string toString() const { return BlockWeight->toString(); }
  };

  /// Weight information for loops
  struct LoopWeightInfo {
  public:
    /// Weight a single iteration of a loop, based on the "heaviest" path
    /// through the loop.
    WeightPtr IterWeight;

    /// The number of iterations between consecutive migration points, e.g.,
    /// a value of 5 means there's an migration point every 5 iterations.
    size_t ItersPerMigPoint;

    /// True if we placed or found an migration point inside the loop's body
    bool MigPointInBody;

    LoopWeightInfo(const Weight *IterWeight,
                   size_t ItersPerMigPoint,
                   bool MigPointInBody)
      : IterWeight(IterWeight->copy()),
        ItersPerMigPoint(MigPointInBody ? 1 : ItersPerMigPoint),
        MigPointInBody(MigPointInBody) {}
    LoopWeightInfo(const WeightPtr &IterWeight,
                   size_t ItersPerMigPoint,
                   bool MigPointInBody)
      : IterWeight(IterWeight->copy()),
        ItersPerMigPoint(MigPointInBody ? 1 : ItersPerMigPoint),
        MigPointInBody(MigPointInBody) {}

    std::string toString() const {
      return IterWeight->toString() + ", " + std::to_string(ItersPerMigPoint) +
             " iteration(s) per migration point";
    }
  };

  /// Get a weight object with zero-initialized weight based on the type of
  /// analysis being used to instrument the application
  Weight *getZeroWeight() const {
    if(DoHTMInstrumentation) return new HTMWeight();
    else llvm_unreachable("Unknown weight type");
  }

  /// Weight information gathered by analyses for basic blocks & loops
  typedef std::map<const BasicBlock *, BasicBlockWeightInfo> BlockWeightMap;
  typedef std::map<const Loop *, LoopWeightInfo> LoopWeightMap;
  BlockWeightMap BBWeight;
  LoopWeightMap LoopWeight;

  /// Code locations marked for instrumentation.
  SmallPtrSet<Loop *, 16> LoopMigPoints;
  SmallPtrSet<Instruction *, 32> MigPointInsts;
  SmallPtrSet<Instruction *, 32> HTMBeginInsts;
  SmallPtrSet<Instruction *, 32> HTMEndInsts;

  //===--------------------------------------------------------------------===//
  // Analysis implementation
  //===--------------------------------------------------------------------===//

  /// Return whether the call instruction is a libc I/O call
  static inline bool isLibcIO(const Instruction *I) {
    if(!I || !isa<CallInst>(I)) return false;
    const Function *CalledFunc = cast<CallInst>(I)->getCalledFunction();
    if(CalledFunc && CalledFunc->hasName())
      return LibcIO.find(CalledFunc->getName()) != LibcIO.end();
    return false;
  }

  /// Return whether the instruction requires HTM begin instrumentation.
  bool shouldAddHTMBegin(Instruction *I) { return HTMBeginInsts.count(I); }

  /// Return whether the instruction requires HTM end instrumentation.
  bool shouldAddHTMEnd(Instruction *I) { return HTMEndInsts.count(I); }

  /// Return whether the instruction is a migration point.  We assume that all
  /// called functions have migration points internally.
  bool isMigrationPoint(Instruction *I) {
    if((isa<CallInst>(I) || isa<InvokeInst>(I)) && !isa<IntrinsicInst>(I))
      return true;
    else return MigPointInsts.count(I);
  }

  /// Mark an instruction to be instrumented with an HTM begin, directly before
  /// the instruction
  void markAsHTMBegin(Instruction *I) {
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as HTM begin\n");
    HTMBeginInsts.insert(I);
  }

  /// Mark an instruction to be instrumented with an HTM end, directly before
  /// the instruction
  void markAsHTMEnd(Instruction *I) {
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as HTM end\n");
    HTMEndInsts.insert(I);
  }

  /// Mark an instruction as a migration point, directly before the
  /// instruction.  Optionally mark instruction as needing HTM start/stop
  /// intrinsics.
  void markAsMigPoint(Instruction *I, bool AddHTMBegin, bool AddHTMEnd) {
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as a migration point\n");
    MigPointInsts.insert(I);
    if(AddHTMBegin) markAsHTMBegin(I);
    if(AddHTMEnd) markAsHTMEnd(I);
  }

  /// Search for & bookend libc functions which are likely to cause an HTM
  /// abort with HTM stop/start intrinsics.
  void wrapLibcWithHTM(Function &F) {
    DEBUG(dbgs() << "\n-> Wrapping I/O functions with HTM stop/start <-\n");
    for(Function::iterator BB = F.begin(), BE = F.end(); BB != BE; BB++) {
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++) {
        if(isLibcIO(I)) {
          markAsHTMEnd(I);

          // Search subsequent instructions for other libc calls to prevent
          // pathological transaction stop/starts.
          const static size_t searchSpan = 10;
          BasicBlock::iterator NextI(I->getNextNode());
          for(size_t rem = searchSpan; rem > 0 && NextI != E; rem--, NextI++) {
            if(isLibcIO(NextI)) {
              DEBUG(dbgs() << "  - Found another libc call"; NextI->dump());
              I = NextI;
              rem = searchSpan;
            }
          }

          // TODO analyze successor blocks as well

          markAsMigPoint(I->getNextNode(), true, false);
        }
      }
    }
  }

  /// Get the starting weight for a basic block based on the merged max ending
  /// weights of its predecessors.
  Weight *getInitialWeight(const BasicBlock *BB,
                           const LoopInfo &LI) const {
    Weight *PredWeight = getZeroWeight();
    const Loop *L = LI[BB];

    for(auto Pred : predecessors(BB)) {
      const Loop *PredLoop = LI[Pred];
      if(PredLoop && PredLoop != L) {
        // TODO rather than trying to determine if there's a migration point
        // between the loop's header and the exit block (and hence whether we
        // should only analyze the weight from the migration point to the
        // exit), just assume we're doing one extra full iteration
        assert(LoopWeight.find(PredLoop) != LoopWeight.end() &&
               "Invalid reverse post-order traversal");
        const LoopWeightInfo &LWI = LoopWeight.find(PredLoop)->second;
        WeightPtr Tmp(LWI.IterWeight->copy());
        Tmp->scale(LWI.ItersPerMigPoint + 1);
        PredWeight->mergeMax(Tmp);
      }
      else {
        assert(BBWeight.find(Pred) != BBWeight.end() &&
               "Invalid reverse post-order traversal");
        PredWeight->mergeMax(BBWeight.at(Pred).BlockWeight);
      }
    }

    return PredWeight;
  }

  /// Analyze a single basic block with an initial starting weight.  Return
  /// true if we placed (or there is an existing) migration point inside
  /// the block.
  bool traverseBlock(BasicBlock *BB, const Weight *Initial) {
    bool hasMigPoint = false;
    BBWeight.emplace(BB, BasicBlockWeightInfo(Initial));
    WeightPtr &CurWeight = BBWeight.at(BB).BlockWeight;

    DEBUG(
      dbgs() << "\nAnalyzing basic block";
      if(BB->hasName()) dbgs() << " '" << BB->getName() << "'";
      dbgs() << "\n";
    );

    for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++) {
      if(isa<PHINode>(I)) continue;

      // Check if there is or there should be a migration point before the
      // instruction, and if so, reset the weight.  This looks a little funky
      // because we don't want to tamper with existing instrumentation, only
      // add a new equivalence point w/ HTM if it's not already there.
      if(isMigrationPoint(I)) goto reset_weight;
      else if(CurWeight->shouldAddMigPoint()) goto mark_mig_point;
      else goto end;

    mark_mig_point:
      markAsMigPoint(I, true, true);
    reset_weight:
      CurWeight->reset();
      hasMigPoint = true;
    end:
      CurWeight->analyze(I, DL);
    }

    DEBUG(dbgs() << "  Weight: " << CurWeight->toString() << "\n");

    return hasMigPoint;
  }

  bool traverseBlock(BasicBlock *BB, const WeightPtr &Initial)
  { return traverseBlock(BB, Initial.get()); }

  /// Sort loops based on nesting depth, with deeper-nested loops coming first.
  /// If the depths are equal, sort based on pointer value so that distinct
  /// loops with equal depths are not considered equivalent during insertion.
  struct LoopNestCmp {
    bool operator() (const Loop * const &A, const Loop * const &B)
    {
      unsigned DepthA = A->getLoopDepth(), DepthB = B->getLoopDepth();
      if(DepthA > DepthB) return true;
      else if(DepthA < DepthB) return false;
      else return (uint64_t)A < (uint64_t)B;
    }
  };

  typedef std::set<Loop *, LoopNestCmp> LoopNest;

  /// Sort loops in a loop nest by their nesting depth to traverse inside-out.
  static void sortLoopsByDepth(const std::vector<BasicBlock *> &SCC,
                               LoopInfo &LI,
                               LoopNest &Nest) {
    Loop *L;
    std::queue<Loop *> ToVisit;

    // Grab the outermost loop in the nest to bootstrap indexing
    L = LI.getLoopFor(SCC.front());
    assert(L && "SCC was marked as having loop but none found in LoopInfo");
    while(L->getLoopDepth() > 1) L = L->getParentLoop();
    Nest.insert(L);
    ToVisit.push(L);

    // Find & index loops from innermost loop outwards
    while(!ToVisit.empty()) {
      L = ToVisit.front();
      ToVisit.pop();
      for(auto CurLoop : L->getSubLoops()) {
        Nest.insert(CurLoop);
        ToVisit.push(CurLoop);
      }
    }
  }

  /// Analyze loop nests & mark locations for migration points.
  void traverseLoopNest(const std::vector<BasicBlock *> &SCC,
                        LoopInfo &LI) {
    LoopNest Nest;
    sortLoopsByDepth(SCC, LI, Nest);

    // Walk loops & mark instructions at which we want migration points
    // TODO what about loops for which we have known numbers of iterations?
    // TODO what about loops which can be contained in a single transaction?
    for(auto CurLoop : Nest) {
      DEBUG(
        const BasicBlock *H = CurLoop->getHeader();
        dbgs() << "\nAnalyzing loop ";
        if(H->hasName()) dbgs() << "with header '" << H->getName() << "'";
        dbgs() << " (depth = " << std::to_string(CurLoop->getLoopDepth())
               << ")\n";
      );

      bool bodyHasMigPoint;
      BasicBlock *CurBB;
      LoopBlocksDFS DFS(CurLoop); DFS.perform(&LI);
      LoopBlocksDFS::RPOIterator Block = DFS.beginRPO(), E = DFS.endRPO();
      assert(Block != E && "Loop with no basic blocks");

      // Mark start of loop as migration point, set loop starting weight to
      // zero & analyze header
      // TODO what if its an irreducible loop, i.e., > 1 header?
      CurBB = *Block;
      WeightPtr TmpWeight(getZeroWeight());
      LoopMigPoints.insert(CurLoop);
      bodyHasMigPoint = traverseBlock(CurBB, TmpWeight);

      // Traverse the loop's blocks
      for(++Block; Block != E; ++Block) {
        CurBB = *Block;
        if(LI[CurBB] != CurLoop) continue; // Skip blocks in nested loops
        WeightPtr PredWeight(getInitialWeight(CurBB, LI));
        bodyHasMigPoint |= traverseBlock(CurBB, PredWeight);
      }

      // Calculate maximum iteration weight & add loop weight information
      SmallVector<BasicBlock *, 4> ExitBlocks;
      CurLoop->getExitingBlocks(ExitBlocks);
      for(auto Exit : ExitBlocks) {
        assert(LI[Exit] == CurLoop && "exiting from sub-loop?");
        assert(BBWeight.find(Exit) != BBWeight.end() &&
               "No weight information for exit basic block");
        TmpWeight->mergeMax(BBWeight.at(Exit).BlockWeight);
      }

      LoopWeight.emplace(CurLoop,
        LoopWeightInfo(TmpWeight, TmpWeight->numIters(), bodyHasMigPoint));

      DEBUG(dbgs() << "\nLoop analysis: "
                   << LoopWeight.at(CurLoop).toString() << "\n");
    }
  }

  /// Analyze the function's body to add migration points.
  void analyzeFunctionBody(Function &F) {
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

    // Start with loop nests, where the bulk of the instrumentation needs to
    // occur.  This will also affect where migration points are placed in
    // other parts of the function.
    DEBUG(dbgs() << "\n-> Analyzing loop nests <-\n");
    for(scc_iterator<Function *> SCC = scc_begin(&F), E = scc_end(&F);
        SCC != E; ++SCC)
      if(SCC.hasLoop()) traverseLoopNest(*SCC, LI);

    // Analyze the rest of the function body
    DEBUG(dbgs() << "\n-> Analyzing the rest of the function body <-\n");
    ReversePostOrderTraversal<Function *> RPOT(&F);
    for(auto BB = RPOT.begin(), BE = RPOT.end(); BB != BE; ++BB) {
      if(LI[*BB]) continue; // Skip loops
      WeightPtr PredWeight(getInitialWeight(*BB, LI));
      traverseBlock(*BB, PredWeight);
    }

    // Finally, determine if we should add an migration point at exit block(s)
    // TODO tune percent of threshold
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++) {
      if(isa<ReturnInst>(BB->getTerminator())) {
        assert(BBWeight.find(BB) != BBWeight.end() && "Missing block weight");
        const BasicBlockWeightInfo &BBWI = BBWeight.at(BB).BlockWeight;
        if(!BBWI.BlockWeight->underPercentOfThreshold(10))
          markAsMigPoint(BB->getTerminator(), true, true);
      }
    }
  }

  //===--------------------------------------------------------------------===//
  // Instrumentation implementation
  //===--------------------------------------------------------------------===//

  /// Transform a loop header so that migration points (and any concomitant
  /// costs) are only experienced every nth iteration, based on weight metrics
  void transformLoopHeader(Loop *L) {
    BasicBlock *Header = L->getHeader(), *NewSuccBB, *MigPointBB;
    size_t ItersPerMigPoint, LNum;

    // If the first instruction has already been marked, nothing to do
    Instruction *First = Header->getFirstInsertionPt();
    if(isMigrationPoint(First) ||
       shouldAddHTMEnd(First) ||
       shouldAddHTMBegin(First)) return;

    assert(LoopWeight.find(L) != LoopWeight.end() && "No loop analysis");
    ItersPerMigPoint = LoopWeight.find(L)->second.ItersPerMigPoint;
    PHINode *IV = L->getCanonicalInductionVariable();
    LNum = LoopsTransformed++;
    // TODO add our own induction variable?

    if(ItersPerMigPoint > 1 && IV) {
      DEBUG(
        dbgs() << "Instrumenting loop ";
        if(Header->hasName()) dbgs() << "header '" << Header->getName() << "' ";
        dbgs() << "to hit migration point every "
               << std::to_string(ItersPerMigPoint) << " iterations\n"
      );

      Type *IVType = IV->getType();
      Function *CurF = Header->getParent();
      LLVMContext &C = Header->getContext();

      // Create new successor for all instructions after migration point
      NewSuccBB =
        Header->splitBasicBlock(Header->getFirstInsertionPt(),
                                "l.postmigpoint" + std::to_string(LNum));

      // Create new block for migration point
      MigPointBB = BasicBlock::Create(C, "l.migpoint" + std::to_string(LNum),
                                     CurF, NewSuccBB);
      IRBuilder<> MigPointWorker(MigPointBB);
      Instruction *Br = cast<Instruction>(MigPointWorker.CreateBr(NewSuccBB));
      markAsMigPoint(Br, true, true);

      // Add check and branch to migration point only every nth iteration
      IRBuilder<> Worker(Header->getTerminator());
      Constant *N = ConstantInt::get(IVType, ItersPerMigPoint, false),
               *Zero = ConstantInt::get(IVType, 0, false);
      Value *Rem = Worker.CreateURem(IV, N);
      Value *Cmp = Worker.CreateICmpEQ(Rem, Zero);
      Worker.CreateCondBr(Cmp, MigPointBB, NewSuccBB);
      Header->getTerminator()->eraseFromParent();
    }
    else {
      DEBUG(
        dbgs() << "Instrumenting loop ";
        if(Header->hasName()) dbgs() << "header '" << Header->getName() << "' ";
        dbgs() << "to hit migration point every iteration";
        if(!IV) dbgs() << " (no loop induction variable)";
        dbgs() << "\n";
      );

      markAsMigPoint(Header->getFirstInsertionPt(), true, true);
    }
  }

  /// Add an migration point directly before an instruction.
  void addMigrationPoint(Instruction *I) {
    // TODO insert flag check & migration call if flag is set
  }

  // Note: because we're only supporting 2 architectures for now, we're not
  // going to abstract this out into the appropriate Target/* folders

  /// Add a transactional execution begin intrinsic for PowerPC, optionally
  /// with rollback-only transactions.
  void addPowerPCHTMBegin(Instruction *I) {
    LLVMContext &C = I->getContext();
    IRBuilder<> Worker(I);
    ConstantInt *ROT = ConstantInt::get(IntegerType::getInt32Ty(C),
                                        !NoROTPPC, false);
    Worker.CreateCall(HTMBeginDecl, ArrayRef<Value *>(ROT));
  }

  /// Add a transactional execution begin intrinsic for x86.
  void addX86HTMBegin(Instruction *I) {
    IRBuilder<> Worker(I);
    Worker.CreateCall(HTMBeginDecl);
  }

  /// Add transactional execution end intrinsic for PowerPC.
  void addPowerPCHTMEnd(Instruction *I) {
    LLVMContext &C = I->getContext();
    IRBuilder<> EndWorker(I);
    ConstantInt *Zero = ConstantInt::get(IntegerType::getInt32Ty(C),
                                         0, false);
    EndWorker.CreateCall(HTMEndDecl, ArrayRef<Value *>(Zero));
  }

  /// Add transactional execution check & end intrinsics for x86.
  void addX86HTMCheckAndEnd(Instruction *I) {
    // Note: x86's HTM facility will cause a segfault if an xend instruction is
    // called outside of a transaction, hence we need to check if we're in a
    // transaction before actually trying to end it.
    LLVMContext &C = I->getContext();
    BasicBlock *CurBB = I->getParent(), *NewSuccBB, *HTMEndBB;
    Function *CurF = CurBB->getParent();

    // Create a new successor which contains all instructions after the HTM
    // check & end
    NewSuccBB = CurBB->splitBasicBlock(I,
      ".htmendsucc" + std::to_string(NumMigPointAdded));

    // Create an HTM end block, which ends the transaction and jumps to the
    // new successor
    HTMEndBB = BasicBlock::Create(C,
      ".htmend" + std::to_string(NumMigPointAdded), CurF, NewSuccBB);
    IRBuilder<> EndWorker(HTMEndBB);
    EndWorker.CreateCall(HTMEndDecl);
    EndWorker.CreateBr(NewSuccBB);

    // Finally, add the HTM test & replace the unconditional branch created by
    // splitBasicBlock() with a conditional branch to either end the
    // transaction or continue on to the new successor
    IRBuilder<> PredWorker(CurBB->getTerminator());
    CallInst *HTMTestVal = PredWorker.CreateCall(HTMTestDecl);
    ConstantInt *Zero = ConstantInt::get(IntegerType::getInt32Ty(C), 0, true);
    Value *Cmp =
      PredWorker.CreateICmpNE(HTMTestVal, Zero,
                              "htmcmp" + std::to_string(NumMigPointAdded));
    PredWorker.CreateCondBr(Cmp, HTMEndBB, NewSuccBB);
    CurBB->getTerminator()->eraseFromParent();
  }

  /// Insert migration points & HTM instrumentation for instructions.
  void addMigrationPoints(Function &F) {
    DEBUG(dbgs() << "\n-> Instrumenting with migration points & HTM <-\n");

    for(auto Loop : LoopMigPoints) transformLoopHeader(Loop);

    for(auto I = MigPointInsts.begin(), E = MigPointInsts.end(); I != E; ++I) {
      addMigrationPoint(*I);
      NumMigPointAdded++;
    }

    if(DoHTMInstrumentation) {
      // Note: add the HTM ends before begins
      for(auto I = HTMEndInsts.begin(), E = HTMEndInsts.end(); I != E; ++I) {
        switch(Arch) {
        case Triple::ppc64le: addPowerPCHTMEnd(*I); break;
        case Triple::x86_64: addX86HTMCheckAndEnd(*I); break;
        default: llvm_unreachable("HTM -- unsupported architecture");
        }
        NumHTMEndAdded++;
      }

      for(auto I = HTMBeginInsts.begin(), E = HTMBeginInsts.end();
          I != E; ++I) {
        switch(Arch) {
        case Triple::ppc64le: addPowerPCHTMBegin(*I); break;
        case Triple::x86_64: addX86HTMBegin(*I); break;
        default: llvm_unreachable("HTM -- unsupported architecture");
        }
        NumHTMBeginAdded++;
      }
    }
  }
};

} /* end anonymous namespace */

char MigrationPoints::ID = 0;

const std::map<Triple::ArchType, Intrinsic::ID> MigrationPoints::HTMBegin = {
  {Triple::x86_64, Intrinsic::x86_xbegin},
  {Triple::ppc64le, Intrinsic::ppc_tbegin}
};

const std::map<Triple::ArchType, Intrinsic::ID> MigrationPoints::HTMEnd = {
  {Triple::x86_64, Intrinsic::x86_xend},
  {Triple::ppc64le, Intrinsic::ppc_tend}
};

const std::map<Triple::ArchType, Intrinsic::ID> MigrationPoints::HTMTest = {
  {Triple::x86_64, Intrinsic::x86_xtest},
  {Triple::ppc64le, Intrinsic::ppc_ttest}
};

INITIALIZE_PASS_BEGIN(MigrationPoints, "migration-points",
                      "Insert migration points into functions",
                      true, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolution)
INITIALIZE_PASS_END(MigrationPoints, "migration-points",
                    "Insert migration points into functions",
                    true, false)

const StringSet<> MigrationPoints::LibcIO = {
  "fopen", "freopen", "fclose", "fflush", "fwide",
  "setbuf", "setvbuf", "fread", "fwrite",
  "fgetc", "getc", "fgets", "fputc", "putc", "fputs",
  "getchar", "gets", "putchar", "puts", "ungetc",
  "fgetwc", "getwc", "fgetws", "fputwc", "putwc", "fputws",
  "getwchar", "putwchar", "ungetwc",
  "scanf", "fscanf", "vscanf", "vfscanf",
  "printf", "fprintf", "vprintf", "vfprintf",
  "wscanf", "fwscanf", "vwscanf", "vfwscanf",
  "wprintf", "fwprintf", "vwprintf", "vfwprintf",
  "ftell", "fgetpos", "fseek", "fsetpos", "rewind",
  "clearerr", "feof", "ferror", "perror",
  "remove", "rename", "tmpfile", "tmpnam",
  "__isoc99_fscanf"
};

namespace llvm {
  FunctionPass *createMigrationPointsPass()
  { return new MigrationPoints(); }
}

