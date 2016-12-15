#include <ctime>
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "name-string-literals"

using namespace llvm;

namespace
{

/**
 * This pass searches for anonymous read-only data for which there is no symbol
 * and generates a symbol for the data.  This is required by the Popcorn
 * compiler in order to align the data at link-time.
 */
class NameStringLiterals : public ModulePass
{
public:
	static char ID;

  NameStringLiterals() : ModulePass(ID) {}
  ~NameStringLiterals() {}

	/* ModulePass virtual methods */
  virtual void getAnalysisUsage(AnalysisUsage &AU) const { AU.setPreservesCFG(); }
	virtual bool runOnModule(Module &M)
  {
    bool modified = false;
    std::string root, newName;
    Module::iterator it, ite;
    Module::global_iterator gl, gle; //for global variables

    DEBUG(errs() << "NameStringLiterals: entering module " << M.getName() << "\n");

    // Iterate over all globals and generate symbol for anonymous string
    // literals in each module
    for(gl = M.global_begin(), gle = M.global_end(); gl != gle; gl++) {
      // DONT NEED TO CHANGE NAME PER-SE just change type
      // PrivateLinkage does NOT show up in any symbol table in the object file!
      if(gl->getLinkage() == GlobalValue::PrivateLinkage) {
        //change Linkage
        //FROM private unnamed_addr constant [num x i8]
        //TO global [num x i8]
        gl->setLinkage(GlobalValue::ExternalLinkage);

        // Make the global's name unique so we don't clash when linking with
        // other files
        std::string::size_type minusPath = M.getName().find_last_of('/');
        root = M.getName().substr(minusPath + 1);
        std::string::size_type pos = root.find_first_of('.');
        root = root.substr(0,pos);
        root += "_" + std::to_string(getTimestamp());
        newName = root + "_" + gl->getName().str();
        gl->setName(newName);

        // Also REMOVE unnamed_addr value
        if(gl->hasUnnamedAddr()) {
          gl->setUnnamedAddr(false);
        }

        modified = true;

        DEBUG(errs() << "New anonymous string name: " << newName << "\n";);
      } else {
        DEBUG(errs() << "> " <<  *gl << "\nLinkage: "
                     << gl->getLinkage() << "\n");
      }
    }
  
    return modified;
  }

private:
  unsigned long long getTimestamp() 
  {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000000000UL) | time.tv_nsec;
  }
};

} /* end anonymous namespace */

char NameStringLiterals::ID = 0;
INITIALIZE_PASS(NameStringLiterals, "name-string-literals",
  "Generate symbols for anonymous string literals", false, false)

namespace llvm {
  ModulePass *createNameStringLiteralsPass() { return new NameStringLiterals(); }
}

