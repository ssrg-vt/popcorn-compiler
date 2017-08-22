//===- EquivalencePoints.cpp ----------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Instrument the code with equivalence points, defined as a location in the
// program code where there is a direct mapping between architecture-specific
// execution state, i.e., registers and stack, across different ISAs.  More
// details can be found in the paper "A Unified Model of Pointwise Equivalence
// of Procedural Computations" by von Bank et. al
// (http://dl.acm.org/citation.cfm?id=197402).
//
// By default, the pass only inserts equivalence points at the beginning and
// end of a function.  More advanced analyses can be used to instrument function
// bodies (in particular, loops) with more equivalence points.
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
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "equivalence-points"

/// Insert more equivalence points into the body of a function.  Analyze memory
/// usage & attempt to instrument the code to reduce the time until the thread
/// reaches an equivalence point.  If HTM instrumentation is enabled, analysis
/// is tailored to avoid hardware transactional memory (HTM) capacity aborts.
const static cl::opt<bool>
MoreEqPoints("more-eq-points", cl::Hidden, cl::init(false),
  cl::desc("Add additional equivalence points into the body of functions"));

/// Cover the application in transactional execution by inserting HTM
/// stop/start instructions at equivalence points.
const static cl::opt<bool>
HTMExec("htm-execution", cl::NotHidden, cl::init(false),
  cl::desc("Instrument equivalence points with HTM execution "
           "(only supported on PowerPC (64-bit) & x86-64)"));

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
/// equivalence points.
const static cl::opt<unsigned>
HTMReadBufSizeArg("htm-buf-read", cl::Hidden, cl::init(8),
  cl::desc("HTM analysis tuning - HTM read buffer size, in kilobytes"),
  cl::value_desc("size"));

/// HTM memory write buffer size for tuning analysis when inserting additional
/// equivalence points.
const static cl::opt<unsigned>
HTMWriteBufSizeArg("htm-buf-write", cl::Hidden, cl::init(8),
  cl::desc("HTM analysis tuning - HTM write buffer size, in kilobytes"),
  cl::value_desc("size"));

#define KB 1024
#define HTMReadBufSize (HTMReadBufSizeArg * KB)
#define HTMWriteBufSize (HTMWriteBufSizeArg * KB)

STATISTIC(NumEqPoints, "Number of equivalence points added");
STATISTIC(NumHTMBegins, "Number of HTM begin intrinsics added");
STATISTIC(NumHTMEnds, "Number of HTM end intrinsics added");
STATISTIC(LoopsTransformed, "Number of loops transformed");

namespace {

/// Weight metrics.  Child classes implement for different analyses.
class Weight {
public:
  virtual ~Weight() {};
  virtual Weight *copy() const = 0;

  /// Expose types of child implementations.
  virtual bool isHTMWeight() const { return false; }

  /// Analyze an instruction & update accounting.
  virtual void analyze(const Instruction *I) = 0;

  /// Return whether or not we should add an equivalence point.
  virtual bool shouldAddEqPoint() const = 0;

  /// Reset internal weights after finding or placing an equivalence point.
  virtual void reset() = 0;

  /// Merge weights of predecessors to get the maximum starting weight of a
  /// code section being analyzed.
  virtual void mergeMax(const Weight *RHS) = 0;
  virtual void mergeMax(const std::unique_ptr<Weight> &RHS) = 0;

  /// Scale the weight by a factor, e.g., a number of loop iterations.
  virtual void scale(size_t factor) = 0;

  /// Number of times this weight "fits" into a given resource before we need
  /// to place an equivalence point.  This is used for calculating how many
  /// iterations of a loop can be executed between equivalence points.
  virtual size_t numIters() const = 0;

  /// Return whether or not the weight is within some percent (0-100) of the
  /// threshold.
  virtual bool withinPercent(unsigned percent) = 0;

  /// Return a human-readable string describing weight information.
  virtual std::string toString() const = 0;
};

/// Weight metrics for HTM analysis, which basically depend on the number
/// of bytes loaded & stored.
class HTMWeight : public Weight {
private:
  // The number of bytes loaded & stored, respectively
  size_t LoadBytes, StoreBytes;

public:
  HTMWeight(size_t LoadBytes = 0, size_t StoreBytes = 0)
    : LoadBytes(LoadBytes), StoreBytes(StoreBytes) {}
  HTMWeight(const HTMWeight &C)
    : LoadBytes(C.LoadBytes), StoreBytes(C.StoreBytes) {}
  virtual Weight *copy() const { return new HTMWeight(*this); }

  virtual bool isHTMWeight() const { return true; }

  /// Update the number of bytes loaded & stored from memory operations.
  virtual void analyze(const Instruction *I) {
    // TODO more advanced analysis, e.g., register pressure heuristics?
    // TODO do extractelement, insertelement, shufflevector, extractvalue, or
    // insertvalue read/write memory?
    // TODO Need to handle the following instructions/instrinsics (also see
    // Instruction::mayLoad() / Instruction::mayStore()):
    //   cmpxchg
    //   atomicrmw
    //   llvm.memcpy
    //   llvm.memmove
    //   llvm.memset
    //   llvm.masked.load
    //   llvm.masked.store
    //   llvm.masked.gather
    //   llvm.masked.store
    if(isa<LoadInst>(I)) {
      const LoadInst *LI = cast<LoadInst>(I);
      const DataLayout &DL = I->getModule()->getDataLayout();
      Type *Ty = LI->getPointerOperand()->getType()->getPointerElementType();
      LoadBytes += DL.getTypeStoreSize(Ty);
    }
    else if(isa<StoreInst>(I)) {
      const StoreInst *SI = cast<StoreInst>(I);
      const DataLayout &DL = I->getModule()->getDataLayout();
      Type *Ty = SI->getPointerOperand()->getType()->getPointerElementType();
      StoreBytes += DL.getTypeStoreSize(Ty);
    }
  }

  /// Return true if we think we're going to overflow the load or store
  /// buffer, false otherwise.
  virtual bool shouldAddEqPoint() const {
    // TODO some tolerance threshold, i.e., load buf size +- 10%?
    if(LoadBytes > HTMReadBufSize || StoreBytes > HTMWriteBufSize) return true;
    else return false;
  }

  virtual void reset() { LoadBytes = StoreBytes = 0; }

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
    size_t NumLoadIters = HTMReadBufSize / LoadBytes,
           NumStoreIters = HTMWriteBufSize / StoreBytes;
    return NumLoadIters < NumStoreIters ? NumLoadIters : NumStoreIters;
  }

  virtual bool withinPercent(unsigned percent) {
    float fppercent = (float)percent / 100.0f;
    if((float)LoadBytes > ((float)HTMReadBufSize * fppercent)) return true;
    else if((float)StoreBytes > ((float)HTMWriteBufSize * fppercent)) return true;
    else return false;
  }

  virtual std::string toString() const {
    return std::to_string(LoadBytes) + " byte(s) loaded, " +
           std::to_string(StoreBytes) + " byte(s) stored";
  }
};

typedef std::unique_ptr<Weight> WeightPtr;

/// EquivalencePoints - insert equivalence points into functions, optionally
/// adding HTM execution.
class EquivalencePoints : public FunctionPass
{
public:
  static char ID;

  EquivalencePoints() : FunctionPass(ID) {}
  ~EquivalencePoints() {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const
  { AU.addRequired<LoopInfoWrapperPass>(); }

  virtual const char *getPassName() const
  { return "Insert equivalence points"; }

  virtual bool doInitialization(Module &M) {
    bool modified = false;

    // Ensure HTM is supported on this architecture if attempting to instrument
    // with transactional execution, otherwise disable it and warn the user
    DoHTMInstrumentation = HTMExec;
    if(DoHTMInstrumentation) {
      Triple TheTriple(M.getTargetTriple());
      Arch = TheTriple.getArch();

      if(HTMBegin.find(Arch) != HTMBegin.end()) {
        HTMBeginDecl = addIntrinsicDecl(M, HTMBegin);
        HTMEndDecl = addIntrinsicDecl(M, HTMEnd);
        HTMTestDecl = addIntrinsicDecl(M, HTMTest);
        modified = true;
      }
      else {
        std::string Msg("HTM instrumentation not supported for '");
        Msg += TheTriple.getArchName();
        Msg += "'";
        DiagnosticInfoInlineAsm DI(Msg, DiagnosticSeverity::DS_Warning);
        M.getContext().diagnose(DI);
        DoHTMInstrumentation = false;
      }
    }

    return modified;
  }

  /// Insert equivalence points into functions
  virtual bool runOnFunction(Function &F)
  {
    NumEqPointAdded = 0;
    NumHTMBeginAdded = 0;
    NumHTMEndAdded = 0;

    DEBUG(dbgs() << "\n********** ADD EQUIVALENCE POINTS **********\n"
                 << "********** Function: " << F.getName() << "\n\n");

    // TODO if doing HTM instrumentation, need to check for HTM attributes,
    // e.g., "+rtm" on Intel and "+htm" on POWER8

    // Mark function entry point.  Regardless if we're placing more equivalence
    // points in the function, we assume that function calls are equivalence
    // points in caller, so we might as well add one in the callee body.
    DEBUG(dbgs() << "-> Marking entry as equivalence point <-\n");
    markAsEqPoint(F.getEntryBlock().getFirstInsertionPt(), true, true);

    // Some libc functions (e.g., I/O) will cause aborts from system calls.
    // Instrument libc calls to stop & resume transactions afterwards.
    if(DoHTMInstrumentation && !NoWrapLibc) wrapLibcWithHTM(F);

    if(MoreEqPoints) analyzeFunctionBody(F);
    else // Instrument function exit point(s)
      for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++)
        if(isa<ReturnInst>(BB->getTerminator()))
          markAsEqPoint(BB->getTerminator(), true, true);

    // Finally, apply code transformations to marked functions
    addEquivalencePoints(F);

    NumEqPoints += NumEqPointAdded;
    NumHTMBegins += NumHTMBeginAdded;
    NumHTMEnds += NumHTMEndAdded;
    return NumEqPointAdded > 0 || NumHTMBeginAdded > 0 || NumHTMEndAdded > 0;
  }

private:
  //===--------------------------------------------------------------------===//
  // Types & fields
  //===--------------------------------------------------------------------===//

  /// Number of various types of instrumentation added to the function
  size_t NumEqPointAdded;
  size_t NumHTMBeginAdded;
  size_t NumHTMEndAdded;

  /// Should we instrument code with HTM execution?  Set if HTM is enabled on
  /// the command line and if the target is supported
  bool DoHTMInstrumentation;

  /// The current architecture - used to access architecture-specific HTM calls
  Triple::ArchType Arch;

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
    /// the block is instrumented with an equivalence point, the weight
    /// information *only* captures the instructions following the equivalence
    /// point (equivalence points "reset" the weight).
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

    /// The number of iterations between consecutive equivalence points, e.g.,
    /// a value of 5 means there's an equivalence point every 5 iterations.
    size_t ItersPerEqPoint;

    /// True if we placed or found an equivalence point inside the loop's body
    bool EqPointInBody;

    LoopWeightInfo(const Weight *IterWeight,
                   size_t ItersPerEqPoint,
                   bool EqPointInBody)
      : IterWeight(IterWeight->copy()),
        ItersPerEqPoint(EqPointInBody ? 1 : ItersPerEqPoint),
        EqPointInBody(EqPointInBody) {}
    LoopWeightInfo(const WeightPtr &IterWeight,
                   size_t ItersPerEqPoint,
                   bool EqPointInBody)
      : IterWeight(IterWeight->copy()),
        ItersPerEqPoint(EqPointInBody ? 1 : ItersPerEqPoint),
        EqPointInBody(EqPointInBody) {}

    std::string toString() const {
      return IterWeight->toString() + ", " + std::to_string(ItersPerEqPoint) +
             " iteration(s) per equivalence point";
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
  SmallPtrSet<Loop *, 16> LoopEqPoints;
  SmallPtrSet<Instruction *, 32> EqPointInsts;
  SmallPtrSet<Instruction *, 32> HTMBeginInsts;
  SmallPtrSet<Instruction *, 32> HTMEndInsts;

  //===--------------------------------------------------------------------===//
  // Analysis implementation
  //===--------------------------------------------------------------------===//

  /// Return whether the call instruction is a libc I/O call
  static inline bool isLibcIO(const Instruction *I) {
    if(!I || !isa<CallInst>(I)) return false;
    const CallInst *CI = cast<CallInst>(I);
    const Function *CalledFunc = CI->getCalledFunction();
    if(CalledFunc && CalledFunc->hasName()) {
      const StringRef Name = CalledFunc->getName();
      return LibcIO.find(Name) != LibcIO.end();
    }
    return false;
  }

  /// Return whether the instruction requires HTM begin instrumentation.
  bool shouldAddHTMBegin(Instruction *I) { return HTMBeginInsts.count(I); }

  /// Return whether the instruction requires HTM end instrumentation.
  bool shouldAddHTMEnd(Instruction *I) { return HTMEndInsts.count(I); }

  /// Return whether the instruction is an equivalence point, either by being
  /// marked through analysis or is by default (i.e., call instructions).
  bool isEqPoint(Instruction *I) {
    if(isa<CallInst>(I) || isa<InvokeInst>(I)) return true;
    else return EqPointInsts.count(I);
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

  /// Mark an instruction as an equivalence point, directly before the
  /// instruction.  Optionally mark instruction as needing HTM start/stop
  /// intrinsics.
  void markAsEqPoint(Instruction *I, bool AddHTMBegin, bool AddHTMEnd) {
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as an equivalence point\n");
    EqPointInsts.insert(I);
    if(AddHTMBegin) markAsHTMBegin(I);
    if(AddHTMEnd) markAsHTMEnd(I);
  }

  /// Mark a loop header as having
  void markLoopHeader(const Loop *L, bool AddHTMBegin, bool AddHTMEnd) {

  }

  /// Search for & bookend libc functions which are likely to cause an HTM
  /// abort with HTM stop/start intrinsics.
  void wrapLibcWithHTM(Function &F) {
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
              I = NextI;
              rem = searchSpan;
            }
          }
          markAsEqPoint(I->getNextNode(), true, false);
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
        // TODO rather than trying to determine if there's an equivalence point
        // between the loop's header and the exit block (and hence whether we
        // should only analyze the weight from the equivalence point to the
        // exit), just assume we're doing one extra full iteration
        assert(LoopWeight.find(PredLoop) != LoopWeight.end() &&
               "Invalid reverse post-order traversal");
        const LoopWeightInfo &LWI = LoopWeight.find(PredLoop)->second;
        WeightPtr Tmp(LWI.IterWeight->copy());
        Tmp->scale(LWI.ItersPerEqPoint + 1);
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
  /// true if we placed (or there is an existing) equivalence point inside
  /// the block.
  bool traverseBlock(BasicBlock *BB, const Weight *Initial) {
    bool hasEqPoint = false;
    BBWeight.emplace(BB, BasicBlockWeightInfo(Initial));
    WeightPtr &CurWeight = BBWeight.at(BB).BlockWeight;

    for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++) {
      CurWeight->analyze(I);
      if(isEqPoint(I)) {
        CurWeight->reset();
        hasEqPoint = true;
      }
      else if(CurWeight->shouldAddEqPoint()) {
        markAsEqPoint(I, true, true);
        CurWeight->reset();
        hasEqPoint = true;
      }
    }

    DEBUG(
      dbgs() << "\nBasic block ";
      if(BB->hasName()) dbgs() << "'" << BB->getName() << "' ";
      dbgs() << "weight: " << CurWeight->toString() << "\n";
    );

    return hasEqPoint;
  }

  bool traverseBlock(BasicBlock *BB, const WeightPtr &Initial)
  { return traverseBlock(BB, Initial.get()); }

  /// Sort loops based on nesting depth, i.e., deeper-nested loops come first
  struct LoopNestCmp {
    bool operator() (const Loop * const &A, const Loop * const &B)
    { return A->getLoopDepth() > B->getLoopDepth(); }
  };

  /// Sort loops in a loop nest by their nesting depth to traverse inside-out.
  static void sortLoopsByDepth(const std::vector<BasicBlock *> &SCC,
                               LoopInfo &LI,
                               std::set<Loop *, LoopNestCmp> &Nest) {
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

  /// Analyze loop nests & mark locations for equivalence points.
  void traverseLoopNest(const std::vector<BasicBlock *> &SCC,
                        LoopInfo &LI) {
    std::set<Loop *, LoopNestCmp> Nest;
    sortLoopsByDepth(SCC, LI, Nest);

    // Walk loops & mark instructions at which we want equivalence points
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

      bool bodyHasEqPoint;
      BasicBlock *CurBB;
      LoopBlocksDFS DFS(CurLoop); DFS.perform(&LI);
      LoopBlocksDFS::RPOIterator Block = DFS.beginRPO(), E = DFS.endRPO();
      assert(Block != E && "Loop with no basic blocks");

      // Mark start of loop as equivalence point, set loop starting weight to
      // zero & analyze header
      // TODO what if its an irreducible loop, i.e., > 1 header?
      CurBB = *Block;
      WeightPtr TmpWeight(getZeroWeight());
      LoopEqPoints.insert(CurLoop);
      bodyHasEqPoint = traverseBlock(CurBB, TmpWeight);

      // Traverse the loop's blocks
      for(++Block; Block != E; ++Block) {
        CurBB = *Block;
        if(LI[CurBB] != CurLoop) continue; // Skip blocks in nested loops
        WeightPtr PredWeight(getInitialWeight(CurBB, LI));
        bodyHasEqPoint |= traverseBlock(CurBB, PredWeight);
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
        LoopWeightInfo(TmpWeight, TmpWeight->numIters(), bodyHasEqPoint));

      DEBUG(dbgs() << "\nLoop analysis: "
                   << LoopWeight.at(CurLoop).toString() << "\n");
    }
  }

  /// Analyze the function's body to add equivalence points.
  void analyzeFunctionBody(Function &F) {
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

    // Start with loop nests, where the bulk of the instrumentation needs to
    // occur.  This will also affect where equivalence points are placed in
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

    // Finally, determine if we should add an equivalence point at exit block(s)
    // TODO tune threshold
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++) {
      if(isa<ReturnInst>(BB->getTerminator())) {
        assert(BBWeight.find(BB) != BBWeight.end() && "Missing block weight");
        const BasicBlockWeightInfo &BBWI = BBWeight.at(BB).BlockWeight;
        if(BBWI.BlockWeight->withinPercent(20))
          markAsEqPoint(BB->getTerminator(), true, true);
      }
    }
  }

  //===--------------------------------------------------------------------===//
  // Instrumentation implementation
  //===--------------------------------------------------------------------===//

  /// Add a declaration for an architecture-specific intrinsic (contained in
  /// the map).
  Constant *addIntrinsicDecl(Module &M, const IntrinsicMap &Map) {
    IntrinsicMap::const_iterator It = Map.find(Arch);
    assert(It != Map.end() && "Unsupported architecture");
    FunctionType *FuncTy = Intrinsic::getType(M.getContext(), It->second);
    return M.getOrInsertFunction(Intrinsic::getName(It->second), FuncTy);
  }

  /// Transform a loop header so that equivalence points (and any concomitant 
  /// costs) are only experienced every nth iteration, based on weight metrics
  void transformLoopHeader(Loop *L) {
    assert(LoopWeight.find(L) != LoopWeight.end() && "No loop analysis");
    const size_t LNum = LoopsTransformed++;
    size_t ItersPerEqPoint = LoopWeight.find(L)->second.ItersPerEqPoint;
    BasicBlock *Header = L->getHeader(), *NewSuccBB, *EqPointBB;
    PHINode *IV = L->getCanonicalInductionVariable();
    // TODO add our own IV?

    if(IV && ItersPerEqPoint > 1) {
      // Only encounter equivalence point every nth iteration
      DEBUG(
        dbgs() << "Instrumenting loop ";
        if(Header->hasName()) dbgs() << "header '" << Header->getName() << "' ";
        dbgs() << "to hit equivalence point every "
               << std::to_string(ItersPerEqPoint) << " iterations\n"
      );

      Type *IVType = IV->getType();
      Function *CurF = Header->getParent();
      LLVMContext &C = Header->getContext();

      // Create new successor for all instructions after equivalence point
      NewSuccBB =
        Header->splitBasicBlock(Header->getFirstInsertionPt(),
                                "l.posteqpoint" + std::to_string(LNum));

      // Create new block for equivalence point
      EqPointBB = BasicBlock::Create(C, "l.eqpoint" + std::to_string(LNum),
                                     CurF, NewSuccBB);
      IRBuilder<> EqPointWorker(EqPointBB);
      Instruction *Br = cast<Instruction>(EqPointWorker.CreateBr(NewSuccBB));
      markAsEqPoint(Br, true, true);

      // Add check and branch to equivalence point only every nth iteration
      IRBuilder<> Worker(Header->getTerminator());
      Constant *N = ConstantInt::get(IVType, ItersPerEqPoint, false),
               *Zero = ConstantInt::get(IVType, 0, false);
      Value *Rem = Worker.CreateURem(IV, N, "");
      Value *Cmp = Worker.CreateICmpEQ(Rem, Zero, "");
      Worker.CreateCondBr(Cmp, EqPointBB, NewSuccBB);
      Header->getTerminator()->eraseFromParent();
    }
    else {
      // Encounter equivalence point every iteration
      DEBUG(
        dbgs() << "Instrumenting loop ";
        if(Header->hasName()) dbgs() << "header '" << Header->getName() << "' ";
        dbgs() << "to hit equivalence point every iteration"
      );
      markAsEqPoint(Header->getFirstInsertionPt(), true, true);
    }
  }

  /// Add an equivalence point directly before an instruction.
  void addEquivalencePoint(Instruction *I) {
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
      ".htmendsucc" + std::to_string(NumEqPointAdded));

    // Create an HTM end block, which ends the transaction and jumps to the
    // new successor
    HTMEndBB = BasicBlock::Create(C,
      ".htmend" + std::to_string(NumEqPointAdded), CurF, NewSuccBB);
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
                              "htmcmp" + std::to_string(NumEqPointAdded));
    PredWorker.CreateCondBr(Cmp, HTMEndBB, NewSuccBB);
    CurBB->getTerminator()->eraseFromParent();
  }

  /// Insert equivalence points & HTM instrumentation for instructions.
  void addEquivalencePoints(Function &F) {
    DEBUG(dbgs() << "\n-> Instrumenting with equivalence points & HTM <-\n");

    for(auto Loop : LoopEqPoints) transformLoopHeader(Loop);

    for(auto I = EqPointInsts.begin(), E = EqPointInsts.end(); I != E; ++I) {
      addEquivalencePoint(*I);
      NumEqPointAdded++;
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

char EquivalencePoints::ID = 0;

const std::map<Triple::ArchType, Intrinsic::ID> EquivalencePoints::HTMBegin = {
  {Triple::x86_64, Intrinsic::x86_xbegin},
  {Triple::ppc64le, Intrinsic::ppc_tbegin}
};

const std::map<Triple::ArchType, Intrinsic::ID> EquivalencePoints::HTMEnd = {
  {Triple::x86_64, Intrinsic::x86_xend},
  {Triple::ppc64le, Intrinsic::ppc_tend}
};

const std::map<Triple::ArchType, Intrinsic::ID> EquivalencePoints::HTMTest = {
  {Triple::x86_64, Intrinsic::x86_xtest},
  {Triple::ppc64le, Intrinsic::ppc_ttest}
};

INITIALIZE_PASS_BEGIN(EquivalencePoints, "equivalence-points",
                      "Insert equivalence points into functions",
                      true, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(EquivalencePoints, "equivalence-points",
                    "Insert equivalence points into functions",
                    true, false)

const StringSet<> EquivalencePoints::LibcIO = {
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
  "remove", "rename", "tmpfile", "tmpnam"
};

namespace llvm {
  FunctionPass *createEquivalencePointsPass()
  { return new EquivalencePoints(); }
}

