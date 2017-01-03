#include <map>
#include <set>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/Analysis/LiveValues.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "insert-stackmaps"

using namespace llvm;

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
  virtual const char *getPassName() const { return "InsertStackMaps"; }

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
    std::set<const AllocaInst *> allocas;
  
    DEBUG(errs() << "InsertStackMaps: entering module " << M.getName() << "\n\r");
  
    this->createSMType(M);
    if(this->addSMDeclaration(M)) modified = true;
  
    /* Iterate over all functions/basic blocks/instructions. */
    for(Module::iterator f = M.begin(), fe = M.end(); f != fe; f++)
    {
      if(f->isDeclaration()) continue;
  
      DEBUG(errs() << "InsertStackMaps: entering function "
                   << f->getName() << "\n\r");

      LiveValues &liveVals = getAnalysis<LiveValues>(*f);
      DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*f).getDomTree();
      std::set<const Value *>::const_iterator v, ve;
  
      /*
       * Gather all allocas because the stack transformation runtime must copy
       * over all local data, and hence they should be recorded in the
       * stackmaps.  If we're not careful allocas can slip through the cracks
       * in liveness analysis, e.g.:
       *
       *  %arr = alloca [4 x double], align 8
       *  %arrayidx = getelementptr inbounds [4 x double], [4 x double]* %arr, i64 0, i64 0
       *  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 1, i32 0, %arrayidx)
       *
       * After getting an element pointer, all subsequent accesses to %arr happen
       * through %arrayidx, hence %arr is not caught by liveness analysis and is
       * not copied to the destination stack.
       */
      allocas.clear();
      BasicBlock &entry = f->getEntryBlock();
      for(BasicBlock::iterator i = entry.begin(), ie = entry.end(); i != ie; i++)
      {
        const AllocaInst *inst = dyn_cast<AllocaInst>(&*i);
        if(inst) allocas.insert(inst);
      }
  
      /* Find call sites in the function. */
      for(Function::iterator b = f->begin(), be = f->end(); b != be; b++)
      {
        DEBUG(
          errs() << "InsertStackMaps: entering basic block ";
          b->printAsOperand(errs(), false);
          errs() << "\n\r"
        );
  
        for(BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; i++)
        {
          CallInst *CI;
          if((CI = dyn_cast<CallInst>(&*i)) &&
             !CI->isInlineAsm() &&
             !isa<IntrinsicInst>(CI))
          {
            live = liveVals.getLiveValues(&*i);
            sortedLive.clear();
            for(const Value *val : *live) sortedLive.insert(val);
            for(const AllocaInst *val : allocas)
              if(DT.dominates(val, CI))
                sortedLive.insert(val);
            delete live;
  
            DEBUG(
              const Function *calledFunc;
  
              errs() << "  ";
              CI->printAsOperand(errs(), false);
              errs() << " ";
  
              calledFunc = CI->getCalledFunction();
              if(calledFunc && calledFunc->hasName())
              {
                StringRef name = CI->getCalledFunction()->getName();
                errs() << name << " " << this->callSiteID;
              }
              else errs() << this->callSiteID;
  
              errs() << ", " << sortedLive.size() << " live value(s)\n\r   ";
              for(const Value *val : sortedLive) {
                errs() << " ";
                val->printAsOperand(errs(), false);
              }
              errs() << "\n\r";
            );
  
            IRBuilder<> builder(CI->getNextNode());
            std::vector<Value *> args(2);
            args[0] = ConstantInt::getSigned(Type::getInt64Ty(M.getContext()), this->callSiteID++);
            args[1] = ConstantInt::getSigned(Type::getInt32Ty(M.getContext()), 0);
            for(v = sortedLive.begin(), ve = sortedLive.end(); v != ve; v++)
              args.push_back((Value*)*v);
            builder.CreateCall(this->SMFunc, ArrayRef<Value*>(args));
            this->numInstrumented++;
          }
        }
      }
      this->callSiteID = 0;
    }
  
    DEBUG(
      errs() << "InsertStackMaps: finished module " << M.getName() << ", added "
             << this->numInstrumented << " stackmaps\n\r";
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
    bool operator() (const Value *lhs, const Value *rhs) const
    { return lhs->getName().compare(rhs->getName()) < 0; }
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
      DEBUG(errs() << "Adding stackmap function declaration to " << M.getName() << "\n\r");
      this->SMFunc = cast<Function>(M.getOrInsertFunction(this->SMName, this->SMTy));
      this->SMFunc->setCallingConv(CallingConv::C);
      return true;
    }
    else return false;
  }
};

} /* end anonymous namespace */


char InsertStackMaps::ID = 0;
const StringRef InsertStackMaps::SMName = "llvm.experimental.stackmap";

INITIALIZE_PASS_BEGIN(InsertStackMaps, "insert-stackmaps",
                      "Instrument equivalence points with stack maps ",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(LiveValues)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(InsertStackMaps, "insert-stackmaps",
                    "Instrument equivalence points with stack maps",
                    false, false)

namespace llvm {
  ModulePass *createInsertStackMapsPass() { return new InsertStackMaps(); }
}

