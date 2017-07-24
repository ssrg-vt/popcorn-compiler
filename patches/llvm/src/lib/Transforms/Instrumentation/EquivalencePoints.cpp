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

#include "llvm/Pass.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/InlineAsm.h"
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

namespace {

/// EquivalencePoints - insert equivalence points into functions, optionally
/// adding HTM execution.
class EquivalencePoints : public FunctionPass
{
public:
  static char ID;

  EquivalencePoints() : FunctionPass(ID), numInstrumented(0) {}
  ~EquivalencePoints() {}

  virtual const char *getPassName() const
  { return "Insert equivalence points"; }

  virtual bool doInitialization(Module &M) {
    // Make sure HTM is supported on this architecture if attempting to
    // instrument with transactional execution
    doHTMInstrumentation = false;
    if(HTMExec) {
      Triple Arch(M.getTargetTriple());
      if(HTMEqPoint.find(Arch.getArchName()) == HTMEqPoint.end()) {
        std::string Msg("HTM instrumentation not supported for '");
        Msg += Arch.getArchName();
        Msg += "'";
        DiagnosticInfoInlineAsm DI(Msg, DiagnosticSeverity::DS_Warning);
        M.getContext().diagnose(DI);
      }
      else doHTMInstrumentation = true;
    }
    return false;
  }

  /// Insert equivalence points into functions
  virtual bool runOnFunction(Function &F)
  {
    numInstrumented = 0;

    // Instrument function boundaries, i.e., entry and return points
    addEquivalencePoint(F.getEntryBlock().getFirstInsertionPt());
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++) {
      if(isa<ReturnInst>(BB->getTerminator())) {
        addEquivalencePoint(BB->getTerminator());
      }
    }

    // Some libc functions (e.g., I/O) will cause aborts from system calls.
    // Instrument libc calls to stop & resume transactions afterwards.
    if(doHTMInstrumentation) wrapLibcWithHTM(F);

    return numInstrumented > 0;
  }

private:
  /// Number of equivalence points added to the application
  size_t numInstrumented;

  /// Rather than modifying the command-line argument (which can mess up
  /// compile configurations for multi-ISA binary generation), store a
  /// per-module value during intialization
  bool doHTMInstrumentation;

  /// HTM inline assembly for a given architecture
  struct AsmSpec {
    /// Assembly template, i.e., assembler instructions.
    std::string Template;

    /// Constraints (inputs, outputs, clobbers) for the assembly template
    std::string Constraints;

    /// Do we have side-effects?
    bool SideEffects;

    /// Do we need to align the stack?
    bool AlignsStack;

    /// Assembly dialect (LLVM only supports AT&T or Intel)
    InlineAsm::AsmDialect Dialect;
  };

  /// Per-architecture inline assembly for HTM execution
  const static StringMap<AsmSpec> HTMBegin;
  const static StringMap<AsmSpec> HTMEnd;
  const static StringMap<AsmSpec> HTMEqPoint;

  /// libc functions which are likely to cause an HTM abort through a syscall
  // TODO LLVM has to have a better way to detect these
  const static StringSet<> LibcIO;

  /// Get an architecture-specific inline ASM statement for transactional
  /// execution at equivalence points.  The template argument specifies which
  /// assembly to generate, e.g., HTM begin/end, or HTM equivalence points.
  template<const StringMap<AsmSpec> &Asm>
  static InlineAsm *getHTMAsm(const Module &M) {
    StringMap<AsmSpec>::const_iterator It;
    Triple Arch(M.getTargetTriple());

    FunctionType *FuncTy =
      FunctionType::get(Type::getVoidTy(M.getContext()), false);
    It = Asm.find(Arch.getArchName());
    assert(It != Asm.end() && "Unsupported architecture");
    const AsmSpec &Spec = It->second;
    return InlineAsm::get(FuncTy,
                          Spec.Template,
                          Spec.Constraints,
                          Spec.SideEffects,
                          Spec.AlignsStack,
                          Spec.Dialect);
  }

  /// Insert an equivalence point directly before the specified instruction
  void addEquivalencePoint(Instruction *I) {
    IRBuilder<> Worker(I);

    if(doHTMInstrumentation)
      Worker.CreateCall(getHTMAsm<HTMEqPoint>(*I->getModule()));
    // TODO insert flag check & migration call if flag is set

    numInstrumented++;
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

  /// Search for & wrap libc functions which are likely to can an HTM abort
  void wrapLibcWithHTM(Function &F) {
    Instruction *Start, *End;

    for(Function::iterator BB = F.begin(), BE = F.end(); BB != BE; BB++) {
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; I++) {
        if(isLibcIO(&*I)) {
          // Wrap multiple consecutive I/O calls together
          Start = End = &*I;
          while(isLibcIO(End->getNextNode())) {
            End = End->getNextNode();
            I++;
          }

          IRBuilder<> Worker(Start);
          Worker.CreateCall(getHTMAsm<HTMEnd>(*Start->getModule()));
          Worker.SetInsertPoint(End->getNextNode());
          Worker.CreateCall(getHTMAsm<HTMBegin>(*Start->getModule()));
        }
      }
    }
  }
};

} /* end anonymous namespace */

char EquivalencePoints::ID = 0;

// TODO PowerPC assembly
const StringMap<EquivalencePoints::AsmSpec> EquivalencePoints::HTMBegin = {
  {"x86_64", {"xbegin 1f;1:", "~{eax},~{dirflag},~{fpsr},~{flags}",
              true, false, InlineAsm::AD_ATT}}
};

const StringMap<EquivalencePoints::AsmSpec> EquivalencePoints::HTMEnd = {
  {"x86_64", {"xtest;jz 1f;xend;1:", "~{dirflag},~{fpsr},~{flags}",
              true, false, InlineAsm::AD_ATT}}
};

const StringMap<EquivalencePoints::AsmSpec> EquivalencePoints::HTMEqPoint = {
  {"x86_64", {"xtest;jz 1f;xend;1:xbegin 2f;2:",
              "~{eax},~{dirflag},~{fpsr},~{flags}",
              true, false, InlineAsm::AD_ATT}}
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

