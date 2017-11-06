//===- MigrationPoints.cpp ------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Instruments the migration points selected by SelectMigrationPoints.  May
// additionally add HTM instrumentation if selected by user & supported by the
// architecture.
//
//===----------------------------------------------------------------------===//

#include <fstream>
#include <map>
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/PopcornUtil.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_os_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "migration-points"
#define MIGRATE_FLAG_NAME "__migrate_flag"

/// Disable rollback-only transactions for PowerPC.
const static cl::opt<bool>
NoROTPPC("htm-ppc-no-rot", cl::Hidden, cl::init(false),
  cl::desc("Disable rollback-only transactions in HTM instrumentation "
           "(PowerPC only)"));

/// Add counters to abort handlers for the specified function.  Allows in-depth
/// profiling of which HTM sections added to the function are causing aborts.
const static cl::opt<std::string>
AbortCount("abort-count", cl::Hidden, cl::init(""),
  cl::desc("Add counters for each abort handler in the specified function"),
  cl::value_desc("function"));

STATISTIC(NumMigPoints, "Number of migration points added");
STATISTIC(NumHTMBegins, "Number of HTM begin intrinsics added");
STATISTIC(NumHTMEnds, "Number of HTM end intrinsics added");

namespace {

/// MigrationPoints - insert migration points into functions, optionally adding
/// HTM execution.
class MigrationPoints : public FunctionPass
{
public:
  static char ID;

  MigrationPoints() : FunctionPass(ID) {}
  ~MigrationPoints() {}

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

  virtual bool doInitialization(Module &M) {
    Triple TheTriple(M.getTargetTriple());
    Arch = TheTriple.getArch();
    Ty = Popcorn::getInstrumentationType(M);

    switch(Ty) {
    case Popcorn::HTM:
      if(HTMBegin.find(Arch) != HTMBegin.end()) {
        DEBUG(dbgs() << "\n-> MigrationPoints: Adding HTM intrinsics for '"
                     << TheTriple.getArchName() << "' <-\n");
        HTMBeginDecl =
          Intrinsic::getDeclaration(&M, HTMBegin.find(Arch)->second);
        HTMEndDecl = Intrinsic::getDeclaration(&M, HTMEnd.find(Arch)->second);
        HTMTestDecl =
          Intrinsic::getDeclaration(&M, HTMTest.find(Arch)->second);
        addMigrationIntrinsic(M, true);
      }
      else {
        DEBUG(
          dbgs() << "\n-> MigrationPoints: Selected HTM instrumentation but '"
                 << TheTriple.getArchName()
                 << "' is not supported, falling back to call-outs <-\n"
        );
        Ty = Popcorn::Cycles;
        addMigrationIntrinsic(M, false);
      }
      break;
    case Popcorn::Cycles:
      addMigrationIntrinsic(M, false);
      break;
    case Popcorn::None:
      return false;
    default: llvm_unreachable("Unknown instrumentation type"); break;
    }

    // Add abort counters if somebody requested abort profiling.
    const Function *CounterFunc;
    if(AbortCount != "" && (CounterFunc = M.getFunction(AbortCount)) &&
       !CounterFunc->isDeclaration()) {
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

    return true;
  }

  /// Insert migration points into functions
  virtual bool runOnFunction(Function &F)
  {
    if(Ty == Popcorn::None) return false;

    DEBUG(dbgs() << "\n********** ADD MIGRATION POINTS **********\n"
                 << "********** Function: " << F.getName() << "\n\n");

    initializeAnalysis(F);

    // Find all instrumentation points marked by previous analysis passes.
    findInstrumentationPoints(F);

    // Apply code transformations to marked instructions, including adding
    // migration points & HTM instrumentation.
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
  void initializeAnalysis(const Function &F) {
    DoHTMInst = false;
    DoAbortInstrument = false;
    MigPointInsts.clear();
    HTMBeginInsts.clear();
    HTMEndInsts.clear();

    if(Ty == Popcorn::HTM) {
      // We've checked at the global scope whether HTM is enabled for the
      // module.  Check whether the target-specific feature for HTM is enabled
      // for the current function.
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

      DEBUG(if(!DoHTMInst) dbgs() << "-> Disabled HTM instrumentation, HTM "
                                     "not listed in target-features\n");

      // Enable HTM abort handler profiling if specified
      if(DoHTMInst && AbortCount == F.getName()) {
        DoAbortInstrument = true;
        AbortHandlerCount = 0;
        MapFile.open("htm-abort.map", std::ios::out | std::ios::trunc);
        assert(MapFile.is_open() && MapFile.good() &&
               "Could not open abort handler map file");
      }
    }

    DEBUG(
      if(DoHTMInst) {
        dbgs() << "-> Adding HTM instrumentation\n";
        if(DoAbortInstrument) dbgs() << "  - Adding abort counters\n";
      }
      else dbgs() << "-> Adding call-out instrumentation\n";
    );
  }

private:
  //===--------------------------------------------------------------------===//
  // Types & fields
  //===--------------------------------------------------------------------===//

  /// Type of the instrumentation to be applied to functions.
  enum Popcorn::InstrumentType Ty;

  /// The current architecture - used to access architecture-specific HTM calls
  Triple::ArchType Arch;

  /// Should we instrument code with HTM execution?  Set if HTM is enabled on
  /// the command line and if the target is supported
  bool DoHTMInst;

  /// Should we instrument HTM abort handlers with counters for precise
  /// profiling of which code locations cause aborts & all associated state.
  bool DoAbortInstrument;
  GlobalVariable *AbortCounters;
  unsigned AbortHandlerCount;
  std::ofstream MapFile;

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

  /// Code locations marked for instrumentation.
  SmallPtrSet<Instruction *, 32> MigPointInsts;
  SmallPtrSet<Instruction *, 32> HTMBeginInsts;
  SmallPtrSet<Instruction *, 32> HTMEndInsts;

  //===--------------------------------------------------------------------===//
  // Instrumentation implementation
  //===--------------------------------------------------------------------===//

  /// Find instructions tagged by SelectMigrationPoints with instrumentation
  /// metadata.
  void findInstrumentationPoints(Function &F) {
    for(Function::iterator BB = F.begin(), BBE = F.end(); BB != BBE; BB++) {
      for(BasicBlock::iterator I = BB->begin(), IE = BB->end(); I != IE; I++) {
        if(Popcorn::hasEquivalencePointMetadata(I)) MigPointInsts.insert(I);
        if(Popcorn::isHTMBeginPoint(I)) HTMBeginInsts.insert(I);
        if(Popcorn::isHTMEndPoint(I)) HTMEndInsts.insert(I);
      }
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
  void addHTMBeginInternal(Instruction *I, Value *Begin, Value *Comparison) {
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

    // Add check & branch based on HTM begin result Comparison.  The true
    // target of the branch is when we've started the transaction.
    IRBuilder<> HTMWorker(CurBB->getTerminator());
    HTMWorker.CreateCondBr(Comparison, NewSuccBB, FlagCheckBB);
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
    Value *Cmp = FlagCheckWorker.CreateICmpEQ(Flag, NegOne);
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
    Value *Cmp = Worker.CreateICmpNE(HTMBeginVal, Zero);
    addHTMBeginInternal(I, HTMBeginVal, Cmp);
  }

  /// Add a transactional execution begin intrinsice for x86.
  void addX86HTMBegin(Instruction *I) {
    LLVMContext &C = I->getContext();
    IRBuilder<> Worker(I);
    Value *HTMBeginVal = Worker.CreateCall(HTMBeginDecl);
    Value *Success = ConstantInt::get(Type::getInt32Ty(C), 0xffffffff, false);
    Value *Cmp = Worker.CreateICmpEQ(HTMBeginVal, Success);
    addHTMBeginInternal(I, HTMBeginVal, Cmp);
  }

  /// Add transactional execution end intrinsic for PowerPC.
  void addPowerPCHTMEnd(Instruction *I) {
    LLVMContext &C = I->getContext();
    IRBuilder<> EndWorker(I);
    ConstantInt *One = ConstantInt::get(IntegerType::getInt32Ty(C),
                                        1, false);
    EndWorker.CreateCall(HTMEndDecl, ArrayRef<Value *>(One));
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

INITIALIZE_PASS(MigrationPoints, "migration-points",
                "Insert migration points into functions", true, false)

namespace llvm {
  FunctionPass *createMigrationPointsPass()
  { return new MigrationPoints(); }
}

