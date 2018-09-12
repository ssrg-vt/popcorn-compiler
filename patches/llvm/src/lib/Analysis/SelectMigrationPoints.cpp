//===- SelectMigrationPoints.cpp ------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Select code locations to instrument with migration points, which are
// locations where threads make calls to invoke the migration process in
// addition to any other instrumentation (e.g., hardware transactional memory,
// HTM, stops & starts).  Migration points only occur at equivalence points, or
// locations in the program code where there is a direct mapping between
// architecture-specific execution state, like registers and stack, across
// different ISAs.  In our implementation, every function call site is an
// equivalence point; hence, calls inserted to invoke the migration by
// definition create equivalence points at the migration point.  Thus, all
// migration points are equivalence points, but not all equivalence points are
// migration points.
//
// By default, the pass only inserts migration points at the beginning and end
// of a function.  More advanced analyses can be used to instrument function
// bodies (in particular, loops) with more migration points and HTM execution.
//
// More details about equivalence points can be found in the paper "A Unified
// Model of Pointwise Migration of Procedural Computations" by von Bank et. al
// (http://dl.acm.org/citation.cfm?id=197402).
//
//===----------------------------------------------------------------------===//

#include <cmath>
#include <map>
#include <memory>
#include "llvm/Pass.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPaths.h"
#include "llvm/Analysis/PopcornUtil.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_os_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "migration-points"

/// Insert more migration points into the body of a function.  Analyze
/// execution behavior & attempt to instrument the code to reduce the time
/// until the thread reaches a migration point.
const static cl::opt<bool>
MoreMigPoints("more-mig-points", cl::Hidden, cl::init(false),
  cl::desc("Add additional migration points into the body of functions"));

/// By default we assume that loops will execute "enough iterations as to
/// require instrumentation".  That's not necessarily true, so contrain N in
/// hitting migration point every N iterations.  If analysis determines that
/// we need to hit analysis for some number larger than N, don't instrument
/// the loop.
const static cl::opt<unsigned>
MaxItersPerMigPoint("max-iters-per-migpoint", cl::Hidden, cl::init(UINT32_MAX),
  cl::desc("Max iterations per migration point"));

/// Percent of capacity (determined by analysis type, e.g., HTM buffer size) at
/// which point weight objects will request a new migration point be inserted.
const static cl::opt<unsigned>
CapacityThreshold("cap-threshold", cl::Hidden, cl::init(80),
  cl::desc("Percent of capacity at which point a new migration point should "
           "be inserted (only applies to -more-mig-points)"));

/// Per-function capacity threshold.
const static cl::list<std::string>
FuncCapThreshold("func-cap", cl::Hidden, cl::ZeroOrMore,
  cl::desc("Function-specific capacity threshold in function,value pairs"));

/// Normally we instrument function entry points with migration points.  If
/// we're below some percent of capacity at all exit points & we haven't added
/// instrumentation into the body (i.e., nothing depends on a clean slate to
/// start), skip this instrumentation.
const static cl::opt<unsigned>
StartThreshold("start-threshold", cl::Hidden, cl::init(5),
  cl::desc("Don't instrument function entry points under a percent of "
           "capacity (only applies to -more-mig-points), used for "
           "small functions"));

/// Per-function starting threshold.
const static cl::list<std::string>
FuncStartThreshold("func-start", cl::Hidden, cl::ZeroOrMore,
  cl::desc("Function-specific start threshold in function,value pairs"));

/// Normally we instrument function exit points with migration points.  If
/// we're below some percent of capacity, skip this instrumentation (useful for
/// very small/short-lived functions).
const static cl::opt<unsigned>
RetThreshold("ret-threshold", cl::Hidden, cl::init(5),
  cl::desc("Don't instrument function exit points under a percent of "
           "capacity (only applies to -more-mig-points)"));

/// Per-function return threshold.
const static cl::list<std::string>
FuncRetThreshold("func-ret", cl::Hidden, cl::ZeroOrMore,
  cl::desc("Function-specific return threshold in function,value pairs"));

/// Don't instrument a specific function with extra migration points.
const static cl::list<std::string>
FuncNoInst("func-no-inst", cl::Hidden, cl::ZeroOrMore,
  cl::desc("Don't instrument a particular function with migration points"));

/// Target cycles between migration points when instrumenting applications with
/// more migration points (but without HTM).  Allows tuning trade off between
/// migration point response time and overhead.
const static cl::opt<unsigned>
MillionCyclesBetweenMigPoints("migpoint-cycles", cl::Hidden, cl::init(50),
  cl::desc("Cycles between migration points, in millions of cycles"));

/// Cover the application in transactional execution by inserting HTM
/// stop/start instructions at migration points.  Tailors the analysis to
/// reduce capacity aborts by estimating memory access behavior.
const static cl::opt<bool>
HTMExec("htm-execution", cl::NotHidden, cl::init(false),
  cl::desc("Instrument migration points with HTM execution "
           "(only supported on PowerPC 64-bit & x86-64)"));

/// Disable wrapping mem<set, copy, move> instructions for which we don't know
/// the size.
const static cl::opt<bool>
NoWrapUnknownMem("htm-no-wrap-unknown-mem", cl::Hidden, cl::init(false),
  cl::desc("Disable wrapping mem<set, copy, move> of unknown size with HTM"));

/// Disable wrapping libc functions which are likely to cause HTM aborts with
/// HTM stop/start intrinsics.  Wrapping happens by default with HTM execution.
const static cl::opt<bool>
NoWrapLibc("htm-no-wrap-libc", cl::Hidden, cl::init(false),
  cl::desc("Disable wrapping libc functions with HTM stop/start"));

/// HTM memory read buffer size for tuning analysis when inserting additional
/// migration points.
const static cl::opt<unsigned>
HTMReadBufSizeArg("htm-buf-read", cl::Hidden, cl::init(32),
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

#define MILLION 1000000
#define CyclesBetweenMigPoints \
  ((unsigned long)MillionCyclesBetweenMigPoints * MILLION)
#define MEM_WEIGHT 40

STATISTIC(LoopsTransformed, "Number of loops transformed");
STATISTIC(NumIVsAdded, "Number of induction variables added");

namespace {

/// Get the integer size of a value, if statically known.
static int64_t getValueSize(const Value *V) {
  if(isa<ConstantInt>(V)) return cast<ConstantInt>(V)->getSExtValue();
  return -1;
}

/// Return a percentage of a value.
static inline size_t getValuePercent(size_t V, unsigned P) {
  assert(P <= 100 && "Invalid percentage");
  return floor(((double)V) * (((double)P) / 100.0));
}

/// Return the number of cache lines accessed for a given number of
/// (assumed contiguous) bytes.
static inline size_t getNumCacheLines(size_t Bytes, unsigned LineSize) {
  return ceil((double)Bytes / (double)LineSize);
}

/// Abstract weight metric.  Child classes implement for analyzing different
/// resource capacities, e.g., HTM buffer sizes.
class Weight {
protected:
  /// Number of times the weight was reset.
  size_t Resets;

  Weight() : Resets(0) {}
  Weight(const Weight &C) : Resets(C.Resets) {}

public:
  virtual ~Weight() {};
  virtual Weight *copy() const = 0;

  /// Expose types of child implementations.
  virtual bool isCycleWeight() const { return false; }
  virtual bool isHTMWeight() const { return false; }

  /// Analyze an instruction & update accounting.
  virtual void analyze(const Instruction *I, const DataLayout *DL) = 0;

  /// Return whether or not we should add a migration point.  This is tuned
  /// based on the resource capacity and percentage threshold options.
  virtual bool shouldAddMigPoint(unsigned percent) const {
    return !underPercentOfThreshold(percent);
  }

  /// Reset the weight.
  virtual void reset() { Resets++; }

  /// Update this weight with the max of this weight and another.
  virtual void max(const Weight *RHS) = 0;
  virtual void max(const std::unique_ptr<Weight> &RHS) { max(RHS.get()); }

  /// Multiply the weight by a factor, e.g., a number of loop iterations.
  virtual void multiply(size_t factor) = 0;

  /// Add another weight to this weight.
  virtual void add(const Weight *RHS) = 0;
  virtual void add(const std::unique_ptr<Weight> &RHS) { add(RHS.get()); }

  /// Number of times this weight "fits" into the resource capacity before we
  /// need to place a migration point.  This is used for calculating how many
  /// iterations of a loop can be executed between migration points.
  virtual size_t numIters(unsigned percent) const = 0;

  /// Return whether or not the weight is within some percent (0-100) of the
  /// resource capacity for a type of weight.
  virtual bool underPercentOfThreshold(unsigned percent) const = 0;

  /// Return a human-readable string describing weight information.
  virtual std::string toString() const = 0;
};

typedef std::unique_ptr<Weight> WeightPtr;

/// Weight metrics for HTM analysis, which basically depend on the number
/// of bytes loaded & stored.
class HTMWeight : public Weight {
private:
  // The number of bytes loaded & stored, respectively.
  size_t LoadBytes, StoreBytes;

  // Statistics about when the weight was reset (i.e., at HTM stop/starts).
  size_t ResetLoad, ResetStore;

public:
  HTMWeight(size_t LoadBytes = 0, size_t StoreBytes = 0)
    : LoadBytes(LoadBytes), StoreBytes(StoreBytes), ResetLoad(0),
      ResetStore(0) {}
  HTMWeight(const HTMWeight &C)
    : Weight(C), LoadBytes(C.LoadBytes), StoreBytes(C.StoreBytes),
      ResetLoad(C.ResetLoad), ResetStore(C.ResetStore) {}
  virtual Weight *copy() const { return new HTMWeight(*this); }

  virtual bool isHTMWeight() const { return true; }

  /// Analyze an instruction for memory operations.
  virtual void analyze(const Instruction *I, const DataLayout *DL) {
    Type *Ty;

    // TODO do extractelement, insertelement, shufflevector, extractvalue, or
    // insertvalue read/write memory?
    // TODO Need to handle the following instructions/instrinsics (also see
    // Instruction::mayLoad() / Instruction::mayStore()):
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

    case Instruction::AtomicCmpXchg: {
      const AtomicCmpXchgInst *Cmp = cast<AtomicCmpXchgInst>(I);
      Ty = Cmp->getPointerOperand()->getType()->getPointerElementType();
      LoadBytes += DL->getTypeStoreSize(Ty);
      StoreBytes += DL->getTypeStoreSize(Ty);
    }

    case Instruction::AtomicRMW: {
      const AtomicRMWInst *RMW = cast<AtomicRMWInst>(I);
      Ty = RMW->getPointerOperand()->getType()->getPointerElementType();
      LoadBytes += DL->getTypeStoreSize(Ty);
      StoreBytes += DL->getTypeStoreSize(Ty);
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

      // Size > 0: we know the size statically
      // Size < 0: we can't determine the size statically
      // Size == 0: some intrinsic we don't care about
      if(Size > 0) {
        if(Loads) LoadBytes += Size;
        if(Stores) StoreBytes += Size;
      }
      else if(Size < 0) {
        // Assume we're doing heavy reading & writing -- may need to revise if
        // transaction begin/ends are too expensive.
        if(Loads) LoadBytes += HTMReadBufSize;
        if(Stores) StoreBytes += HTMWriteBufSize;
      }

      break;
    }
    }
  }

  virtual void reset() {
    Weight::reset();
    ResetLoad += LoadBytes;
    ResetStore += StoreBytes;
    LoadBytes = StoreBytes = 0;
  }

  /// The max value for HTM weights is the max of the two weights' LoadBytes
  /// and StoreBytes (maintained separately).
  virtual void max(const Weight *RHS) {
    assert(RHS->isHTMWeight() && "Cannot mix weight types");
    const HTMWeight *W = (const HTMWeight *)RHS;
    if(W->LoadBytes > LoadBytes) LoadBytes = W->LoadBytes;
    if(W->StoreBytes > StoreBytes) StoreBytes = W->StoreBytes;
  }

  virtual void multiply(size_t factor) {
    LoadBytes *= factor;
    StoreBytes *= factor;
  }

  virtual void add(const Weight *RHS) {
    assert(RHS->isHTMWeight() && "Cannot mix weight types");
    const HTMWeight *W = (const HTMWeight *)RHS;
    LoadBytes += W->LoadBytes;
    StoreBytes += W->StoreBytes;
  }

  /// The number of times this weight's load & stores could be executed without
  /// overflowing the capacity threshold of the HTM buffers.
  virtual size_t numIters(unsigned percent) const {
    size_t NumLoadIters = UINT64_MAX, NumStoreIters = UINT64_MAX,
      FPHtmReadSize = getValuePercent(HTMReadBufSize, percent),
      FPHtmWriteSize = getValuePercent(HTMWriteBufSize, percent);

    if(!LoadBytes && !StoreBytes) return 1024; // Return a safe value
    else {
      if(LoadBytes) NumLoadIters = FPHtmReadSize / LoadBytes;
      if(StoreBytes) NumStoreIters = FPHtmWriteSize / StoreBytes;

      if(!NumLoadIters && !NumStoreIters) return 1;
      else return NumLoadIters < NumStoreIters ? NumLoadIters : NumStoreIters;
    }
  }

  virtual bool underPercentOfThreshold(unsigned percent) const {
    if(LoadBytes <= getValuePercent(HTMReadBufSize, percent) &&
       StoreBytes <= getValuePercent(HTMWriteBufSize, percent))
      return true;
    else return false;
  }

  virtual std::string toString() const {
    return std::to_string(LoadBytes) + " byte(s) loaded, " +
           std::to_string(StoreBytes) + " byte(s) stored";
  }
};

/// Weight metric for temporally-spaced migration points.
class CycleWeight : public Weight {
private:
  // An estimate of the number of cycles since the last migration point.
  size_t Cycles;

  // Statistics about when the weight was reset (i.e., at migration points).
  size_t ResetCycles;

public:
  CycleWeight(size_t Cycles = 0) : Cycles(Cycles), ResetCycles(0) {}
  CycleWeight(const CycleWeight &C)
    : Weight(C), Cycles(C.Cycles), ResetCycles(C.ResetCycles) {}
  virtual CycleWeight *copy() const { return new CycleWeight(*this); }

  virtual bool isCycleWeight() const { return true; }

  virtual void analyze(const Instruction *I, const DataLayout *DL) {
    Type *Ty;

    // Cycles are estimated using Agner Fog's instruction latency guide at
    // http://www.agner.org/optimize/instruction_tables.pdf for "Broadwell".
    switch(I->getOpcode()) {
    default: break;
    // Terminator instructions
    // TODO Ret, Invoke, Resume
    case Instruction::Br: Cycles += 2; break;
    case Instruction::Switch: Cycles += 2; break;
    case Instruction::IndirectBr: Cycles += 2; break;

    // Binary instructions
    case Instruction::Add: Cycles++; break;
    case Instruction::FAdd: Cycles += 3; break;
    case Instruction::Sub: Cycles++; break;
    case Instruction::FSub: Cycles += 3; break;
    case Instruction::Mul: Cycles += 2; break;
    case Instruction::FMul: Cycles += 3; break;
    case Instruction::UDiv: Cycles += 73; break;
    case Instruction::SDiv: Cycles += 81; break;
    case Instruction::FDiv: Cycles += 14; break;
    case Instruction::URem: Cycles += 73; break;
    case Instruction::SRem: Cycles += 81; break;
    case Instruction::FRem: Cycles += 14; break;

    // Logical operators
    case Instruction::Shl: Cycles += 2; break;
    case Instruction::LShr: Cycles += 2; break;
    case Instruction::AShr: Cycles += 2; break;
    case Instruction::And: Cycles += 1; break;
    case Instruction::Or: Cycles += 1; break;
    case Instruction::Xor: Cycles += 1; break;

    // Memory instructions
    case Instruction::Load: {
      const LoadInst *LI = cast<LoadInst>(I);
      Ty = LI->getPointerOperand()->getType()->getPointerElementType();
      Cycles += getNumCacheLines(DL->getTypeStoreSize(Ty), 64) * MEM_WEIGHT;
      break;
    }
    case Instruction::Store: {
      const StoreInst *SI = cast<StoreInst>(I);
      Ty = SI->getValueOperand()->getType();
      Cycles += getNumCacheLines(DL->getTypeStoreSize(Ty), 64) * MEM_WEIGHT;
      break;
    }
    case Instruction::GetElementPtr: Cycles++; break;
    case Instruction::Fence: Cycles += 33; break;
    case Instruction::AtomicCmpXchg: Cycles += 21; break;
    case Instruction::AtomicRMW: Cycles += 21; break;

    // Cast instructions
    case Instruction::Trunc: Cycles++; break;
    case Instruction::ZExt: Cycles++; break;
    case Instruction::SExt: Cycles++; break;
    case Instruction::FPToUI: Cycles += 4; break;
    case Instruction::FPToSI: Cycles += 4; break;
    case Instruction::UIToFP: Cycles += 5; break;
    case Instruction::SIToFP: Cycles += 5; break;
    case Instruction::FPTrunc: Cycles += 4; break;
    case Instruction::FPExt: Cycles += 2; break;

    // Other instructions
    // TODO VAArg, ExtractElement, InsertElement, ShuffleVector, ExtractValue,
    // InsertValue, LandingPad
    case Instruction::ICmp: Cycles++; break;
    case Instruction::FCmp: Cycles += 3; break;
    case Instruction::Call: {
      const IntrinsicInst *II = dyn_cast<IntrinsicInst>(I);
      int64_t Size = 0;

      if(!II) Cycles += 3;
      else {
        switch(II->getIntrinsicID()) {
        default: break;
        case Intrinsic::memcpy:
        case Intrinsic::memmove:
        case Intrinsic::memset:
          // Arguments: i8* dest, i8* src, i<x> len, i32 align, i1 isvolatile
          Size = getValueSize(II->getArgOperand(2));
          break;
        }

        if(Size > 0) Cycles += getNumCacheLines(Size, 64) * MEM_WEIGHT;
      }
      break;
    }
    case Instruction::Select: Cycles += 3; break;
    }
  }

  virtual void reset() {
    Weight::reset();
    ResetCycles += Cycles;
    Cycles = 0;
  }

  virtual void max(const Weight *RHS) {
    assert(RHS->isCycleWeight() && "Cannot mix weight types");
    const CycleWeight *W = (const CycleWeight *)RHS;
    if(W->Cycles > Cycles) Cycles = W->Cycles;
  }

  virtual void multiply(size_t factor) { Cycles *= factor; }
  virtual void add(const Weight *RHS) {
    assert(RHS->isCycleWeight() && "Cannot mix weight types");
    const CycleWeight *W = (const CycleWeight *)RHS;
    Cycles += W->Cycles;
  }

  virtual size_t numIters(unsigned percent) const {
    if(!Cycles) return 1048576; // Return a safe value
    else {
      size_t FPCycleCap = getValuePercent(CyclesBetweenMigPoints, percent);
      size_t Iters = FPCycleCap / Cycles;
      return Iters ? Iters : 1;
    }
  }

  virtual bool underPercentOfThreshold(unsigned percent) const {
    if(Cycles <= getValuePercent(CyclesBetweenMigPoints, percent)) return true;
    else return false;
  }

  virtual std::string toString() const {
    return std::to_string(Cycles) + " cycles";
  }
};

/// Get a weight object with zero-initialized weight based on the type of
/// analysis being used to instrument the application.
///
/// Note: returns a dynamically allocated object to be managed by the caller
static Weight *getZeroWeight() {
  if(HTMExec) return new HTMWeight();
  else return new CycleWeight();
}

/// SelectMigrationPoints - select locations at which to insert migration
/// points into functions.
class SelectMigrationPoints : public FunctionPass
{
public:
  static char ID;

  SelectMigrationPoints() : FunctionPass(ID) {
    initializeSelectMigrationPointsPass(*PassRegistry::getPassRegistry());
  }
  ~SelectMigrationPoints() {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<EnumerateLoopPaths>();
    AU.addRequired<ScalarEvolution>();
  }

  virtual const char *getPassName() const
  { return "Select migration point locations"; }

  static inline unsigned splitFuncValPair(const std::string &Pair,
                                          std::string &Func) {
    unsigned Val;
    size_t Comma = Pair.rfind(',');
    Func = Pair.substr(0, Comma);
    Val = stoul(Pair.substr(Comma + 1));
    assert(Val <= 100 && "Invalid percentage");
    return Val;
  }

  /// Parse per-function threshold values from the command line.
  void parsePerFuncThresholds() {
    unsigned Val;
    std::string Name;

    FuncCapList.clear();
    FuncStartList.clear();
    FuncRetList.clear();
    NoInstFuncs.clear();

    for(auto Pair : FuncCapThreshold) {
      Val = splitFuncValPair(Pair, Name);
      FuncCapList[Name] = Val;
    }
    for(auto Pair : FuncStartThreshold) {
      Val = splitFuncValPair(Pair, Name);
      FuncStartList[Name] = Val;
    }
    for(auto Pair : FuncRetThreshold) {
      Val = splitFuncValPair(Pair, Name);
      FuncRetList[Name] = Val;
    }
    for(auto Func : FuncNoInst) NoInstFuncs.insert(Func);
  }

  virtual bool doInitialization(Module &M) {
    DL = &M.getDataLayout();
    addPopcornFnAttributes(M);
    if(MoreMigPoints) parsePerFuncThresholds();
    if(HTMExec) Popcorn::setInstrumentationType(M, Popcorn::HTM);
    else Popcorn::setInstrumentationType(M, Popcorn::Cycles);
    return false;
  }

  /// Select where to insert migration points into functions.
  virtual bool runOnFunction(Function &F)
  {
    DEBUG(dbgs() << "\n********** SELECT MIGRATION POINTS **********\n"
                 << "********** Function: " << F.getName() << "\n\n");

    if(F.hasFnAttribute("popcorn-noinstr") ||
       NoInstFuncs.find(F.getName()) != NoInstFuncs.end()) return false;

    initializeAnalysis(F);

    // Some operations (e.g., big memory copies, I/O) will cause aborts.
    // Instrument these operations to stop & resume transactions afterwards.
    if(HTMExec) {
      bool AddedMigPoint = wrapWithHTM(F, isBigMemoryOp,
        "memory operations that overflow HTM buffers");
      if(!NoWrapLibc)
        AddedMigPoint |= wrapWithHTM(F, isLibcIO, "I/O functions");
      if(AddedMigPoint) LP->runOnFunction(F);
    }

    if(MoreMigPoints && !LP->analysisFailed()) {
      StringRef FuncName = F.getName();
      StringMap<unsigned>::const_iterator It;
      if((It = FuncCapList.find(FuncName)) != FuncCapList.end())
        CurCapThresh = It->second;
      else CurCapThresh = CapacityThreshold;
      if((It = FuncStartList.find(FuncName)) != FuncStartList.end())
        CurStartThresh = It->second;
      else CurStartThresh = StartThreshold;
      if((It = FuncRetList.find(FuncName)) != FuncRetList.end())
        CurRetThresh = It->second;
      else CurRetThresh = RetThreshold;

      DEBUG(
        dbgs() << "\n-> Analyzing function body to add migration points <-\n"
               << "\nCapacity threshold: " << std::to_string(CurCapThresh)
               << "\nStart threshold: " << std::to_string(CurStartThresh)
               << "\nReturn threshold: " << std::to_string(CurRetThresh)
               << "\nMaximum iterations/migration point: "
               << std::to_string(MaxItersPerMigPoint);

        if(HTMExec)
          dbgs() << "\nAnalyzing for HTM Instrumentation"
                 << "\n  HTM read buffer size: "
                 << std::to_string(HTMReadBufSizeArg) << "kb"
                 << "\n  HTM write buffer size: "
                 << std::to_string(HTMWriteBufSizeArg) << "kb\n";
        else
          dbgs() << "\nAnalyzing for migration call out instrumentation"
                 << "\n  Target millions of cycles between migration points: "
                 << std::to_string(MillionCyclesBetweenMigPoints) << "\n";
      );

      // We by default mark the function start as a migration point, but if we
      // don't add any instrumentation & the function's exit weights are
      // sufficiently small avoid instrumentation altogether.
      bool MarkStart = false;
      if(!analyzeFunctionBody(F)) {
        for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++)
          if(isa<ReturnInst>(BB->getTerminator()) &&
             !BBWeights[BB].BlockWeight->underPercentOfThreshold(CurStartThresh))
            MarkStart = true;
      }
      else MarkStart = true;

      if(MarkStart) {
        DEBUG(dbgs() << "-> Marking function entry as a migration point <-\n");
        markAsMigPoint(F.getEntryBlock().getFirstInsertionPt(), true, true);
      }
      else { DEBUG(dbgs() << "-> Eliding instrumenting function entry <-\n"); }
    }
    else {
      if(MoreMigPoints) {
        std::string Msg = "too many paths to instrument function with more "
                          "migration points -- falling back to instrumenting "
                          "function entry/exit";
        DiagnosticInfoOptimizationFailure DI(F, nullptr, Msg);
        F.getContext().diagnose(DI);
      }

      DEBUG(dbgs() << "-> Marking function entry as a migration point <-\n");
      markAsMigPoint(F.getEntryBlock().getFirstInsertionPt(), true, true);

      // Instrument function exit point(s)
      DEBUG(dbgs() << "-> Marking function exit(s) as a migration point <-\n");
      for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++)
        if(isa<ReturnInst>(BB->getTerminator()))
          markAsMigPoint(BB->getTerminator(), true, true);
    }

    // Finally, apply transformations to loops headers according to analysis.
    transformLoopHeaders(F);

    return true;
  }

  /// Reset all analysis.
  void initializeAnalysis(const Function &F) {
    SE = &getAnalysis<ScalarEvolution>();
    LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    LP = &getAnalysis<EnumerateLoopPaths>();
    BBWeights.clear();
    LoopWeights.clear();
    TransformLoops.clear();
    MigPointInsts.clear();
    HTMBeginInsts.clear();
    HTMEndInsts.clear();
  }

private:
  //===--------------------------------------------------------------------===//
  // Types & fields
  //===--------------------------------------------------------------------===//

  /// Configuration for the function currently being analyzed.
  unsigned CurCapThresh;
  unsigned CurStartThresh;
  unsigned CurRetThresh;

  /// The current architecture - used to access architecture-specific HTM calls
  const DataLayout *DL;

  /// Parsed per-function thresholds.
  StringMap<unsigned> FuncCapList;
  StringMap<unsigned> FuncStartList;
  StringMap<unsigned> FuncRetList;
  StringSet<> NoInstFuncs;

  /// Analyses on which we depend
  ScalarEvolution *SE;
  LoopInfo *LI;
  EnumerateLoopPaths *LP;

  /// libc functions which are likely to cause an HTM abort through a syscall
  const static StringSet<> LibcIO;

  /// Weight information for basic blocks.
  class BasicBlockWeightInfo {
  public:
    /// Weight of the basic block at the end of its execution.  If the block has
    /// a migration point, the weight *only* captures the instructions following
    /// the migration point (migration points "reset" the weight).
    WeightPtr BlockWeight;

    BasicBlockWeightInfo() : BlockWeight(nullptr) {}
    BasicBlockWeightInfo(const Weight *BlockWeight)
      : BlockWeight(BlockWeight->copy()) {}
    BasicBlockWeightInfo(const WeightPtr &BlockWeight)
      : BlockWeight(BlockWeight->copy()) {}

    std::string toString() const {
      if(BlockWeight) return BlockWeight->toString();
      else return "<uninitialized basic block weight info>";
    }
  };

  /// Weight information for loops.  Maintains weights at loop exit points as
  /// well as path-specific weight information for the loop & exit blocks.
  class LoopWeightInfo {
  private:
    /// The weight of the loop upon entry.  Zero in the default case, but may
    /// be set if analysis elides instrumentation in and around the loop.
    WeightPtr EntryWeight;

    /// The maximum weight when exiting the loop at each of its exit blocks.
    /// Automatically recalculated when any of its ingredients are changed.
    DenseMap<const BasicBlock *, WeightPtr> ExitWeights;

    /// Whether the loop has either of the two types of paths, and if so the
    /// maximum weight of each type.  Note that the spanning path weight is
    /// *not* scaled by the number of iterations, ItersPerMigPoint.
    bool LoopHasSpanningPath, LoopHasEqPointPath;
    WeightPtr LoopSpanningPathWeight, LoopEqPointPathWeight;

    /// Number of iterations between migration points if the loop has one or
    /// more spanning paths, or zero otherwise.
    size_t ItersPerMigPoint;

    /// Whether there are either of the two types of paths through each exit
    /// block, and if so the maximum weight of each type.
    DenseMap<const BasicBlock *, bool> ExitHasSpanningPath, ExitHasEqPointPath;
    DenseMap<const BasicBlock *, WeightPtr> ExitSpanningPathWeights,
                                            ExitEqPointPathWeights;

    /// Calculate the exit block's maximum weight, which is the max of both the
    /// spanning path exit weight and equivalence point path exit weight.
    void computeExitWeight(const BasicBlock *BB) {
      // Note: these operations are in a specific order -- change with care!

      // Calculate the loop weight up until the current iteration
      WeightPtr BBWeight(getZeroWeight());
      if(LoopHasSpanningPath) BBWeight->max(getLoopSpanningPathWeight(true));
      if(LoopHasEqPointPath) BBWeight->max(LoopEqPointPathWeight);

      // Calculate the maximum possible value of the current iteration:
      //   - Spanning path: loop weight + current path weight
      //   - Equivalence point path: current weight path
      if(ExitHasSpanningPath[BB]) BBWeight->add(ExitSpanningPathWeights[BB]);
      if(ExitHasEqPointPath[BB]) BBWeight->max(ExitEqPointPathWeights[BB]);

      ExitWeights[BB] = std::move(BBWeight);
    }

    void computeAllExitWeights() {
      for(auto I = ExitWeights.begin(), E = ExitWeights.end(); I != E; I++)
        computeExitWeight(I->first);
    }

  public:
    LoopWeightInfo() = delete;
    LoopWeightInfo(const Loop *L)
      : EntryWeight(getZeroWeight()), LoopHasSpanningPath(false),
        LoopHasEqPointPath(false), ItersPerMigPoint(0) {
      SmallVector<BasicBlock *, 4> ExitBlocks;
      L->getExitingBlocks(ExitBlocks);
      for(auto Block : ExitBlocks) {
        ExitHasSpanningPath[Block] = false;
        ExitHasEqPointPath[Block] = false;
      }
    }

    /// Set the weight upon entering the loop & recompute all exit weights.
    void setEntryWeight(const WeightPtr &W) {
      EntryWeight.reset(W->copy());
      computeAllExitWeights();
    }

    /// Get the number of iterations between migration points, or zero if there
    /// are no spanning paths through the loop.
    size_t getItersPerMigPoint() const {
      return ItersPerMigPoint;
    }

    /// Get the loop's spanning path weight, scaled based on the number of
    /// iterations.  Also includes loop entry weight if requested.
    WeightPtr getLoopSpanningPathWeight(bool AddEntry) const {
      assert(LoopHasSpanningPath && "No spanning path weight for loop");
      WeightPtr Ret(LoopSpanningPathWeight->copy());
      Ret->multiply(ItersPerMigPoint - 1);
      if(AddEntry) Ret->add(EntryWeight);
      return Ret;
    }

    /// Set the loop's spanning path weight & recompute all exit weights.
    ///  - W: the maximum weight of a single spanning path iteration
    ///  - I: the number of iterations per migration point
    void setLoopSpanningPathWeight(const WeightPtr &W, size_t I) {
      LoopHasSpanningPath = true;
      LoopSpanningPathWeight.reset(W->copy());
      ItersPerMigPoint = I;
      computeAllExitWeights();
    }

    /// Get the loop's equivalence point path weight.
    WeightPtr getLoopEqPointPathWeight() const
    {
      assert(LoopHasEqPointPath && "No equivalence point path weight for loop");
      return WeightPtr(LoopEqPointPathWeight->copy());
    }

    /// Set the loop's equivalence point path weight & recompute all exit
    /// weights.
    void setLoopEqPointPathWeight(const WeightPtr &W) {
      LoopHasEqPointPath = true;
      LoopEqPointPathWeight.reset(W->copy());
      computeAllExitWeights();
    }

    /// Get an exit block's spanning path weight.  This is the raw weight for
    /// a single iteration of paths through this exiting block, it does *not*
    /// incorporate loop weights.
    WeightPtr getExitSpanningPathWeight(const BasicBlock *BB) const
    {
      assert(ExitHasSpanningPath.find(BB)->second &&
             "No spanning path weight for exit block");
      return WeightPtr(ExitSpanningPathWeights.find(BB)->second->copy());
    }

    /// Set the exit block's spanning path weight & recompute the exit block's
    /// overall maximum weight.
    void setExitSpanningPathWeight(const BasicBlock *BB, const WeightPtr &W)
    {
      ExitHasSpanningPath[BB] = true;
      ExitSpanningPathWeights[BB].reset(W->copy());
      computeExitWeight(BB);
    }

    /// Get an exit block's equivalence point path weight.  This is the raw
    /// weight for a single iteration of paths through this exiting block, it
    /// does *not* incorporate loop weights.
    WeightPtr getExitEqPointPathWeight(const BasicBlock *BB) const
    {
      assert(ExitHasEqPointPath.find(BB)->second &&
             "No equivalence point path weight for exit block");
      return WeightPtr(ExitEqPointPathWeights.find(BB)->second->copy());
    }

    /// Set the equivalence point path exit block weight & recompute the exit
    /// block's overall maximum weight.
    void setExitEqPointPathWeight(const BasicBlock *BB, const WeightPtr &W)
    {
      ExitHasEqPointPath[BB] = true;
      ExitEqPointPathWeights[BB].reset(W->copy());
      computeExitWeight(BB);
    }

    /// Return whether the loop/exit block has spanning and equivalence point
    /// paths through it.
    bool loopHasSpanningPath() const { return LoopHasSpanningPath; }
    bool loopHasEqPointPath() const { return LoopHasEqPointPath; }
    bool exitHasSpanningPath(const BasicBlock *BB) const
    { return ExitHasSpanningPath.find(BB)->second; }
    bool exitHasEqPointPath(const BasicBlock *BB) const
    { return ExitHasEqPointPath.find(BB)->second; }

    /// Return the weight of a given exiting basic block.
    const WeightPtr &getExitWeight(const BasicBlock *BB) const {
      assert(ExitWeights.count(BB) && "Invalid exit basic block");
      return ExitWeights.find(BB)->second;
    }

    const WeightPtr &operator[](const BasicBlock *BB) const
    { return getExitWeight(BB); }

    std::string toString() const {
      if(!ExitWeights.size()) return "<uninitialized loop weight info>";
      else {
        std::string buf = "Exit block weights:\n";
        for(auto It = ExitWeights.begin(), E = ExitWeights.end();
            It != E; ++It) {
          buf += "    ";
          if(It->first->hasName()) {
            buf += It->first->getName();
            buf += ": ";
          }
          buf += It->second->toString() + "\n";
        }
        return buf;
      }
    }
  };

  /// Weight information gathered by analyses for basic blocks & loops
  typedef std::map<const BasicBlock *, BasicBlockWeightInfo> BlockWeightMap;
  typedef std::map<const Loop *, LoopWeightInfo> LoopWeightMap;
  BlockWeightMap BBWeights;
  LoopWeightMap LoopWeights;

  /// Code locations marked for instrumentation.
  SmallPtrSet<Loop *, 16> TransformLoops;
  SmallPtrSet<Instruction *, 32> MigPointInsts;
  SmallPtrSet<Instruction *, 32> HTMBeginInsts;
  SmallPtrSet<Instruction *, 32> HTMEndInsts;

  //===--------------------------------------------------------------------===//
  // Analysis implementation
  //===--------------------------------------------------------------------===//

  /// Add Popcorn-related function attributes where appropriate.
  void addPopcornFnAttributes(Module &M) const {
    auto GlobalAnnos = M.getNamedGlobal("llvm.global.annotations");
    if(GlobalAnnos) {
      auto a = cast<ConstantArray>(GlobalAnnos->getOperand(0));
      for(unsigned int i = 0; i < a->getNumOperands(); i++) {
        auto e = cast<ConstantStruct>(a->getOperand(i));
        if(auto fn = dyn_cast<Function>(e->getOperand(0)->getOperand(0))) {
          auto Anno = cast<ConstantDataArray>(
                        cast<GlobalVariable>(
                          e->getOperand(1)->getOperand(0)
                        )->getOperand(0)
                      )->getAsCString();
          fn->addFnAttr(Anno);
        }
      }
    }
  }

  /// Return whether the instruction requires HTM begin instrumentation.
  bool shouldAddHTMBegin(Instruction *I) const {
    if(Popcorn::isHTMBeginPoint(I)) return true;
    else return HTMBeginInsts.count(I);
  }

  /// Return whether the instruction requires HTM end instrumentation.
  bool shouldAddHTMEnd(Instruction *I) const {
    if(Popcorn::isHTMEndPoint(I)) return true;
    else return HTMEndInsts.count(I);
  }

  /// Return whether the instruction is a migration point.  We assume that all
  /// called functions have migration points internally.
  bool isMigrationPoint(Instruction *I) const {
    if(Popcorn::isEquivalencePoint(I)) return true;
    else return MigPointInsts.count(I);
  }

  /// Return whether the instruction is marked for any instrumentation.
  bool isMarkedForInstrumentation(Instruction *I) const {
    return isMigrationPoint(I) || shouldAddHTMBegin(I) || shouldAddHTMEnd(I);
  }

  /// Mark an instruction to be instrumented with an HTM begin, directly before
  /// the instruction
  bool markAsHTMBegin(Instruction *I) {
    if(!HTMExec) return false;
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as HTM begin\n");
    HTMBeginInsts.insert(I);
    Popcorn::addHTMBeginMetadata(I);
    return true;
  }

  /// Mark an instruction to be instrumented with an HTM end, directly before
  /// the instruction
  bool markAsHTMEnd(Instruction *I) {
    if(!HTMExec) return false;
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as HTM end\n");
    HTMEndInsts.insert(I);
    Popcorn::addHTMEndMetadata(I);
    return true;
  }

  /// Mark an instruction to be instrumented with a migration point, directly
  /// before the instruction.  Optionally mark instruction as needing HTM
  /// start/stop intrinsics.
  bool markAsMigPoint(Instruction *I, bool AddHTMBegin, bool AddHTMEnd) {
    // Don't clobber any existing instrumentation
    if(isMarkedForInstrumentation(I)) return false;
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as a migration point\n");
    MigPointInsts.insert(I);
    Popcorn::addEquivalencePointMetadata(I);
    if(AddHTMBegin) markAsHTMBegin(I);
    if(AddHTMEnd) markAsHTMEnd(I);
    return true;
  }

  /// Instruction matching function type.
  typedef bool (*InstMatch)(const Instruction *, unsigned Thresh);

  /// Return whether the instruction is a memory operation that will overflow
  /// HTM buffers.
  static bool isBigMemoryOp(const Instruction *I, unsigned Thresh) {
    if(!I || !isa<IntrinsicInst>(I)) return false;
    const IntrinsicInst *II = cast<IntrinsicInst>(I);
    int64_t Size = 0;
    switch(II->getIntrinsicID()) {
    default: return false;
    case Intrinsic::memcpy: case Intrinsic::memmove: case Intrinsic::memset:
      // Arguments: i8* dest, i8* src, i<x> len, i32 align, i1 isvolatile
      Size = getValueSize(II->getArgOperand(2));
      break;
    }

    if(Size >= 0) { // We know the size
      size_t USize = (size_t)Size;
      return USize >= getValuePercent(HTMReadBufSize, Thresh) ||
             USize >= getValuePercent(HTMWriteBufSize, Thresh);
    }
    else return !NoWrapUnknownMem;
  }

  /// Return whether the instruction is a libc I/O call.
  static bool isLibcIO(const Instruction *I, unsigned Thresh) {
    if(!I || !Popcorn::isCallSite(I)) return false;
    const ImmutableCallSite CS(I);
    const Function *CalledFunc = CS.getCalledFunction();
    if(CalledFunc && CalledFunc->hasName())
      return LibcIO.find(CalledFunc->getName()) != LibcIO.end();
    return false;
  }

  /// Search for & wrap operations that match a certain criteria.
  bool wrapWithHTM(Function &F, InstMatch Matcher, const char *Desc) {
    bool AddedMigPoint = false;

    DEBUG(dbgs() << "\n-> Wrapping " << Desc << " with HTM stop/start <-\n");
    for(Function::iterator BB = F.begin(), BE = F.end(); BB != BE; BB++) {
      if(LI->getLoopFor(BB)) continue; // Don't do this in loops!
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++) {
        if(Matcher(I, CurCapThresh)) {
          markAsHTMEnd(I);

          // Search subsequent instructions for other libc calls to prevent
          // pathological transaction stop/starts.
          const static size_t searchSpan = 10;
          BasicBlock::iterator NextI(I->getNextNode());
          for(size_t rem = searchSpan; rem > 0 && NextI != E; rem--, NextI++) {
            if(Matcher(NextI, CurCapThresh)) {
              DEBUG(dbgs() << "  - Found another match:"; NextI->dump());
              I = NextI;
              rem = searchSpan;
            }
          }

          // TODO analyze successor blocks as well

          AddedMigPoint |= markAsMigPoint(I->getNextNode(), true, false);
        }
      }
    }

    return AddedMigPoint;
  }

  /// Get the starting weight for a basic block based on the max weights of its
  /// predecessors.
  ///
  /// Note: returns a dynamically allocated object to be managed by the caller
  Weight *getInitialWeight(const BasicBlock *BB) const {
    Weight *PredWeight = getZeroWeight();
    const Loop *L = LI->getLoopFor(BB);
    bool BBIsHeader = L && (BB == L->getHeader());
    unsigned LDepth = L ? L->getLoopDepth() : 0;

    for(auto Pred : predecessors(BB)) {
      const Loop *PredLoop = LI->getLoopFor(Pred);

      // We *only* gather header initial weights when analyzing whether to
      // instrument loop entry, which doesn't depend on latches.
      if(BBIsHeader && PredLoop == L) continue;

      // Determine if the predecessor is an exit block from another loop:
      //
      //   1. The predecessor is in a loop
      //   2. The predecessor's loop is not BB's loop
      //   3. The nesting depth of the predecessor's loop is >= BB's loop*
      //
      // If it's an exit block, use the loop weight info to get the exit
      // weight.  Otherwise, use the basic block weight info.
      //
      // *Note: if the predecessor's nesting depth is < BB's, then BB is in a
      // child loop inside the predecessor's loop, and the predecessor is NOT a
      // loop exiting block.
      if(PredLoop && PredLoop != L && PredLoop->getLoopDepth() >= LDepth) {
        assert(LoopWeights.count(PredLoop) &&
               "Invalid reverse post-order traversal");
        PredWeight->max(LoopWeights.at(PredLoop)[Pred]);
      }
      else {
        assert(BBWeights.count(Pred) && "Invalid reverse post-order traversal");
        PredWeight->max(BBWeights.at(Pred).BlockWeight);
      }
    }

    return PredWeight;
  }

  /// Analyze a single basic block with an initial starting weight and update
  /// it with the block's ending weight.  Return whether or not a migration
  /// point was added.
  bool traverseBlock(BasicBlock *BB, Weight *CurWeight) {
    bool AddedMigPoint = false;

    DEBUG(
      dbgs() << "      Analyzing basic block";
      if(BB->hasName()) dbgs() << " '" << BB->getName() << "'";
      dbgs() << "\n";
    );

    // TODO this doesn't respect spans identified by wrapWithHTM()!

    for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++) {
      if(isa<PHINode>(I)) continue;

      // Check if there is or there should be a migration point before the
      // instruction, and if so, reset the weight.  Note: markAsMigPoint()
      // internally avoids tampering with existing instrumentation.
      if(isMigrationPoint(I)) CurWeight->reset();
      else if(CurWeight->shouldAddMigPoint(CurCapThresh)) {
        AddedMigPoint |= markAsMigPoint(I, true, true);
        CurWeight->reset();
      }

      CurWeight->analyze(I, DL);
    }

    DEBUG(dbgs() << "       - Weight: " << CurWeight->toString() << "\n");

    return AddedMigPoint;
  }

  bool traverseBlock(BasicBlock *BB, WeightPtr &Initial)
  { return traverseBlock(BB, Initial.get()); }

  /// Mark loop predecessors, i.e., all branches into the loop header, as
  /// migration points.  Return whether or not a migration point was added.
  bool markLoopPredecessors(const Loop *L) {
    bool AddedMigPoint = false;
    BasicBlock *Header = L->getHeader();
    for(auto Pred : predecessors(Header)) {
      // Weed out latches
      if(!L->contains(Pred)) {
        // Avoid adding migration points in bodies of predecessor loops when
        // exiting from one loop directly into the header of another, e.g.,
        //
        //   for.body:  ;Body of first loop
        //     ...
        //     br i1 %cmp, for.body, for.body.2
        //
        //   for.body.2: ;Body of second, completely independent loop
        //     ...
        const Loop *PredL = LI->getLoopFor(Pred);
        if(PredL == nullptr || (PredL->getLoopDepth() < L->getLoopDepth()))
          AddedMigPoint |= markAsMigPoint(Pred->getTerminator(), true, true);
      }
    }
    return AddedMigPoint;
  }

  /// Analyze & mark loop entry with migration points.  Avoid instrumenting if
  /// we can execute the entire loop & any entry code without overflowing our
  /// resource capacity.
  bool traverseLoopEntry(Loop *L) {
    // We don't need to instrument around the loop if we're instrumenting the
    // header, as we'll hit a migration point at the beginning of the loop.
    if(TransformLoops.count(L)) return false;

    assert(LoopWeights.count(L) && "Invalid reverse post-order traversal");
    LoopWeightInfo &LWI = LoopWeights.at(L);

    // If the loop only has equivalence point paths, assume that we'll hit an
    // equivalence point before we abort -- may need to revise if there are too
    // many capacity aborts.
    if(!LWI.loopHasSpanningPath()) {
      DEBUG(dbgs() << "       - Loop only has equivalence point paths, "
                      "can elide instrumenting loop entry points\n");
      return false;
    }

    // TODO what if it's an irreducible loop, i.e., > 1 header?
    BasicBlock *Header = L->getHeader();
    WeightPtr HeaderWeight(getInitialWeight(Header));

    DEBUG(dbgs() << "       + Analyzing loop entry points to "
                 << Header->getName() << ", header weight: "
                 << HeaderWeight->toString() << "\n");

    // See if any of the exit spanning path weights are too heavy to include
    // the entry point weight (entry point weights don't affect equivalence
    // point paths).
    bool InstrumentLoopEntry = false;
    SmallVector<BasicBlock *, 4> ExitBlocks;
    L->getExitingBlocks(ExitBlocks);
    for(auto Exit : ExitBlocks) {
      if(LWI.exitHasSpanningPath(Exit)) {
        WeightPtr SpExitWeight(LWI.getLoopSpanningPathWeight(false));
        SpExitWeight->add(LWI.getExitSpanningPathWeight(Exit));
        SpExitWeight->add(HeaderWeight);
        if(SpExitWeight->shouldAddMigPoint(CurCapThresh))
          InstrumentLoopEntry = true;
      }
    }

    if(InstrumentLoopEntry) {
      DEBUG(dbgs() << "       - One or more spanning path(s) were too heavy, "
                      "instrumenting loop entry points\n");
      return markLoopPredecessors(L);
    }
    else {
      DEBUG(dbgs() << "       + Can elide instrumenting loop entry points\n");
      LWI.setEntryWeight(HeaderWeight);
      return false;
    }
  }

  /// Traverse a loop and instrument with migration points on paths that are
  /// too "heavy".  Return whether or not a migration point was added.
  bool traverseLoop(Loop *L) {
    bool AddedMigPoint = false;
    LoopBlocksDFS DFS(L); DFS.perform(LI);
    LoopBlocksDFS::RPOIterator Block = DFS.beginRPO(), E = DFS.endRPO();
    SmallPtrSet<const Loop *, 4> MarkedLoops;
    Loop *BlockLoop;

    assert(Block != E && "Loop with no basic blocks");

    DEBUG(
      dbgs() << "  + Analyzing "; L->dump();
      dbgs() << "    - At "; L->getStartLoc().dump();
    );

    // TODO what if it's an irreducible loop, i.e., > 1 header?
    BasicBlock *CurBB = *Block;
    WeightPtr HdrWeight(getZeroWeight());
    AddedMigPoint |= traverseBlock(CurBB, HdrWeight);
    BBWeights[CurBB] = std::move(HdrWeight);

    for(++Block; Block != E; ++Block) {
      CurBB = *Block;
      BlockLoop = LI->getLoopFor(CurBB);
      if(BlockLoop == L) { // Block is in same loop & nesting depth
        WeightPtr PredWeight(getInitialWeight(CurBB));
        AddedMigPoint |= traverseBlock(CurBB, PredWeight);
        BBWeights[CurBB] = std::move(PredWeight);
      }
      else if(!MarkedLoops.count(BlockLoop)) {
        // Block is in a sub-loop, analyze & mark sub-loop's entry.  Only
        // analyze direct sub-loops, as deeper-nested (2+) loops will have
        // already been analyzed by their parents.
        if(BlockLoop->getLoopDepth() - L->getLoopDepth() == 1)
          AddedMigPoint |= traverseLoopEntry(BlockLoop);
        MarkedLoops.insert(BlockLoop);
      }
    }

    DEBUG(dbgs() << "    Finished analyzing loop\n");

    return AddedMigPoint;
  }

  /// Analyze a path in a loop up until a particular end instruction and return
  /// its weight.  Doesn't do any marking.
  ///
  /// Note: returns a dynamically allocated object to be managed by the caller
  Weight *traversePathInternal(const LoopPath *LP,
                               const Instruction *PathEnd,
                               bool &ActuallyEqPoint) const {
    assert(LP->cbegin() != LP->cend() && "Trivial loop path, no blocks");
    assert(LP->contains(PathEnd->getParent()) && "Invalid end instruction");
    ActuallyEqPoint = false;

    Loop *SubLoop;
    Weight *PathWeight = getZeroWeight();
    SetVector<PathNode>::const_iterator Node = LP->cbegin(),
                                        EndNode = LP->cend();
    const BasicBlock *NodeBlock = Node->getBlock(),
                     *EndBlock = PathEnd->getParent();
    BasicBlock::const_iterator Inst, EndInst, PathEndInst(PathEnd);

    if(Node->isSubLoopExit()) {
      // Since the sub-loop exit block is the start of the path, it's by
      // definition exiting from an equivalence point path.
      SubLoop = LI->getLoopFor(NodeBlock);
      assert(LoopWeights.count(SubLoop) && "Invalid traversal");
      const LoopWeightInfo &LWI = LoopWeights.at(SubLoop);
      PathWeight->add(LWI.getExitEqPointPathWeight(NodeBlock));
    }
    else {
      for(Inst = LP->startInst(), EndInst = NodeBlock->end();
          Inst != EndInst && Inst != PathEndInst; Inst++)
        PathWeight->analyze(Inst, DL);
    }

    if(NodeBlock == EndBlock) {
      PathWeight->analyze(PathEndInst, DL);
      return PathWeight;
    }

    for(Node++; Node != EndNode; Node++) {
      NodeBlock = Node->getBlock();
      if(Node->isSubLoopExit()) {
        // Since the sub-loop exit block is in the middle of the path, it's by
        // definition exiting from a spanning path.  EnumerateLoopPaths doesn't
        // know about loops we've marked for transformation, however, so reset
        // the path weight for loops that'll have a migration point added to
        // their header.
        SubLoop = LI->getLoopFor(NodeBlock);
        assert(LoopWeights.count(SubLoop) && "Invalid traversal");
        const LoopWeightInfo &LWI = LoopWeights.at(SubLoop);
        if(TransformLoops.count(SubLoop)) {
          ActuallyEqPoint = true;
          PathWeight->reset();
        }

        // TODO we need to ultimately deal with the following situation more
        // gracefully:
        //
        //   loop 1: all spanning paths, contains loop 2
        //     loop 2: all spanning paths, contains loop 3
        //       loop 3: all spanning paths, to be instrumented
        //
        // Analysis determines loop 3 needs to be instrumented.  If all paths
        // in loop 2 go through loop 3, then loop 2 no longer has spanning
        // paths but only equivalence point paths.  The previous if statement
        // detects this, and reports it to calculateLoopExitWeights().  However
        // when analyzing paths through loop 1, we can't detect that loop 2
        // only has equivalence points paths.

        if(LWI.loopHasSpanningPath()) {
          PathWeight->add(LWI.getLoopSpanningPathWeight(false));
          PathWeight->add(LWI.getExitSpanningPathWeight(NodeBlock));
        }
        else {
          ActuallyEqPoint = true;
          PathWeight->reset();
          PathWeight->add(LWI[NodeBlock]);
        }
      }
      else {
        for(Inst = NodeBlock->begin(), EndInst = NodeBlock->end();
            Inst != EndInst && Inst != PathEndInst; Inst++)
          PathWeight->analyze(Inst, DL);
      }

      if(NodeBlock == EndBlock) break;
    }
    PathWeight->analyze(PathEndInst, DL);

    return PathWeight;
  }

  /// Analyze a path in a loop and return its weight.  Doesn't do any marking.
  ///
  /// Note: returns a dynamically allocated object to be managed by the caller
  Weight *traversePath(const LoopPath *LP, bool &ActuallyEqPoint) const {
    DEBUG(dbgs() << "  + Analyzing loop path: "; LP->dump());
    return traversePathInternal(LP, LP->endInst(), ActuallyEqPoint);
  }

  /// Analyze a path until a given exit block & return path's weight up until
  /// the exit point.
  ///
  /// Note: returns a dynamically allocated object to be managed by the caller
  Weight *traversePathUntilExit(const LoopPath *LP,
                                BasicBlock *Exit,
                                bool &ActuallyEqPoint) const
  { return traversePathInternal(LP, Exit->getTerminator(), ActuallyEqPoint); }

  /// Get the loop trip count if available and less than UINT32_MAX, or 0
  /// otherwise.
  ///
  /// Note: ported from ScalarEvolution::getSmallConstantMaxTripCount() in
  /// later LLVM releases.
  unsigned getTripCount(const Loop *L) const {
    const SCEVConstant *MaxExitCount =
      dyn_cast<SCEVConstant>(SE->getMaxBackedgeTakenCount(L));
    if(!MaxExitCount) return 0;
    ConstantInt *ExitConst = MaxExitCount->getValue();
    if(ExitConst->getValue().getActiveBits() > 32) return 0;
    else return ((unsigned)ExitConst->getZExtValue()) + 1;
  }

  /// Calculate the exit weights of a loop at all exit points.
  void calculateLoopExitWeights(Loop *L) {
    assert(!LoopWeights.count(L) && "Previously analyzed loop?");

    bool HasSpPath = false, HasEqPointPath = false, ActuallyEqPoint;
    std::vector<const LoopPath *> Paths;
    LoopWeights.emplace(L, LoopWeightInfo(L));
    LoopWeightInfo &LWI = LoopWeights.at(L);
    SmallVector<BasicBlock *, 4> ExitBlocks;
    WeightPtr SpanningWeight(getZeroWeight()),
              EqPointWeight(getZeroWeight());
    LP->getBackedgePaths(L, Paths);

    DEBUG(dbgs() << "\n    Calculating loop path weights: "
                 << std::to_string(Paths.size()) << " backedge path(s)\n");

    // Analyze weights of individual paths through the loop that end at a
    // backedge, as these will dictate the loop's weight.
    for(auto Path : Paths) {
      WeightPtr PathWeight(traversePath(Path, ActuallyEqPoint));
      DEBUG(dbgs() << "    Path weight: " << PathWeight->toString() << " ");
      if(Path->isSpanningPath() && !ActuallyEqPoint) {
        HasSpPath = true;
        SpanningWeight->max(PathWeight);
        DEBUG(dbgs() << "(spanning path)\n");
      }
      else {
        HasEqPointPath = true;
        EqPointWeight->max(PathWeight);
        DEBUG(dbgs() << "(equivalence point path)\n");
      }
    }

    // Calculate/store the loop's spanning and equivalence point path weights.
    if(HasSpPath) {
      // Optimization: if the loop trip count is smaller than the number of
      // iterations between migration points, elide loop instrumentation.
      size_t NumIters = SpanningWeight->numIters(CurCapThresh);
      unsigned TripCount = getTripCount(L);
      assert(NumIters > 0 && "Should have added a migration point");
      if(TripCount && TripCount < NumIters) {
        DEBUG(dbgs() << "  Eliding loop instrumentation, loop trip count: "
                     << std::to_string(TripCount) << "\n");
        NumIters = TripCount;
      }
      else if(L->getLoopDepth() > 1 &&
              NumIters > (size_t)MaxItersPerMigPoint) {
        DEBUG(dbgs() << "  Eliding loop instrumentation (exceeded maximum "
                        " iterations per migration point), loop trip count: "
                     << std::to_string(MaxItersPerMigPoint) << "\n");
        NumIters = MaxItersPerMigPoint;
      }
      // TODO mark first insertion point in loop header as migration point,
      // propagate whether we added a migration point as return value
      else TransformLoops.insert(L);
      LWI.setLoopSpanningPathWeight(SpanningWeight, NumIters);

      DEBUG(
        dbgs() << "  Loop spanning path weight: " << SpanningWeight->toString()
               << ", " << std::to_string(NumIters)
               << " iteration(s)/migration point\n";
      );
    }
    if(HasEqPointPath) {
      LWI.setLoopEqPointPathWeight(EqPointWeight);

      DEBUG(dbgs() << "  Loop equivalence point path weight: "
                   << EqPointWeight->toString() << "\n");
    }

    DEBUG(dbgs() << "\n    Calculating loop exit weights");

    // Calculate the weight of the loop at every exit point.  Maintain separate
    // spanning & equivalence point path exit weights so that if we avoid
    // instrumenting loop boundaries in traverseLoopEntry() we can update the
    // exit weights.
    L->getExitingBlocks(ExitBlocks);
    for(auto Exit : ExitBlocks) {
      HasSpPath = HasEqPointPath = false;
      SpanningWeight.reset(getZeroWeight());
      EqPointWeight.reset(getZeroWeight());

      LP->getPathsThroughBlock(L, Exit, Paths);
      for(auto Path : Paths) {
        WeightPtr PathWeight(traversePathUntilExit(Path, Exit, ActuallyEqPoint));
        if(Path->isSpanningPath() && !ActuallyEqPoint) {
          HasSpPath = true;
          SpanningWeight->max(PathWeight);
        }
        else {
          HasEqPointPath = true;
          EqPointWeight->max(PathWeight);
        }
      }

      if(HasSpPath) LWI.setExitSpanningPathWeight(Exit, SpanningWeight);
      if(HasEqPointPath) LWI.setExitEqPointPathWeight(Exit, EqPointWeight);
    }
  }

  /// Analyze loop nests & mark locations for migration points.  Return whether
  /// or not a migration point was added.
  bool traverseLoopNest(const std::vector<BasicBlock *> &SCC) {
    bool AddedMigPoint = false;
    Loop *L;
    LoopNest Nest;

    // Get outermost loop in loop nest & enumerate the rest of the nest
    assert(LI->getLoopFor(SCC.front()) && "No loop in SCC");
    L = LI->getLoopFor(SCC.front());
    while(L->getLoopDepth() != 1) L = L->getParentLoop();
    LoopPathUtilities::populateLoopNest(L, Nest);

    DEBUG(
      dbgs() << " + Analyzing loop nest at "; L->getStartLoc().print(dbgs());
      dbgs() << " with " << std::to_string(Nest.size()) << " loop(s)\n\n";
    );

    for(auto CurLoop : Nest) {
      // Note: if migration points were added to any sub-loo(s) then we need to
      // re-run the LoopPaths analysis on the outer loop.
      // TODO this is a little overzealous, sibling loops (e.g., 2 sub-loops at
      // the same depth and contained in the same outer loop) can cause
      // unnecessary re-enumerations.
      if(traverseLoop(CurLoop) || AddedMigPoint) {
        AddedMigPoint = true;
        LP->rerunOnLoop(CurLoop);
      }

      // TODO if we are instrumenting the loop header, re-enumerate paths
      calculateLoopExitWeights(CurLoop);

      DEBUG(dbgs() << "\n  Loop analysis: "
                   << LoopWeights.at(CurLoop).toString() << "\n");
    }

    DEBUG(dbgs() << " - Finished loop nest\n");

    return AddedMigPoint;
  }

  /// Analyze the function's body to add migration points.  Return whether or
  /// not a migration point was added.
  bool analyzeFunctionBody(Function &F) {
    std::set<const Loop *> MarkedLoops;
    bool AddedMigPoint = false;
    Loop *BlockLoop;

    // Analyze & mark paths through loop nests
    DEBUG(dbgs() << "\n-> Analyzing loop nests <-\n");
    for(scc_iterator<Function *> SCC = scc_begin(&F), E = scc_end(&F);
        SCC != E; ++SCC)
      if(SCC.hasLoop()) AddedMigPoint |= traverseLoopNest(*SCC);

    // Analyze the rest of the function body
    DEBUG(dbgs() << "\n-> Analyzing the rest of the function body <-\n");
    ReversePostOrderTraversal<Function *> RPOT(&F);
    for(auto BB = RPOT.begin(), BE = RPOT.end(); BB != BE; ++BB) {
      BlockLoop = LI->getLoopFor(*BB);
      if(!BlockLoop) {
        WeightPtr PredWeight(getInitialWeight(*BB));
        AddedMigPoint |= traverseBlock(*BB, PredWeight);
        BBWeights[*BB] = std::move(PredWeight);
      }
      else if(!MarkedLoops.count(BlockLoop)) {
        // Block is in a loop, analyze & mark loop's boundaries
        AddedMigPoint |= traverseLoopEntry(BlockLoop);
        MarkedLoops.insert(BlockLoop);
      }
    }

    // Finally, determine if we should add a migration point at exit block(s).
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++) {
      if(isa<ReturnInst>(BB->getTerminator())) {
        assert(!LI->getLoopFor(BB) && "Returning inside a loop");
        assert(BBWeights.count(BB) && "Missing block weight");
        const BasicBlockWeightInfo &BBWI = BBWeights[BB].BlockWeight;
        if(!BBWI.BlockWeight->underPercentOfThreshold(CurRetThresh)) {
          DEBUG(dbgs() << " - Not under weight threshold, marking return\n");
          markAsMigPoint(BB->getTerminator(), true, true);
        }
      }
    }

    return AddedMigPoint;
  }

  //===--------------------------------------------------------------------===//
  // Instrumentation implementation
  //===--------------------------------------------------------------------===//

  /// Either find an existing induction variable (and its stride), or create
  /// one for a loop.
  Instruction *getInductionVariable(Loop *L, size_t &Stride) {
    BasicBlock *H = L->getHeader();
    const SCEVAddRecExpr *Induct;
    const SCEVConstant *StrideExpr;
    Type *IVTy;

    // Search for the induction variable & it's stride
    for(BasicBlock::iterator I = H->begin(), E = H->end(); I != E; ++I) {
      if(!isa<PHINode>(*I)) break;
      IVTy = I->getType();
      if(IVTy->isPointerTy() || !SE->isSCEVable(IVTy)) continue;
      Induct = dyn_cast<SCEVAddRecExpr>(SE->getSCEV(I));
      if(Induct && isa<SCEVConstant>(Induct->getStepRecurrence(*SE))) {
        StrideExpr = cast<SCEVConstant>(Induct->getStepRecurrence(*SE));
        Stride = std::abs(StrideExpr->getValue()->getSExtValue());

        // TODO if stride != 1, it's hard to ensure we're hitting a migration
        // point every n iterations unless we know the *exact* number at which
        // it starts.  For example, if stride = 4 but we start at 1, the
        // migration point checking logic has to add checks for 1, 5, 9, etc.
        // It's easier to just create our own induction variable.
        if(Stride != 1) continue;

        DEBUG(dbgs() << "Found induction variable with loop stride of "
                     << std::to_string(Stride) << ":"; I->print(dbgs());
              dbgs() << "\n");

        return I;
      }
    }

    DEBUG(dbgs() << "No induction variable, adding'migpoint.iv."
                 << std::to_string(NumIVsAdded) << "' to the loop\n");

    LLVMContext &C = H->getContext();
    Type *Int32Ty = Type::getInt32Ty(C);
    IRBuilder<> PhiBuilder(H->getFirstInsertionPt());
    PHINode *IV = PhiBuilder.CreatePHI(Int32Ty, 0,
      "migpoint.iv." + std::to_string(NumIVsAdded++));
    Constant *One = ConstantInt::get(Int32Ty, 1, 0),
             *Zero = ConstantInt::get(Int32Ty, 0, 0);
    for(auto Pred : predecessors(H)) {
      IRBuilder<> AddRecBuilder(Pred->getTerminator());
      if(L->contains(Pred)) { // Backedge
        Value *RecVal = AddRecBuilder.CreateAdd(IV, One);
        IV->addIncoming(RecVal, Pred);
      }
      else IV->addIncoming(Zero, Pred);
    }

    Stride = 1;
    return IV;
  }

  /// Round a value down to the nearest power of 2.  Stolen/modified from
  /// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  unsigned roundDownPowerOf2(unsigned Count) {
    unsigned Starting = Count;
    Count--;
    Count |= Count >> 1;
    Count |= Count >> 2;
    Count |= Count >> 4;
    Count |= Count >> 8;
    Count |= Count >> 16;
    Count++;

    // If we're already a power of 2, then the above math returns the same
    // value.  Otherwise, we've rounded *up* to the nearest power of 2 and need
    // to divide by 2 to round *down*.
    if(Count != Starting) Count >>= 1;
    return Count;
  }

  /// Transform a loop header so that migration points (and any concomitant
  /// costs) are only experienced every nth iteration, based on weight metrics
  void transformLoopHeader(Loop *L) {
    BasicBlock *Header = L->getHeader();
    size_t ItersPerMigPoint, Stride = 0, InstrStride;

    // If the first instruction has already been marked due to heuristics that
    // bookend libc I/O & big memory operations, then there's nothing to do.
    Instruction *First = Header->getFirstInsertionPt();
    if(isMarkedForInstrumentation(First)) return;

    DEBUG(dbgs() << "+ Instrumenting "; L->dump());

    assert(LoopWeights.count(L) && "No loop analysis");
    ItersPerMigPoint = LoopWeights.at(L).getItersPerMigPoint();

    if(ItersPerMigPoint > 1) {
      BasicBlock *NewSuccBB, *MigPointBB;
      Instruction *IV = getInductionVariable(L, Stride);

      IntegerType *IVType = cast<IntegerType>(IV->getType());
      Function *CurF = Header->getParent();
      LLVMContext &C = Header->getContext();

      // Create new successor for all instructions after migration point
      NewSuccBB = Header->splitBasicBlock(Header->getFirstInsertionPt(),
        "l.postmigpoint" + std::to_string(LoopsTransformed));

      // Create new block for migration point
      MigPointBB = BasicBlock::Create(C,
        "l.migpoint" + std::to_string(LoopsTransformed), CurF, NewSuccBB);
      IRBuilder<> MigPointWorker(MigPointBB);
      Instruction *Br = cast<Instruction>(MigPointWorker.CreateBr(NewSuccBB));
      markAsMigPoint(Br, true, true);

      // Add check and branch to migration point only every nth iteration.
      // Round down to nearest power-of-2, which allows us to use a simple
      // bitmask for migration point check (URem instructions can cause
      // non-negligible overhead in tight-loops).
      IRBuilder<> Worker(Header->getTerminator());
      InstrStride = roundDownPowerOf2(ItersPerMigPoint * Stride) - 1;
      assert(InstrStride > 0 && "Invalid migration point stride");
      Constant *N = ConstantInt::get(IVType, InstrStride, IVType->getSignBit()),
               *Zero = ConstantInt::get(IVType, 0, IVType->getSignBit());
      Value *Rem = Worker.CreateAnd(IV, N);
      Value *Cmp = Worker.CreateICmpEQ(Rem, Zero);
      Worker.CreateCondBr(Cmp, MigPointBB, NewSuccBB);
      Header->getTerminator()->eraseFromParent();

      DEBUG(dbgs() << "Instrumenting to hit migration point every "
                   << std::to_string(InstrStride + 1) << " iterations\n");
    }
    else {
      DEBUG(dbgs() << "Instrumenting to hit migration point every iteration\n");
      markAsMigPoint(Header->getFirstInsertionPt(), true, true);
    }
  }

  /// Insert migration points & HTM instrumentation for instructions.
  void transformLoopHeaders(Function &F) {
    DEBUG(dbgs() << "\n-> Transforming loop headers <-\n");
    for(auto Loop : TransformLoops) {
      transformLoopHeader(Loop);
      LoopsTransformed++;
    }
  }
};

} /* end anonymous namespace */

char SelectMigrationPoints::ID = 0;

INITIALIZE_PASS_BEGIN(SelectMigrationPoints, "select-migration-points",
                      "Select migration points locations", true, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(EnumerateLoopPaths)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolution)
INITIALIZE_PASS_END(SelectMigrationPoints, "select-migration-points",
                    "Select migration points locations", true, false)

const StringSet<> SelectMigrationPoints::LibcIO = {
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
  "__isoc99_fscanf", "exit"
};

namespace llvm {
  FunctionPass *createSelectMigrationPointsPass()
  { return new SelectMigrationPoints(); }
}

