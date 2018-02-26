//===- PopcornCompatibility.cpp -------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Looks for code features which are not currently handled by the Popcorn
// compiler/stack transformation process.  These code features either *might*
// cause issues during stack transformation (and hence the compiler will issue
// a warning), or are guaranteed to not be handled correctly and will cause
// compilation to abort.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"

using namespace llvm;

#define DEBUG_TYPE "popcorn-compat"

namespace {

class PopcornCompatibility : public FunctionPass
{
public:
  static char ID;

  PopcornCompatibility() : FunctionPass(ID) {
    initializePopcornCompatibilityPass(*PassRegistry::getPassRegistry());
  }
  ~PopcornCompatibility() {}

  virtual const char *getPassName() const
  { return "Popcorn compatibility checking"; }

  //===--------------------------------------------------------------------===//
  // Warning & error printing
  //===--------------------------------------------------------------------===//

  /// Emit a warning message for a given location, denoted by an instruction.
  static void warn(const Instruction *I, const std::string &Msg) {
    const Function *F = I->getParent()->getParent();
    std::string Warning("Popcorn compatibility: " + Msg);
    DiagnosticInfoOptimizationFailure DI(*F, I->getDebugLoc(), Warning);
    I->getContext().diagnose(DI);
  }

  /// Emit a warning message for a function.
  static void warn(const Function &F, const std::string &Msg)
  { warn(F.getEntryBlock().begin(), Msg); }

  /// Emit an error message for a given location, denoted by an instruction.
  static void error(const Instruction *I, const std::string &Msg) {
    const Function *F = I->getParent()->getParent();
    std::string Error("Popcorn compatibility: " + Msg);
    DiagnosticInfoOptimizationError DI(*F, I->getDebugLoc(), Error);
    I->getContext().diagnose(DI);
  }

  //===--------------------------------------------------------------------===//
  // Properties of instructions
  //===--------------------------------------------------------------------===//

  /// Return whether the alloca is dynamically-sized.
  static bool isVariableSizedAlloca(const Instruction &I) {
    const AllocaInst *AI;
    if((AI = dyn_cast<AllocaInst>(&I)) && !AI->isStaticAlloca()) return true;
    else return false;
  }

  static bool isInlineAsm(const Instruction &I) {
    if((isa<CallInst>(I) || isa<InvokeInst>(I)) && !isa<IntrinsicInst>(I)) {
      ImmutableCallSite CS(&I);
      if(CS.isInlineAsm()) return true;
    }
    return false;
  }

  //===--------------------------------------------------------------------===//
  // The main show
  //===--------------------------------------------------------------------===//

  virtual bool runOnFunction(Function &F) {
    std::string Msg;

    if(!F.isDeclaration() && !F.isIntrinsic()) {
      if(F.isVarArg()) warn(F, "function takes a variable number of arguments");
      for(auto &BB : F) {
        for(auto &I : BB) {
          if(isVariableSizedAlloca(I)) {
            Msg = "stack variable '";
            Msg += I.getName();
            Msg += "' is dynamically sized (will cause "
                   "issues during code generation)";
            error(&I, Msg);
          }

          if(isInlineAsm(I))
            warn(&I, "inline assembly may have unanalyzable side-effects");

          if(isa<VAArgInst>(I) || isa<VACopyInst>(I) || isa<VAEndInst>(I))
            warn(&I, "va_arg not transformable across architectures");
        }
      }
    }
    return false;
  }

private:
};

} /* end anonymous namespace */

char PopcornCompatibility::ID = 0;

INITIALIZE_PASS(PopcornCompatibility, "popcorn-compat",
                "Analyze code for compatibility issues", false, true)

namespace llvm {
  FunctionPass *createPopcornCompatibilityPass()
  { return new PopcornCompatibility(); }
}

