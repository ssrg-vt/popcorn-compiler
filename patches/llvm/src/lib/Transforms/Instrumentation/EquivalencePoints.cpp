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
// end of a function.
//
// TODO more advanced analysis to insert additional equivalence points
//
//===----------------------------------------------------------------------===//

#include <map>
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "equivalence-points"

/// Cover the application in transactional execution by inserting HTM
/// stop/start instructions at equivalence points.
static cl::opt<bool>
HTMExec("htm-execution", cl::NotHidden, cl::init(false),
  cl::desc("Instrument equivalence points with HTM execution "
           "(only supported on PowerPC & x86-64)"));

/// Disable wrapping libc functions which are likely to cause HTM aborts with
/// HTM stop/start intrinsics.  Wrapping happens by default with HTM execution.
static cl::opt<bool>
NoWrapLibc("htm-no-wrap-libc", cl::Hidden, cl::init(false),
  cl::desc("Disable wrapping libc functions with HTM stop/start"));

/// Insert more equivalence points into the body of a function.  Analyze memory
/// usage & attempt to instrument the code to reduce the time until the thread
/// reaches an equivalence point.  Analysis is tailored to avoid hardware
/// transactional memory (HTM) capacity aborts.
static cl::opt<bool>
MoreEqPoints("more-eq-points", cl::Hidden, cl::init(false),
  cl::desc("Add additional equivalence points into the body of functions "
           "(implies '-eq-points')"));

/// HTM memory read buffer size for tuning analysis when inserting additional
/// equivalence points.
static cl::opt<unsigned>
HTMReadBufSize("htm-buf-read", cl::Hidden, cl::init(8),
  cl::desc("HTM analysis tuning - HTM read buffer size, in kilobytes"),
  cl::value_desc("size"));

/// HTM memory write buffer size for tuning analysis when inserting additional
/// equivalence points.
static cl::opt<unsigned>
HTMWriteBufSize("htm-buf-write", cl::Hidden, cl::init(8),
  cl::desc("HTM analysis tuning - HTM write buffer size, in kilobytes"),
  cl::value_desc("size"));

STATISTIC(NumEqPoints, "Number of equivalence points added");

namespace {

/// EquivalencePoints - insert equivalence points into functions, optionally
/// adding HTM execution.
class EquivalencePoints : public FunctionPass
{
public:
  static char ID;

  EquivalencePoints() : FunctionPass(ID), NumInstr(0) {}
  ~EquivalencePoints() {}

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

      // TODO need to check for HTM attributes, e.g., on Intel "+rtm"

      if(HTMBegin.find(Arch) != HTMBegin.end()) {
        // Add intrinsic declarations, used to create call instructions
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
    NumInstr = 0;
    SmallVector<Instruction *, 16> Returns;

    // Instrument function boundaries, i.e., entry and return points.  Collect
    // returns first & then instrument, otherwise we can inadvertently create
    // more return instructions & infinitely loop.
    addEquivalencePoint(F.getEntryBlock().getFirstInsertionPt());
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++)
      if(isa<ReturnInst>(BB->getTerminator()))
        Returns.push_back(BB->getTerminator());
    for(auto &I : Returns) addEquivalencePoint(I);

    // Some libc functions (e.g., I/O) will cause aborts from system calls.
    // Instrument libc calls to stop & resume transactions afterwards.
    if(DoHTMInstrumentation && !NoWrapLibc) wrapLibcWithHTM(F);

    NumEqPoints += NumInstr;
    return NumInstr > 0;
  }

private:
  /// Number of equivalence points added to the application
  size_t NumInstr;

  /// Rather than modifying the command-line argument (which can mess up
  /// compile configurations for multi-ISA binary generation), store a
  /// per-module value during intialization
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

  /// Add a declaration for an architecture-specific intrinsic (contained in
  /// the map).
  Constant *addIntrinsicDecl(Module &M, const IntrinsicMap &Map) {
    IntrinsicMap::const_iterator It = Map.find(Arch);
    assert(It != Map.end() && "Unsupported architecture");
    FunctionType *FuncTy = Intrinsic::getType(M.getContext(), It->second);
    return M.getOrInsertFunction(Intrinsic::getName(It->second), FuncTy);
  }

  /// Add a transactional execution begin intrinsic
  void addHTMBegin(Instruction *I) {
    IRBuilder<> Worker(I);
    Worker.CreateCall(HTMBeginDecl);
  }

  /// Add transactional execution check & end intrinsics before an instruction.
  void addHTMCheckAndEnd(Instruction *I) {
    LLVMContext &C = I->getContext();
    BasicBlock *CurBB = I->getParent(), *NewSuccBB, *HTMEndBB;
    Function *CurF = CurBB->getParent();

    // Create a new successor which contains all instructions after the HTM
    // check & end
    NewSuccBB =
      CurBB->splitBasicBlock(I, ".htmendsucc" + std::to_string(NumInstr));

    // Create an HTM end block, which ends the transaction and jumps to the
    // new successor
    HTMEndBB = BasicBlock::Create(C, ".htmend" + std::to_string(NumInstr),
                                  CurF, NewSuccBB);
    IRBuilder<> EndWorker(HTMEndBB);
    EndWorker.CreateCall(HTMEndDecl);
    EndWorker.CreateBr(NewSuccBB);

    // Finally, add the HTM test & replace the unconditional branch created by
    // splitBasicBlock() with a conditional branch to end the transaction or
    // continue on to the new successor
    IRBuilder<> PredWorker(CurBB->getTerminator());
    CallInst *HTMTestVal = PredWorker.CreateCall(HTMTestDecl);
    IntegerType *I32 = IntegerType::getInt32Ty(C);
    ConstantInt *Zero = ConstantInt::get(I32, 0, true);
    Value *Cmp = PredWorker.CreateICmpNE(HTMTestVal, Zero,
                                         "htmcmp" + std::to_string(NumInstr));
    PredWorker.CreateCondBr(Cmp, HTMEndBB, NewSuccBB);
    CurBB->getTerminator()->eraseFromParent();
  }

  /// Insert an equivalence point directly before an instruction
  void addEquivalencePoint(Instruction *I) {

    if(DoHTMInstrumentation) {
      addHTMCheckAndEnd(I);
      addHTMBegin(I);
    }
    // TODO insert flag check & migration call if flag is set

    NumInstr++;
  }

  /// Return whether the call instruction is a libc I/O call
  static inline bool isLibcIO(const Instruction *I) {
    const CallInst *CI;

    // Is it a call instruction?
    if(!I || !isa<CallInst>(I)) return false;
    CI = cast<CallInst>(I);

    // Is the called function in the set of libc I/O functions?
    const Function *CalledFunc = CI->getCalledFunction();
    if(CalledFunc && CalledFunc->hasName()) {
      const StringRef Name = CalledFunc->getName();
      return LibcIO.find(Name) != LibcIO.end();
    }
    return false;
  }

  /// Search for & wrap libc functions which are likely to cause an HTM abort
  void wrapLibcWithHTM(Function &F) {
    SmallVector<Instruction *, 16> LibcCalls;

    // Add libc call instructions to the work list & then instrument (same
    // reasoning as for instrumenting function returns)
    for(Function::iterator BB = F.begin(), BE = F.end(); BB != BE; BB++)
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++)
        if(isLibcIO(&*I)) LibcCalls.push_back(&*I);

    // Add HTM check/end control flow before and HTM begins after calls
    for(auto &Inst : LibcCalls) {
      addHTMCheckAndEnd(Inst);
      addHTMBegin(Inst->getNextNode());
    }
  }
};

} /* end anonymous namespace */

char EquivalencePoints::ID = 0;

// TODO LLVM has intrinsics for x86 & PPC HTM inline assembly
const std::map<Triple::ArchType, Intrinsic::ID> EquivalencePoints::HTMBegin = {
  {Triple::x86_64, Intrinsic::x86_xbegin},
  {Triple::ppc64le, Intrinsic::ppc_tbegin}
};

const std::map<Triple::ArchType, Intrinsic::ID> EquivalencePoints::HTMEnd = {
  {Triple::x86_64, Intrinsic::x86_xend}
};

const std::map<Triple::ArchType, Intrinsic::ID> EquivalencePoints::HTMTest = {
  {Triple::x86_64, Intrinsic::x86_xtest}
};

INITIALIZE_PASS(EquivalencePoints, "equivalence-points",
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

