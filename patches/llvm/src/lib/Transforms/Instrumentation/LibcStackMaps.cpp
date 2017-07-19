#include <map>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "libc-stackmaps"

using namespace llvm;

namespace {

/**
 * Instrument thread starting points with stackmaps.  These are the only
 * functions inside of libc for which we want to generate metadata, since we
 * disallow migration inside the public libc API.
 */
// TODO: only implemented for musl-libc!
class LibcStackMaps : public ModulePass
{
public:
  static char ID;
  size_t numInstrumented;

  LibcStackMaps() : ModulePass(ID), numInstrumented(0) {
    initializeLibcStackMapsPass(*PassRegistry::getPassRegistry());
  }
  ~LibcStackMaps() {}

  /* ModulePass virtual methods */
  virtual const char *getPassName() const
  { return "Insert stackmaps in libc thread start functions"; }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const
  { AU.setPreservesCFG(); }

  virtual bool runOnModule(Module &M)
  {
    int64_t smid;
    bool modified = false;
    Function *F;
    std::map<std::string, std::vector<std::string> >::const_iterator file;

    /* Is this a module (i.e., source file) we're interested in? */
    if((file = funcs.find(sys::path::stem(M.getName()))) != funcs.end())
    {
      DEBUG(dbgs() << "\n********** Begin LibcStackMaps **********\n"
                   << "********** Module: " << file->first << " **********\n\n");

      this->createSMType(M);
      modified |= this->addSMDeclaration(M);

      /* Iterate over thread starting functions in the module */
      for(size_t f = 0, fe = file->second.size(); f < fe; f++)
      {
        DEBUG(dbgs() << "LibcStackMaps: entering thread starting function "
                     << file->second[f] << "\n");

        F = M.getFunction(file->second[f]);
        assert(F && !F->isDeclaration() && "No thread function definition");
        modified |= this->removeOldStackmaps(F);
        assert(smids.find(file->second[f]) != smids.end() && "No ID for function");
        smid = smids.find(file->second[f])->second;

        /*
         * Look for & instrument a generic call instruction followed by a call
         * to an exit function, e.g.,
         *
         *   %call = call i32 %main(...)
         *   call void @exit(i32 %call)
         */
        for(Function::iterator bb = F->begin(), be = F->end(); bb != be; bb++)
        {
          bool track = false;
          for(BasicBlock::reverse_iterator i = bb->rbegin(), ie = bb->rend();
              i != ie; i++)
          {
            if(isExitCall(*i)) track = true;
            else if(track && isa<CallInst>(*i))
            {
              IRBuilder<> builder(i->getNextNode());
              std::vector<Value *> args(2);
              args[0] = ConstantInt::getSigned(Type::getInt64Ty(M.getContext()), smid);
              args[1] = ConstantInt::getSigned(Type::getInt32Ty(M.getContext()), 0);
              builder.CreateCall(this->SMFunc, ArrayRef<Value*>(args));
              this->numInstrumented++;
              break;
            }
          }
        }
      }

      DEBUG(dbgs() << "LibcStackMaps: finished module " << M.getName()
                   << ", added " << this->numInstrumented << " stackmaps\n\n";);
    }

    if(numInstrumented > 0) modified = true;
    return modified;
  }

private:
  /* Name of stack map intrinsic */
  static const StringRef SMName;

  /* Stack map instruction creation */
  Function *SMFunc;
  FunctionType *SMTy; // Used for creating function declaration

  /* Files, functions & IDs */
  static const std::map<std::string, std::vector<std::string> > funcs;
  static const std::map<std::string, int64_t> smids;
  static const std::vector<std::string> exitFuncs;

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
      DEBUG(dbgs() << "Adding stackmap function declaration to " << M.getName() << "\n");
      this->SMFunc = cast<Function>(M.getOrInsertFunction(this->SMName, this->SMTy));
      this->SMFunc->setCallingConv(CallingConv::C);
      return true;
    }
    else return false;
  }

  /**
   * Iterate over all instructions, removing previously found stackmaps.
   */
  bool removeOldStackmaps(Function *F)
  {
    bool modified = false;
    CallInst* CI;
    const Function *CurF;

    DEBUG(dbgs() << "Searching for/removing old stackmaps\n";);

    for(Function::iterator bb = F->begin(), bbe = F->end(); bb != bbe; bb++) {
      for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; i++) {
        if((CI = dyn_cast<CallInst>(&*i))) {
          CurF = CI->getCalledFunction();
          if(CurF && CurF->hasName() && CurF->getName() == SMName) {
            i = i->eraseFromParent()->getPrevNode();
            modified = true;
          }
        }
      }
    }

    DEBUG(if(modified) dbgs() << "WARNING: found previous stackmaps!\n";);
    return modified;
  }

  /**
   * Return whether or not the instruction is a call to an exit function.
   */
  bool isExitCall(Instruction &I)
  {
    CallInst *CI;
    Function *F;

    if((CI = dyn_cast<CallInst>(&I)))
    {
      F = CI->getCalledFunction();
      if(F && F->hasName())
        for(size_t i = 0, e = exitFuncs.size(); i < e; i++)
          if(F->getName() == exitFuncs[i]) return true;
    }

    return false;
  }
};

} /* end anonymous namespace */

char LibcStackMaps::ID = 0;
const StringRef LibcStackMaps::SMName = "llvm.experimental.stackmap";

/**
 * Map a source code filename (minus the extension) to the names of functions
 * inside which are to be instrumented.
 */
const std::map<std::string, std::vector<std::string> > LibcStackMaps::funcs = {
  {"__libc_start_main", {"__libc_start_main"}},
  {"pthread_create", {"start", "start_c11"}}
};

/* Map a function name to the stackmap ID representing that function. */
const std::map<std::string, int64_t> LibcStackMaps::smids = {
  {"__libc_start_main", UINT64_MAX},
  {"start", UINT64_MAX - 1},
  {"start_c11", UINT64_MAX - 2}
};

/**
 * Thread exit function names, used to search for starting function call site
 * to be instrumented with stackmap.
 */
const std::vector<std::string> LibcStackMaps::exitFuncs = {
  "exit", "pthread_exit", "__pthread_exit"
};

INITIALIZE_PASS(LibcStackMaps, "libc-stackmaps",
  "Instrument libc thread start functions with stack maps", false, false)

namespace llvm {
  ModulePass *createLibcStackMapsPass() { return new LibcStackMaps(); }
}

