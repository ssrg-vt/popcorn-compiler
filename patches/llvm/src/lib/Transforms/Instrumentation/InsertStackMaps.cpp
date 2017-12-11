#include <map>
#include <set>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/Analysis/LiveValues.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "insert-stackmaps"

using namespace llvm;

static cl::opt<bool>
NoLiveVals("no-live-vals",
           cl::desc("Don't add live values to inserted stackmaps"),
           cl::init(false),
           cl::Hidden);

namespace {

/**
 * This class instruments equivalence points in the IR with LLVM's stackmap
 * intrinsic.  This tells the backend to record the locations of IR values
 * after register allocation in a separate ELF section.
 */
class InsertStackMaps : public ModulePass
{
public:
  static char ID;
  size_t callSiteID;
  size_t numInstrumented;

  InsertStackMaps() : ModulePass(ID), callSiteID(0), numInstrumented(0) {
    initializeInsertStackMapsPass(*PassRegistry::getPassRegistry());
  }
  ~InsertStackMaps() {}

  /* ModulePass virtual methods */
  virtual const char *getPassName() const { return "Insert stackmaps"; }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const
  {
    AU.addRequired<LiveValues>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.setPreservesCFG();
  }

  /**
   * Use liveness analysis to insert stackmap intrinsics into the IR to record
   * live values at equivalence points.
   *
   * Note: currently we only insert stackmaps at function call sites.
   */
  virtual bool runOnModule(Module &M)
  {
    bool modified = false;
    std::set<const Value *> *live;
    std::set<const Value *, ValueComp> sortedLive;
    std::set<const Instruction *> hiddenInst;
    std::set<const Argument *> hiddenArgs;

    DEBUG(errs() << "\n********** Begin InsertStackMaps **********\n"
                 << "********** Module: " << M.getName() << " **********\n\n");

    this->createSMType(M);
    if(this->addSMDeclaration(M)) modified = true;

    modified |= this->removeOldStackmaps(M);

    /* Iterate over all functions/basic blocks/instructions. */
    for(Module::iterator f = M.begin(), fe = M.end(); f != fe; f++)
    {
      if(f->isDeclaration()) continue;

      DEBUG(errs() << "InsertStackMaps: entering function "
                   << f->getName() << "\n");

      LiveValues &liveVals = getAnalysis<LiveValues>(*f);
      DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*f).getDomTree();
      std::set<const Value *>::const_iterator v, ve;
      getHiddenVals(*f, hiddenInst, hiddenArgs);

      /* Find call sites in the function. */
      for(Function::iterator b = f->begin(), be = f->end(); b != be; b++)
      {
        DEBUG(
          errs() << "InsertStackMaps: entering basic block ";
          b->printAsOperand(errs(), false);
          errs() << "\n"
        );

        for(BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; i++)
        {
          CallInst *CI;
          if((CI = dyn_cast<CallInst>(&*i)) &&
             !CI->isInlineAsm() &&
             !isa<IntrinsicInst>(CI))
          {
            IRBuilder<> builder(CI->getNextNode());
            std::vector<Value *> args(2);
            args[0] = ConstantInt::getSigned(Type::getInt64Ty(M.getContext()), this->callSiteID++);
            args[1] = ConstantInt::getSigned(Type::getInt32Ty(M.getContext()), 0);

            if(NoLiveVals) {
              builder.CreateCall(this->SMFunc, ArrayRef<Value*>(args));
              this->numInstrumented++;
              continue;
            }

            live = liveVals.getLiveValues(&*i);
            for(const Value *val : *live) sortedLive.insert(val);
            for(const Instruction *val : hiddenInst) {
              /*
               * The two criteria for inclusion of a hidden value are:
               *   1. The value's definition dominates the call
               *   2. A use which hides the definition is in the stackmap
               */
              if(DT.dominates(val, CI) && hasLiveUser(val, *live))
                sortedLive.insert(val);
            }
            for(const Argument *val : hiddenArgs) {
              /*
               * Similar criteria apply as above, except we know arguments
               * dominate the entire function.
               */
              if(hasLiveUser(val, *live))
                sortedLive.insert(val);
            }
            delete live;

            /* If the call's value is used, add it to the stackmap */
            if(CI->use_begin() != CI->use_end())
              sortedLive.insert(CI);

            DEBUG(
              const Function *calledFunc;

              errs() << "  ";
              if(!CI->getType()->isVoidTy()) {
                CI->printAsOperand(errs(), false);
                errs() << " ";
              }
              else errs() << "(void) ";

              calledFunc = CI->getCalledFunction();
              if(calledFunc && calledFunc->hasName())
              {
                StringRef name = CI->getCalledFunction()->getName();
                errs() << name << " ";
              }
              errs() << "ID: " << this->callSiteID;

              errs() << ", " << sortedLive.size() << " live value(s)\n   ";
              for(const Value *val : sortedLive) {
                errs() << " ";
                val->printAsOperand(errs(), false);
              }
              errs() << "\n";
            );

            for(v = sortedLive.begin(), ve = sortedLive.end(); v != ve; v++)
              args.push_back((Value*)*v);
            builder.CreateCall(this->SMFunc, ArrayRef<Value*>(args));
            sortedLive.clear();
            this->numInstrumented++;
          }
        }
      }

      hiddenInst.clear();
      hiddenArgs.clear();
      this->callSiteID = 0;
    }

    DEBUG(
      errs() << "InsertStackMaps: finished module " << M.getName() << ", added "
             << this->numInstrumented << " stackmaps\n\n";
    );

    if(numInstrumented > 0) modified = true;

    return modified;
  }

private:
  /* Name of stack map intrinsic */
  static const StringRef SMName;

  /* Stack map instruction creation */
  Function *SMFunc;
  FunctionType *SMTy; // Used for creating function declaration

  /* Sort values based on name */
  struct ValueComp {
    bool operator() (const Value *a, const Value *b) const
    {
      if(a->hasName() && b->hasName())
        return a->getName().compare(b->getName()) < 0;
      else if(a->hasName()) return true;
      else if(b->hasName()) return false;
      else return a < b;
    }
  };

  /**
   * Create the function type for the stack map intrinsic.
   */
  void createSMType(const Module &M)
  {
    std::vector<Type*> params(2);
    params[0] = Type::getInt64Ty(M.getContext());
    params[1] = Type::getInt32Ty(M.getContext());
    this->SMTy = FunctionType::get(Type::getVoidTy(M.getContext()),
                                                   ArrayRef<Type*>(params),
                                                   true);
  }

  /**
   * Add the stackmap intrinisic's function declaration if not already present.
   * Return true if the declaration was added, or false if it's already there.
   */
  bool addSMDeclaration(Module &M)
  {
    if(!(this->SMFunc = M.getFunction(this->SMName)))
    {
      DEBUG(errs() << "Adding stackmap function declaration to " << M.getName() << "\n");
      this->SMFunc = cast<Function>(M.getOrInsertFunction(this->SMName, this->SMTy));
      this->SMFunc->setCallingConv(CallingConv::C);
      return true;
    }
    else return false;
  }

  /**
   * Iterate over all instructions, removing previously found stackmaps.
   */
  bool removeOldStackmaps(Module &M)
  {
    bool modified = false;
    CallInst* CI;
    const Function *F;

    DEBUG(dbgs() << "Searching for/removing old stackmaps\n";);

    for(Module::iterator f = M.begin(), fe = M.end(); f != fe; f++) {
      for(Function::iterator bb = f->begin(), bbe = f->end(); bb != bbe; bb++) {
        for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; i++) {
          if((CI = dyn_cast<CallInst>(&*i))) {
            F = CI->getCalledFunction();
            if(F && F->hasName() && F->getName() == SMName) {
              i = i->eraseFromParent()->getPrevNode();
              modified = true;
            }
          }
        }
      }
    }

    DEBUG(if(modified)
            dbgs() << "WARNING: found previous run of Popcorn passes!\n";);

    return modified;
  }

  /**
   * Gather a list of values which may be "hidden" from live value analysis.
   * This function collects the values used in these instructions, which are
   * later added to the appropriate stackmaps.
   *
   * 1. Instructions which access fields of structs or entries of arrays, like
   *    getelementptr, can interfere with the live value analysis to hide the
   *    backing values used in the instruction.  For example, the following IR
   *    obscures %arr from the live value analysis:
   *
   *  %arr = alloca [4 x double], align 8
   *  %arrayidx = getelementptr inbounds [4 x double], [4 x double]* %arr, i64 0, i64 0
   *
   *  -> Access to %arr might only happen through %arrayidx, and %arr may not
   *     be used any more
   *
   * 2. Compare instructions, such as icmp & fcmp, can be lowered to complex &
   *    architecture-specific  machine code by the backend.  To help capture
   *    all live values, we capture both the value used in the comparison and
   *    the resulting condition value.
   *
   */
  void getHiddenVals(Function &F,
                     std::set<const Instruction *> &inst,
                     std::set<const Argument *> &args)
  {
    /* Does the instruction potentially hide values from liveness analysis? */
    auto hidesValues = [](const Instruction *I) {
      if(isa<ExtractElementInst>(I) || isa<InsertElementInst>(I) ||
         isa<ExtractValueInst>(I) || isa<InsertValueInst>(I) ||
         isa<GetElementPtrInst>(I) || isa<ICmpInst>(I) || isa<FCmpInst>(I))
        return true ;
      else return false;
    };

    /* Search for instructions that obscure live values & record operands */
    for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
      if(hidesValues(&*i)) {
        for(unsigned op = 0; op < i->getNumOperands(); op++) {
          if(isa<Instruction>(i->getOperand(op)))
            inst.insert(cast<Instruction>(i->getOperand(op)));
          else if(isa<Argument>(i->getOperand(op)))
            args.insert(cast<Argument>(i->getOperand(op)));
        }
      }
    }
  }

  /**
   * Return whether or not a value's user is in a liveness set.
   *
   * @param Val a value whose users are checked against the liveness set
   * @param Live a set of live values
   * @return true if a user is in the liveness set, false otherwise
   */
  bool hasLiveUser(const Value *Val,
                   const std::set<const Value *> &Live) const {
    Value::const_use_iterator use, e;
    for(use = Val->use_begin(), e = Val->use_end(); use != e; use++)
      if(Live.count(use->getUser())) return true;
    return false;
  }
};

} /* end anonymous namespace */

char InsertStackMaps::ID = 0;
const StringRef InsertStackMaps::SMName = "llvm.experimental.stackmap";

INITIALIZE_PASS_BEGIN(InsertStackMaps, "insert-stackmaps",
                      "Instrument equivalence points with stack maps",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(LiveValues)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(InsertStackMaps, "insert-stackmaps",
                    "Instrument equivalence points with stack maps",
                    false, false)

namespace llvm {
  ModulePass *createInsertStackMapsPass() { return new InsertStackMaps(); }
}

