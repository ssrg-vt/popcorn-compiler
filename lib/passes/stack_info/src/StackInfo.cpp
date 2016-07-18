#include <map>
#include <set>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "LiveValues.h"
#include "StackInfo.h"

#define DEBUG_TYPE "stack-info"

using namespace llvm;

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/**
 * Default constructor.
 */
StackInfo::StackInfo()
  : ModulePass(ID), callSiteID(0), numInstrumented(0),
    SMName("llvm.experimental.stackmap") {}

/**
 * Default destructor.
 */
StackInfo::~StackInfo() {}

void StackInfo::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<LiveValues>();
  AU.setPreservesCFG();
}

/**
 * Find and instrument each call site with the "llvm.stackmap" intrinsic to
 * record the locations of live variables and to correlate call sites across
 * binaries.
 *
 * @param M a module to search for call sites
 * @return true if the basic block was modified
 */
bool StackInfo::runOnModule(Module &M)
{
  bool modified = false;
  std::set<const Value *> *live;
  std::set<const Value *, ValueComp> sortedLive;
  Triple triple(M.getTargetTriple());
  // TODO default to x86, need to get arch via triple.getArch()
  size_t maxLive = MaxLive[llvm::Triple::ArchType::x86_64];
  //size_t maxLive = MaxLive[triple.getArch()];

  DEBUG(errs() << "StackInfo: entering module " << M.getName() << "\n\r");

  this->createSMType(M);
  if(this->addSMDeclaration(M)) modified = true;

  /* Iterate over all functions/basic blocks/instructions. */
  for(Module::iterator f = M.begin(), fe = M.end(); f != fe; f++)
  {
    if(f->isDeclaration()) continue;

    DEBUG(errs() << "StackInfo: entering function " << f->getName() << "\n\r");

    LiveValues &liveVals = getAnalysis<LiveValues>(*f);
    std::set<const Value *>::const_iterator v, ve;
    size_t numRecords;

    /*
     * Put a stackmap at the beginning of the function to capture arguments. This
     * should *always* have an ID of 0.
     */
    live = liveVals.getLiveIn(&f->getEntryBlock());
    sortedLive.clear();
    for(const Value *val : *live) sortedLive.insert(val);
    delete live;

    std::vector<Value *> funcArgs(2);
    funcArgs[0] = ConstantInt::getSigned(Type::getInt64Ty(M.getContext()), this->callSiteID++);
    funcArgs[1] = ConstantInt::getSigned(Type::getInt32Ty(M.getContext()), 0);
    for(v = sortedLive.begin(), ve = sortedLive.end(), numRecords = 0;
        v != ve && numRecords < maxLive;
        v++, numRecords++)
      funcArgs.push_back((Value *)*v);
    IRBuilder<> funcArgBuilder(&*f->getEntryBlock().getFirstInsertionPt());
    funcArgBuilder.CreateCall(this->SMFunc, ArrayRef<Value *>(funcArgs));
    this->numInstrumented++;

    if(numRecords == maxLive)
      errs() << "WARNING: reached maximum number of records for stackmap ("
             << triple.getArchName() << ": " << maxLive << ")\n\r";


    /* Find call sites in the function. */
    for(Function::iterator b = f->begin(), be = f->end(); b != be; b++)
    {
      DEBUG(
        errs() << "StackInfo: entering basic block ";
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
          for(v = sortedLive.begin(), ve = sortedLive.end(), numRecords = 0;
              v != ve && numRecords < maxLive;
              v++, numRecords++)
            args.push_back((Value*)*v);
          builder.CreateCall(this->SMFunc, ArrayRef<Value*>(args));
          this->numInstrumented++;

          if(numRecords == maxLive)
            errs() << "WARNING: in " << f->getName()
                   << ", reached maximum number of records for stackmap "
                   << (this->callSiteID - 1) << " ("
                   << triple.getArchName() << ": " << maxLive << ")\n\r";
        }
      }
    }
    this->callSiteID = 0;
  }

  DEBUG(
    errs() << "StackInfo: finished module " << M.getName() << ", added "
           << this->numInstrumented << " stackmaps\n\r";
  );

  if(numInstrumented > 0) modified = true;

  return modified;
}

/* Define pass ID & register pass w/ driver. */
char StackInfo::ID = 0;
static RegisterPass<StackInfo> RPStackInfo(
  "stack-info",
  "Record live variable locations & tag call sites for stack transformation",
  false,
  false
);

///////////////////////////////////////////////////////////////////////////////
// Private API
///////////////////////////////////////////////////////////////////////////////

/**
 * Create the function type for the stack map intrinsic.
 *
 * @param M the module passed to runOnModule() (provides an LLVMContext)
 */
void StackInfo::createSMType(const Module &M)
{
  std::vector<Type*> params(2);
  params[0] = Type::getInt64Ty(M.getContext());
  params[1] = Type::getInt32Ty(M.getContext());
  this->SMTy = FunctionType::get(Type::getVoidTy(M.getContext()),
                                                 ArrayRef<Type*>(params),
                                                 true);
}

/**
 * Add the llvm.experimental.stackmap function declaration (if not already
 * present).
 *
 * @param M compilation module for which to add a declaration
 * @return true if the declaration was added, false otherwise
 */
bool StackInfo::addSMDeclaration(Module &M)
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

