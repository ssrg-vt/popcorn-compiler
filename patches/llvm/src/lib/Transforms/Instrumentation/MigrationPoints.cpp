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
// execution state, like registers and stack, across different ISAs.  In our
// implementation, every function call site is an equivalence point; hence,
// calls inserted to invoke the migration by definition create equivalence
// points at the migration point.  Thus, all migration points are equivalence
// points, but not all equivalence points are migration points.
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
#include <fstream>
#include <map>
#include <memory>
#include "llvm/Pass.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPaths.h"
#include "llvm/Analysis/PopcornUtil.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/InlineAsm.h"
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
#define MIGRATE_FLAG_NAME "__migrate_flag"

/// Insert more migration points into the body of a function.  Analyze memory
/// usage & attempt to instrument the code to reduce the time until the thread
/// reaches a migration point.  If HTM instrumentation is enabled, analysis is
/// tailored to avoid hardware transactional memory (HTM) capacity aborts.
const static cl::opt<bool>
MoreMigPoints("more-mig-points", cl::Hidden, cl::init(false),
  cl::desc("Add additional migration points into the body of functions"));

/// Percent of capacity (determined by analysis type, e.g., HTM buffer size) at
/// which point weight objects will request a new migration point be inserted.
static cl::opt<unsigned>
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
static cl::opt<unsigned>
StartThreshold("start-threshold", cl::Hidden, cl::init(5),
  cl::desc("Don't instrument function entry points under a percent of "
           "capacity (only applies to -more-mig-points), used for "
           "small functions"));

/// Per-function starting threshold.
const static cl::list<std::string>
FuncStartThreshold("func-start", cl::Hidden, cl::ZeroOrMore,
  cl::desc("Function-specific start threshold in function,value pairs"));

/// Normally we instrument function entry & exit points with migration points.
/// If we're below some percent of capacity, skip this instrumentation (useful
/// for very small/short-lived functions).
static cl::opt<unsigned>
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
const static cl::opt<size_t>
MillionCyclesBetweenMigPoints("migpoint-cycles", cl::Hidden, cl::init(50),
  cl::desc("Cycles between migration points, in millions of cycles"));

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
HTMReadBufSizeArg("htm-buf-read", cl::Hidden, cl::init(32),
  cl::desc("HTM analysis tuning - HTM read buffer size, in kilobytes"),
  cl::value_desc("size"));

/// HTM memory write buffer size for tuning analysis when inserting additional
/// migration points.
const static cl::opt<unsigned>
HTMWriteBufSizeArg("htm-buf-write", cl::Hidden, cl::init(8),
  cl::desc("HTM analysis tuning - HTM write buffer size, in kilobytes"),
  cl::value_desc("size"));

/// Add counters to abort handlers for the specified function.  Allows in-depth
/// profiling of which HTM sections added to the function are causing aborts.
const static cl::opt<std::string>
AbortCount("abort-count", cl::Hidden, cl::init(""),
  cl::desc("Add counters for each abort handler in the specified function"),
  cl::value_desc("function"));

#define KB 1024
#define HTMReadBufSize (HTMReadBufSizeArg * KB)
#define HTMWriteBufSize (HTMWriteBufSizeArg * KB)

#define MILLION 1000000
#define CyclesBetweenMigPoints (MillionCyclesBetweenMigPoints * MILLION)
#define MEM_WEIGHT 40

STATISTIC(NumMigPoints, "Number of migration points added");
STATISTIC(NumHTMBegins, "Number of HTM begin intrinsics added");
STATISTIC(NumHTMEnds, "Number of HTM end intrinsics added");
STATISTIC(LoopsTransformed, "Number of loops transformed");
STATISTIC(NumIVsAdded, "Number of induction variables added");

namespace {

/// Get the integer size of a value, if statically known.
static int64_t getValueSize(const Value *V) {
  if(isa<ConstantInt>(V)) return cast<ConstantInt>(V)->getSExtValue();
  return -1;
}

/// Return a percentage of a value.
static inline float getValuePercent(size_t V, unsigned P) {
  assert(P <= 100 && "Invalid percentage");
  return ((float)V) * (((float)P) / 100.0f);
}

/// Return the number of cache lines accessed for a given number of
/// (assumed contiguous) bytes.
static inline size_t getNumCacheLines(size_t Bytes, unsigned LineSize) {
  return ceil((float)Bytes / (float)LineSize);
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
  virtual bool shouldAddMigPoint() const {
    return !underPercentOfThreshold(CapacityThreshold);
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
  virtual size_t numIters() const = 0;

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
  virtual size_t numIters() const {
    size_t NumLoadIters = UINT64_MAX, NumStoreIters = UINT64_MAX;
    float FPHtmReadSize = getValuePercent(HTMReadBufSize, CapacityThreshold),
          FPHtmWriteSize = getValuePercent(HTMWriteBufSize, CapacityThreshold);

    if(!LoadBytes && !StoreBytes) return 1024; // Return a safe value
    else {
      if(LoadBytes) NumLoadIters = FPHtmReadSize / (float)LoadBytes;
      if(StoreBytes) NumStoreIters = FPHtmWriteSize / (float)StoreBytes;
      return NumLoadIters < NumStoreIters ? NumLoadIters : NumStoreIters;
    }
  }

  virtual bool underPercentOfThreshold(unsigned percent) const {
    if((float)LoadBytes <= getValuePercent(HTMReadBufSize, percent) &&
       (float)StoreBytes <= getValuePercent(HTMWriteBufSize, percent))
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

  virtual size_t numIters() const {
    if(!Cycles) return 1048576; // Return a safe value
    else {
      float FPCycleCap = getValuePercent(CyclesBetweenMigPoints,
                                         CapacityThreshold);
      return FPCycleCap / (float)Cycles;
    }
  }

  virtual bool underPercentOfThreshold(unsigned percent) const {
    if((float)Cycles <= getValuePercent(CyclesBetweenMigPoints, percent))
      return true;
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
static Weight *getZeroWeight(bool DoHTMInst) {
  if(DoHTMInst) return new HTMWeight();
  else return new CycleWeight();
}

/// MigrationPoints - insert migration points into functions, optionally adding
/// HTM execution.
class MigrationPoints : public FunctionPass
{
public:
  static char ID;

  MigrationPoints() : FunctionPass(ID) {
    initializeMigrationPointsPass(*PassRegistry::getPassRegistry());
  }
  ~MigrationPoints() {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<EnumerateLoopPaths>();
    AU.addRequired<ScalarEvolution>();
  }

  virtual const char *getPassName() const
  { return "Insert migration points"; }

  /// Generate the migration library API function declaration.
  void addMigrationIntrinsic(Module &M, bool DoHTM) {
    LLVMContext &C = M.getContext();
    Type *VoidTy = Type::getVoidTy(C);
    PointerType *VoidPtrTy = Type::getInt8PtrTy(C, 0);
    std::vector<Type *> FuncPtrArgTy = { VoidPtrTy };
    FunctionType *FuncPtrTy = FunctionType::get(VoidTy, FuncPtrArgTy, false);
    CallbackType = PointerType::get(FuncPtrTy, 0);
    std::vector<Type *> ArgTy = { CallbackType, VoidPtrTy };
    FunctionType *FuncTy = FunctionType::get(VoidTy, ArgTy, false);
    if(DoHTM) {
      MigrateAPI = M.getOrInsertFunction("migrate", FuncTy);
      MigrateFlag = cast<GlobalValue>(
        M.getOrInsertGlobal(MIGRATE_FLAG_NAME, Type::getInt32Ty(C)));
      // TODO this needs to be thread-local storage
      //MigrateFlag->setThreadLocal(true);
    }
    else {
      MigrateAPI = M.getOrInsertFunction("check_migrate", FuncTy);
      MigrateFlag = nullptr;
    }
  }

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
    bool AddedHTM = false;
    DL = &M.getDataLayout();
    Triple TheTriple(M.getTargetTriple());
    Arch = TheTriple.getArch();

    if(MoreMigPoints) parsePerFuncThresholds();

    // If instrumenting with HTM, add begin/end/test intrinsic declarations or
    // warn user if HTM is not supported on this architecture.
    if(HTMExec) {
      if(HTMBegin.find(Arch) != HTMBegin.end()) {
        HTMBeginDecl =
          Intrinsic::getDeclaration(&M, HTMBegin.find(Arch)->second);
        HTMEndDecl = Intrinsic::getDeclaration(&M, HTMEnd.find(Arch)->second);
        HTMTestDecl = Intrinsic::getDeclaration(&M, HTMTest.find(Arch)->second);
        addMigrationIntrinsic(M, true);
        AddedHTM = true;
      }
      else {
        std::string Msg("HTM instrumentation not supported for '");
        Msg += TheTriple.getArchName();
        Msg += "'";
        DiagnosticInfoInlineAsm DI(Msg, DiagnosticSeverity::DS_Warning);
        M.getContext().diagnose(DI);
      }
    }

    if(AbortCount != "" && M.getFunction(AbortCount)) {
      LLVMContext &C = M.getContext();
      IntegerType *Unsigned = Type::getInt32Ty(C);
      GlobalVariable *NumCtrs = cast<GlobalVariable>(
        M.getOrInsertGlobal("__num_abort_counters", Unsigned));
      NumCtrs->setInitializer(ConstantInt::get(Unsigned, 1024, false));
      Type *ArrType = ArrayType::get(Type::getInt64Ty(C), 1024);
      AbortCounters = cast<GlobalVariable>(
        M.getOrInsertGlobal("__abort_counters", ArrType));
      AbortCounters->setInitializer(ConstantAggregateZero::get(ArrType));
    }

    if(!AddedHTM) addMigrationIntrinsic(M, false);
    return true;
  }

  /// Insert migration points into functions
  virtual bool runOnFunction(Function &F)
  {
    DEBUG(dbgs() << "\n********** ADD MIGRATION POINTS **********\n"
                 << "********** Function: " << F.getName() << "\n\n");

    initializeAnalysis(F);

    // If the user has requested the function not be instrumented, do some
    // basic marking (to avoid spurious HTM aborts) and return.
    if(NoInstFuncs.find(F.getName()) != NoInstFuncs.end()) {
      markAsHTMEnd(F.getEntryBlock().getFirstInsertionPt());
      addMigrationPoints(F);
      return true;
    }

    // Some operations (e.g., big memory copies, I/O) will cause aborts.
    // Instrument these operations to stop & resume transactions afterwards.
    if(DoHTMInst) {
      bool AddedMigPoint;

      AddedMigPoint = wrapWithHTM(F, isBigMemoryOp,
        "memory operations that overflow HTM buffers");
      if(!NoWrapLibc)
        AddedMigPoint |= wrapWithHTM(F, isLibcIO, "I/O functions");

      if(AddedMigPoint) LP->runOnFunction(F);
    }

    if(MoreMigPoints) {
      // Check if user specified function-specific thresholds.
      unsigned OldCap = UINT32_MAX, OldStart = UINT32_MAX, OldRet = UINT32_MAX;
      StringRef FuncName = F.getName();
      StringMap<unsigned>::const_iterator It;
      if((It = FuncCapList.find(FuncName)) != FuncCapList.end()) {
        OldCap = CapacityThreshold;
        CapacityThreshold = It->second;
      }
      if((It = FuncStartList.find(FuncName)) != FuncStartList.end()) {
        OldStart = StartThreshold;
        StartThreshold = It->second;
      }
      if((It = FuncRetList.find(FuncName)) != FuncRetList.end()) {
        OldRet = RetThreshold;
        RetThreshold = It->second;
      }

      DEBUG(
        dbgs() << "\n-> Analyzing function body to add migration points <-\n"
               << "\nCapacity threshold: " << std::to_string(CapacityThreshold)
               << "\nStart threshold: " << std::to_string(StartThreshold)
               << "\nReturn threshold: " << std::to_string(RetThreshold) << "\n"
      );

      // We by default mark the function start as a migration point, but if we
      // don't add any instrumentation & the function's exit weights are
      // sufficiently small avoid instrumentation altogether.
      bool MarkStart = false;
      if(!analyzeFunctionBody(F)) {
        for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++)
          if(isa<ReturnInst>(BB->getTerminator()) &&
             !BBWeights[BB].BlockWeight->underPercentOfThreshold(StartThreshold))
            MarkStart = true;
      }
      else MarkStart = true;

      if(MarkStart) {
        DEBUG(dbgs() << "-> Marking function entry as a migration point <-\n");
        markAsMigPoint(F.getEntryBlock().getFirstInsertionPt(), true, true);
      }
      else { DEBUG(dbgs() << "-> Eliding instrumenting function entry <-\n"); }

      // Restore global thresholds.
      if(OldCap != UINT32_MAX) CapacityThreshold = OldCap;
      if(OldStart != UINT32_MAX) StartThreshold = OldStart;
      if(OldRet != UINT32_MAX) RetThreshold = OldRet;
    }
    else {
      DEBUG(dbgs() << "-> Marking function entry as a migration point <-\n");
      markAsMigPoint(F.getEntryBlock().getFirstInsertionPt(), true, true);

      // Instrument function exit point(s)
      DEBUG(dbgs() << "-> Marking function exit(s) as a migration point <-\n");
      for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++)
        if(isa<ReturnInst>(BB->getTerminator()))
          markAsMigPoint(BB->getTerminator(), true, true);
    }

    // Finally, apply code transformations to marked instructions.
    addMigrationPoints(F);

    // Write the modified IR & close the abort handler map file if we
    // instrumented the code to profile abort handlers.
    if(MapFile.is_open()) {
      MapFile.close();
      std::fstream TheIR("htm-abort-ir.ll", std::ios::out | std::ios::trunc);
      raw_os_ostream IRStream(TheIR);
      F.print(IRStream);
      TheIR.close();
    }

    return true;
  }

  /// Reset all analysis.
  virtual void initializeAnalysis(const Function &F) {
    DoHTMInst = false;
    DoAbortInstrument = false;
    SE = &getAnalysis<ScalarEvolution>();
    LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    LP = &getAnalysis<EnumerateLoopPaths>();
    BBWeights.clear();
    LoopWeights.clear();
    LoopMigPoints.clear();
    MigPointInsts.clear();
    HTMBeginInsts.clear();
    HTMEndInsts.clear();

    if(HTMExec) {
      // We've checked at a global scope whether the architecture supports HTM,
      // but we need to check whether the target-specific feature for HTM is
      // enabled for the current function
      if(!F.hasFnAttribute("target-features")) {
        DEBUG(dbgs() << "-> Disabled HTM instrumentation, "
                        "no 'target-features' attribute\n");
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

      DoHTMInst = (pos != StringRef::npos);

      // Enable HTM abort handler profiling if specified
      if(DoHTMInst && AbortCount == F.getName()) {
        DoAbortInstrument = true;
        AbortHandlerCount = 0;
        MapFile.open("htm-abort.map", std::ios::out | std::ios::trunc);
        assert(MapFile.is_open() && MapFile.good() &&
               "Could not open abort handler map file");
      }

      DEBUG(
        if(DoHTMInst) dbgs() << "-> Enabling HTM instrumentation\n";
        else dbgs() << "-> Disabled HTM instrumentation, HTM not listed in "
                       "target-features\n";
      );
    }
  }

private:
  //===--------------------------------------------------------------------===//
  // Types & fields
  //===--------------------------------------------------------------------===//

  /// The current architecture - used to access architecture-specific HTM calls
  Triple::ArchType Arch;
  const DataLayout *DL;

  /// Parsed per-function thresholds.
  StringMap<unsigned> FuncCapList;
  StringMap<unsigned> FuncStartList;
  StringMap<unsigned> FuncRetList;
  StringSet<> NoInstFuncs;

  /// Should we instrument code with HTM execution?  Set if HTM is enabled on
  /// the command line and if the target is supported
  bool DoHTMInst;

  /// Should we instrument HTM abort handlers with counters for precise
  /// profiling of which code locations cause aborts & all associated state.
  bool DoAbortInstrument;
  GlobalVariable *AbortCounters;
  unsigned AbortHandlerCount;
  std::ofstream MapFile;

  /// Analyses on which we depend
  ScalarEvolution *SE;
  LoopInfo *LI;
  EnumerateLoopPaths *LP;

  /// Function declaration & migration node ID for migration library API
  Constant *MigrateAPI;
  GlobalValue *MigrateFlag;
  PointerType *CallbackType;

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
    /// Copy of MigrationPoint's DoHTMInst, needed to get zero weights.
    bool DoHTMInst;

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
      WeightPtr BBWeight(getZeroWeight(DoHTMInst));
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
    LoopWeightInfo(const Loop *L, bool DoHTMInst)
      : DoHTMInst(DoHTMInst), EntryWeight(getZeroWeight(DoHTMInst)),
        LoopHasSpanningPath(false), LoopHasEqPointPath(false),
        ItersPerMigPoint(0) {
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
  SmallPtrSet<Loop *, 16> LoopMigPoints;
  SmallPtrSet<Instruction *, 32> MigPointInsts;
  SmallPtrSet<Instruction *, 32> HTMBeginInsts;
  SmallPtrSet<Instruction *, 32> HTMEndInsts;

  //===--------------------------------------------------------------------===//
  // Analysis implementation
  //===--------------------------------------------------------------------===//

  /// Return whether the instruction requires HTM begin instrumentation.
  bool shouldAddHTMBegin(Instruction *I) const { return HTMBeginInsts.count(I); }

  /// Return whether the instruction requires HTM end instrumentation.
  bool shouldAddHTMEnd(Instruction *I) const { return HTMEndInsts.count(I); }

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
    if(!DoHTMInst) return false;
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as HTM begin\n");
    HTMBeginInsts.insert(I);
    return true;
  }

  /// Mark an instruction to be instrumented with an HTM end, directly before
  /// the instruction
  bool markAsHTMEnd(Instruction *I) {
    if(!DoHTMInst) return false;
    DEBUG(dbgs() << "  + Marking"; I->print(dbgs());
          dbgs() << " as HTM end\n");
    HTMEndInsts.insert(I);
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
  typedef bool (*InstMatch)(const Instruction *);

  /// Return whether the instruction is a memory operation that will overflow
  /// HTM buffers.
  static bool isBigMemoryOp(const Instruction *I) {
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

    // If we can't get the size assume it's a big memory operation
    if(Size < 0) return true;
    else return Size >= getValuePercent(HTMReadBufSize, CapacityThreshold) ||
                Size >= getValuePercent(HTMWriteBufSize, CapacityThreshold);
  }

  /// Return whether the instruction is a libc I/O call.
  static bool isLibcIO(const Instruction *I) {
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
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++) {
        if(Matcher(I)) {
          markAsHTMEnd(I);

          // Search subsequent instructions for other libc calls to prevent
          // pathological transaction stop/starts.
          const static size_t searchSpan = 10;
          BasicBlock::iterator NextI(I->getNextNode());
          for(size_t rem = searchSpan; rem > 0 && NextI != E; rem--, NextI++) {
            if(Matcher(NextI)) {
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
    Weight *PredWeight = getZeroWeight(DoHTMInst);
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
      if(isMigrationPoint(I) || CurWeight->shouldAddMigPoint()) {
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
    if(LoopMigPoints.count(L)) return false;

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

    DEBUG(dbgs() << "       + Analyzing loop entry points, header weight: "
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
        if(SpExitWeight->shouldAddMigPoint()) InstrumentLoopEntry = true;
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
    WeightPtr HdrWeight(getZeroWeight(DoHTMInst));
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
        // Block is in a sub-loop, analyze & mark sub-loop's entry
        AddedMigPoint |= traverseLoopEntry(BlockLoop);
        MarkedLoops.insert(BlockLoop);
      }
    }

    DEBUG(dbgs() << "    Finished analyzing loop\n");

    return AddedMigPoint;
  }

  /// Analyze a path in a loop and return its weight.  Doesn't do any marking.
  ///
  /// Note: returns a dynamically allocated object to be managed by the caller
  Weight *traversePath(const LoopPath *LP, bool &ActuallyEqPoint) const {
    DEBUG(dbgs() << "  + Analyzing loop path: "; LP->dump(););
    assert(LP->cbegin() != LP->cend() && "Trivial loop path, no blocks");
    ActuallyEqPoint = false;

    // Note: path ending instructions should either be control flow or calls,
    // so they do not need to be analyzed.
    Loop *SubLoop;
    Weight *PathWeight = getZeroWeight(DoHTMInst);
    SetVector<PathNode>::const_iterator Node = LP->cbegin(),
                                        EndNode = LP->cend();
    const BasicBlock *NodeBlock = Node->getBlock();
    BasicBlock::const_iterator Inst(LP->startInst()),
                               EndInst(NodeBlock->end()),
                               PathEndInst(LP->endInst());


    if(Node->isSubLoopExit()) {
      // Since the sub-loop exit block is the start of the path, it's by
      // definition exiting from an equivalence point path.
      SubLoop = LI->getLoopFor(NodeBlock);
      assert(LoopWeights.count(SubLoop) && "Invalid traversal");
      const LoopWeightInfo &LWI = LoopWeights.at(SubLoop);
      PathWeight->add(LWI.getExitEqPointPathWeight(NodeBlock));
    }
    else {
      for(; Inst != EndInst && Inst != PathEndInst; Inst++)
        PathWeight->analyze(Inst, DL);
    }

    for(Node++; Node != EndNode; Node++) {
      NodeBlock = Node->getBlock();
      if(Node->isSubLoopExit()) {
        // Since the sub-loop exit block is in the middle of the path, it's by
        // definition exiting from a spanning path
        SubLoop = LI->getLoopFor(NodeBlock);
        assert(LoopWeights.count(SubLoop) && "Invalid traversal");
        const LoopWeightInfo &LWI = LoopWeights.at(SubLoop);

        // EnumerateLoopPaths doesn't know about loops we've marked for
        // transformation, so explicitly reset the path weight for loops
        // that'll have a migration point added to their header.
        if(LoopMigPoints.count(SubLoop)) {
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
        Inst = NodeBlock->begin();
        EndInst = NodeBlock->end();
        for(; Inst != EndInst && Inst != PathEndInst; Inst++)
          PathWeight->analyze(Inst, DL);
      }
    }

    return PathWeight;
  }

  /// Traverse a path until a given exit block & return path's weight up until
  /// the exit point.
  ///
  /// Note: returns a dynamically allocated object to be managed by the caller
  Weight *traversePathUntilExit(const LoopPath *LP,
                                BasicBlock *Exit,
                                bool &ActuallyEqPoint) const {
    assert(LP->cbegin() != LP->cend() && "Trivial loop path, no blocks");
    assert(LP->contains(Exit) && "Invalid path and/or exit block");
    ActuallyEqPoint = false;

    // Note: the path's end must be either the terminator of the exit block (if
    // the exit block is also a latch) or an equivalence point/backedge branch
    // further down the path from the exit block.
    Loop *SubLoop;
    Weight *PathWeight = getZeroWeight(DoHTMInst);
    SetVector<PathNode>::const_iterator Node = LP->cbegin(),
                                        EndNode = LP->cend();
    const BasicBlock *NodeBlock = Node->getBlock();
    BasicBlock::const_iterator Inst(LP->startInst()),
                               EndInst(Node->getBlock()->end()),
                               PathEndInst(Exit->end());

    if(Node->isSubLoopExit()) {
      // Since the sub-loop exit block is the start of the path, it's by
      // definition exiting from an equivalence point path.
      SubLoop = LI->getLoopFor(NodeBlock);
      assert(LoopWeights.count(SubLoop) && "Invalid traversal");
      const LoopWeightInfo &LWI = LoopWeights.at(SubLoop);
      PathWeight->add(LWI.getExitEqPointPathWeight(NodeBlock));
    }
    else {
      for(; Inst != EndInst && Inst != PathEndInst; Inst++)
        PathWeight->analyze(Inst, DL);
    }
    if(Node->getBlock() == Exit) return PathWeight;

    for(Node++; Node != EndNode; Node++) {
      NodeBlock = Node->getBlock();
      if(Node->isSubLoopExit()) {
        // Since the sub-loop exit block is in the middle of the path, it's by
        // definition exiting from a spanning path
        SubLoop = LI->getLoopFor(NodeBlock);
        assert(LoopWeights.count(SubLoop) && "Invalid traversal");
        const LoopWeightInfo &LWI = LoopWeights.at(SubLoop);

        // EnumerateLoopPaths doesn't know about loops we've marked for
        // transformation, so explicitly reset the path weight for loops
        // that'll have a migration point added to their header.
        if(LoopMigPoints.count(SubLoop)) {
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
        Inst = Node->getBlock()->begin();
        EndInst = Node->getBlock()->end();
        for(; Inst != EndInst && Inst != PathEndInst; Inst++)
          PathWeight->analyze(Inst, DL);
      }
      if(Node->getBlock() == Exit) break;
    }

    return PathWeight;
  }

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
    LoopWeights.emplace(L, LoopWeightInfo(L, DoHTMInst));
    LoopWeightInfo &LWI = LoopWeights.at(L);
    SmallVector<BasicBlock *, 4> ExitBlocks;
    WeightPtr SpanningWeight(getZeroWeight(DoHTMInst)),
              EqPointWeight(getZeroWeight(DoHTMInst));
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
      unsigned NumIters = SpanningWeight->numIters(),
               TripCount = getTripCount(L);
      assert(NumIters > 0 && "Should have added a migration point");
      if(TripCount && TripCount < NumIters) {
        DEBUG(dbgs() << "  Eliding loop instrumentation, loop trip count: "
                     << std::to_string(TripCount) << "\n");
        NumIters = TripCount;
      }
      // TODO mark first insertion point in loop header as migration point,
      // propagate whether we added a migration point as return value
      else LoopMigPoints.insert(L);
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
      SpanningWeight.reset(getZeroWeight(DoHTMInst));
      EqPointWeight.reset(getZeroWeight(DoHTMInst));

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
        if(!BBWI.BlockWeight->underPercentOfThreshold(RetThreshold)) {
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

  /// Add a migration point directly before an instruction.
  void addMigrationPoint(Instruction *I) {
    LLVMContext &C = I->getContext();
    IRBuilder<> Worker(I);
    std::vector<Value *> Args = {
      ConstantPointerNull::get(CallbackType),
      ConstantPointerNull::get(Type::getInt8PtrTy(C, 0))
    };
    Worker.CreateCall(MigrateAPI, Args);
  }

  // Note: because we're only supporting 2 architectures for now, we're not
  // going to abstract this out into the appropriate Target/* folders

  /// Add HTM begin which avoids doing any work unless there's an abort.  In
  /// the event of an abort, the instrumentation checks if it should migrate,
  /// and if so, invokes the migration API.
  void addHTMBeginInternal(Instruction *I, Value *Begin, Value *BeginSuccess) {
    LLVMContext &C = I->getContext();
    BasicBlock *CurBB = I->getParent(), *NewSuccBB, *FlagCheckBB, *MigPointBB;

    // Set up each of the new basic blocks
    NewSuccBB =
      CurBB->splitBasicBlock(I, "migpointsucc" + std::to_string(NumMigPoints));
    MigPointBB =
      BasicBlock::Create(C, "migpoint" + std::to_string(NumMigPoints),
                         CurBB->getParent(), NewSuccBB);
    FlagCheckBB =
      BasicBlock::Create(C, "migflagcheck" + std::to_string(NumMigPoints),
                         CurBB->getParent(), MigPointBB);

    // Add check & branch based on HTM begin result; if Begin == BeginSuccess,
    // we've successfully started the transaction.
    IRBuilder<> HTMWorker(CurBB->getTerminator());
    Value *Cmp = HTMWorker.CreateICmpEQ(Begin, BeginSuccess);
    HTMWorker.CreateCondBr(Cmp, NewSuccBB, FlagCheckBB);
    CurBB->getTerminator()->eraseFromParent();

    // Check flag to see if we should invoke migration library API.
    IRBuilder<> FlagCheckWorker(FlagCheckBB);
    if(DoAbortInstrument) {
      assert(AbortHandlerCount < 1024 && "Too abort handler many counters!");

      // Write the name of the basic block to the map file so we can map abort
      // counters to their basic blocks.
      if(!AbortHandlerCount) MapFile << FlagCheckBB->getName().str();
      else MapFile << " " << FlagCheckBB->getName().str();

      // Add instrumentation to increment the counter's value.
      std::string CtrNum(std::to_string(AbortHandlerCount));
      std::vector<Value *> Idx = {
        ConstantInt::get(Type::getInt64Ty(C), 0),
        ConstantInt::get(Type::getInt64Ty(C), AbortHandlerCount),
      };
      Value *One = ConstantInt::get(Type::getInt64Ty(C), 1, false);
      Value *GEP = FlagCheckWorker.CreateInBoundsGEP(AbortCounters, Idx,
                                                     "ctrptr" + CtrNum);
      Value *CtrVal = FlagCheckWorker.CreateLoad(GEP, "ctr" + CtrNum);
      Value *Inc = FlagCheckWorker.CreateAdd(CtrVal, One);
      FlagCheckWorker.CreateStore(Inc, GEP);

      AbortHandlerCount++;
    }
    Value *Flag = FlagCheckWorker.CreateLoad(MigrateFlag);
    Value *NegOne = ConstantInt::get(Type::getInt32Ty(C), -1, true);
    Cmp = FlagCheckWorker.CreateICmpEQ(Flag, NegOne);
    FlagCheckWorker.CreateCondBr(Cmp, NewSuccBB, MigPointBB);

    // Add call to migration library API.
    IRBuilder<> MigPointWorker(MigPointBB);
    std::vector<Value *> Args = {
      ConstantPointerNull::get(CallbackType),
      ConstantPointerNull::get(Type::getInt8PtrTy(C, 0))
    };
    MigPointWorker.CreateCall(MigrateAPI, Args);
    MigPointWorker.CreateBr(NewSuccBB);
  }

  /// Add a transactional execution begin intrinsic for PowerPC, optionally
  /// with rollback-only transactions.
  void addPowerPCHTMBegin(Instruction *I) {
    LLVMContext &C = I->getContext();
    IRBuilder<> Worker(I);
    std::vector<Value *> Args = { ConstantInt::get(Type::getInt32Ty(C),
                                                   NoROTPPC ? 0 : 1,
                                                   false) };
    Value *HTMBeginVal = Worker.CreateCall(HTMBeginDecl, Args);
    Value *Zero = ConstantInt::get(Type::getInt32Ty(C), 0, false);
    addHTMBeginInternal(I, HTMBeginVal, Zero);
  }

  /// Add a transactional execution begin intrinsice for x86.
  void addX86HTMBegin(Instruction *I) {
    LLVMContext &C = I->getContext();
    IRBuilder<> Worker(I);
    Value *HTMBeginVal = Worker.CreateCall(HTMBeginDecl);
    Value *Success = ConstantInt::get(Type::getInt32Ty(C), 0xffffffff, false);
    addHTMBeginInternal(I, HTMBeginVal, Success);
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
      ".htmendsucc" + std::to_string(NumHTMEnds));

    // Create an HTM end block, which ends the transaction and jumps to the
    // new successor
    HTMEndBB = BasicBlock::Create(C,
      ".htmend" + std::to_string(NumHTMEnds), CurF, NewSuccBB);
    IRBuilder<> EndWorker(HTMEndBB);
    EndWorker.CreateCall(HTMEndDecl);
    EndWorker.CreateBr(NewSuccBB);

    // Finally, add the HTM test & replace the unconditional branch created by
    // splitBasicBlock() with a conditional branch to either end the
    // transaction or continue on to the new successor
    IRBuilder<> PredWorker(CurBB->getTerminator());
    CallInst *HTMTestVal = PredWorker.CreateCall(HTMTestDecl);
    ConstantInt *Zero = ConstantInt::get(IntegerType::getInt32Ty(C), 0, true);
    Value *Cmp = PredWorker.CreateICmpNE(HTMTestVal, Zero,
      "htmcmp" + std::to_string(NumHTMEnds));
    PredWorker.CreateCondBr(Cmp, HTMEndBB, NewSuccBB);
    CurBB->getTerminator()->eraseFromParent();
  }

  /// Insert migration points & HTM instrumentation for instructions.
  void addMigrationPoints(Function &F) {
    DEBUG(
      dbgs() << "\n-> Instrumenting with migration points ";
      if(DoHTMInst) dbgs() << "& HTM ";
      dbgs() << "<-\n";
    );

    for(auto Loop : LoopMigPoints) {
      transformLoopHeader(Loop);
      LoopsTransformed++;
    }

    if(DoHTMInst) {
      // Note: need to add the HTM ends before begins
      for(auto I = HTMEndInsts.begin(), E = HTMEndInsts.end(); I != E; ++I) {
        switch(Arch) {
        case Triple::ppc64le: addPowerPCHTMEnd(*I); break;
        case Triple::x86_64: addX86HTMCheckAndEnd(*I); break;
        default: llvm_unreachable("HTM -- unsupported architecture");
        }
        NumHTMEnds++;
      }

      // The following APIs both insert HTM begins & migration points because
      // the control flow with/without abort handlers is intertwined
      for(auto I = HTMBeginInsts.begin(), E = HTMBeginInsts.end();
          I != E; ++I) {
        switch(Arch) {
        case Triple::ppc64le: addPowerPCHTMBegin(*I); break;
        case Triple::x86_64: addX86HTMBegin(*I); break;
        default: llvm_unreachable("HTM -- unsupported architecture");
        }
        NumHTMBegins++;
        NumMigPoints++;
      }
    }
    else {
      for(auto I = MigPointInsts.begin(), E = MigPointInsts.end();
          I != E; ++I) {
        addMigrationPoint(*I);
        NumMigPoints++;
      }
    }
  }
};

} /* end anonymous namespace */

char MigrationPoints::ID = 0;

const MigrationPoints::IntrinsicMap MigrationPoints::HTMBegin = {
  {Triple::x86_64, Intrinsic::x86_xbegin},
  {Triple::ppc64le, Intrinsic::ppc_tbegin}
};

const MigrationPoints::IntrinsicMap MigrationPoints::HTMEnd = {
  {Triple::x86_64, Intrinsic::x86_xend},
  {Triple::ppc64le, Intrinsic::ppc_tend}
};

const MigrationPoints::IntrinsicMap MigrationPoints::HTMTest = {
  {Triple::x86_64, Intrinsic::x86_xtest},
  {Triple::ppc64le, Intrinsic::ppc_ttest}
};

INITIALIZE_PASS_BEGIN(MigrationPoints, "migration-points",
                      "Insert migration points into functions",
                      true, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(EnumerateLoopPaths)
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
  "__isoc99_fscanf", "exit"
};

namespace llvm {
  FunctionPass *createMigrationPointsPass()
  { return new MigrationPoints(); }
}

