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
    if(HTMExec) {
      Triple Arch(M.getTargetTriple());
      if(HTMAsm.find(Arch.getArchName()) == HTMAsm.end()) {
        std::string Msg("HTM instrumentation not supported for '");
        Msg += Arch.getArchName();
        Msg += "'";
        DiagnosticInfoInlineAsm DI(Msg, DiagnosticSeverity::DS_Warning);
        M.getContext().diagnose(DI);
        doHTMAsmInstrumentation = false;
      }
      else doHTMAsmInstrumentation = true;
    }

    return false;
  }

  /// Insert equivalence points into functions
  virtual bool runOnFunction(Function &F)
  {
    numInstrumented = 0;

    // Instrument function boundaries, i.e., entry and return points
    addEquivalencePoint(*F.getEntryBlock().getFirstInsertionPt());
    for(Function::iterator BB = F.begin(), E = F.end(); BB != E; BB++) {
      if(isa<ReturnInst>(BB->getTerminator())) {
        addEquivalencePoint(*BB->getTerminator());
      }
    }

    return numInstrumented > 0;
  }

private:
  /// Number of equivalence points added to the application
  size_t numInstrumented;

  /// Rather than modifying the command-line argument (which can mess up
  /// compile configurations for multi-ISA binary generation), store a
  /// per-module value during intialization
  bool doHTMAsmInstrumentation;

  /// HTM inline assembly for a given architecture
  struct HTMAsmSpec {
    /// Assembly template (i.e., assembler instructions) for HTM stop/start.
    /// Implements the following pseudo-code:
    ///
    ///   if(in_transaction):
    ///     stop_transaction
    ///   start_transaction
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
  const static StringMap<HTMAsmSpec> HTMAsm;

  /// Get an architecture-specific inline ASM statement for transactional
  /// execution at equivalence points
  static InlineAsm *getHTMAsm(const Module &M) {
    StringMap<HTMAsmSpec>::const_iterator It;
    Triple Arch(M.getTargetTriple());

    FunctionType *FuncTy =
      FunctionType::get(Type::getVoidTy(M.getContext()), false);
    It = HTMAsm.find(Arch.getArchName());
    assert(It != HTMAsm.end() && "Unsupported architecture");
    const HTMAsmSpec &Spec = It->second;
    return InlineAsm::get(FuncTy,
                          Spec.Template,
                          Spec.Constraints,
                          Spec.SideEffects,
                          Spec.AlignsStack,
                          Spec.Dialect);
  }

  /// Insert an equivalence point directly before the specified instruction
  void addEquivalencePoint(Instruction &I) {
    IRBuilder<> Worker(&I);

    if(doHTMAsmInstrumentation) Worker.CreateCall(getHTMAsm(*I.getModule()));
    // TODO insert flag check & migration call if flag is set

    numInstrumented++;
  }
};

} /* end anonymous namespace */

char EquivalencePoints::ID = 0;

const StringMap<EquivalencePoints::HTMAsmSpec> EquivalencePoints::HTMAsm = {
  {"x86_64", {"xtest;jz 1f;xend;1:xbegin 2f;2:",
              "~{dirflag},~{fpsr},~{flags}",
              true, false, InlineAsm::AD_ATT}}
  // TODO PowerPC assembly
};

INITIALIZE_PASS(EquivalencePoints, "equivalence-points",
                "Insert equivalence points into functions",
                true, false)

namespace llvm {
  FunctionPass *createEquivalencePointsPass()
  { return new EquivalencePoints(); }
}

