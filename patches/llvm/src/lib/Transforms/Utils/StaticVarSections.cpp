#include <algorithm>
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "static-var-sections"

using namespace llvm;

namespace
{

std::string UniquifySymbol(const Module &M,
                           std::string &section,
                           GlobalVariable &Sym)
{
  std::string newName;
  auto filter = [](char c){ return !isalnum(c); };

  newName = M.getName().str() + "_" + Sym.getName().str();
  std::replace_if(newName.begin(), newName.end(), filter, '_');

  return section + newName;
}

/**
 * This pass searches for static, i.e., module-private, global variables and
 * modifies their linkage to be in their own sections similarly to other
 * global variables with the -fdata-sections switch.  By default, LLVM doesn't
 * apply -fdata-sections to static global variables.
 */
class StaticVarSections : public ModulePass
{
public:
	static char ID;

	StaticVarSections() : ModulePass(ID) {}
	~StaticVarSections() {}

	/* ModulePass virtual methods */
  virtual void getAnalysisUsage(AnalysisUsage &AU) const { AU.setPreservesCFG(); }
	virtual bool runOnModule(Module &M)
  {
    bool modified = false;
    Module::iterator it, ite;
    Module::global_iterator gl, gle; // for global variables
  
    DEBUG(errs() << "\n********** Beginning StaticVarSections **********\n"
                 << "********** Module: " << M.getName() << " **********\n\n");
  
    // Iterate over all static globals and place them in their own section
    for(gl = M.global_begin(), gle = M.global_end(); gl != gle; gl++) {
      std::string secName = ".";
      if(gl->isThreadLocal()) secName += "t";
  
      if(gl->hasCommonLinkage() &&
         gl->getName().find(".cache.") != std::string::npos) {
        gl->setLinkage(GlobalValue::InternalLinkage);
      }
  
      // InternalLinkage is specifically for STATIC variables
      if(gl->hasInternalLinkage() && !gl->hasSection()) {
        if(gl->isConstant()) {
          //Belongs in RODATA
          assert(!gl->isThreadLocal() && "TLS data should not be in .rodata");
          secName += "rodata.";
        }
        else if(gl->getInitializer()->isZeroValue()) {
          //Belongs in BSS
          secName += "bss.";
        }
        else {
          //Belongs in DATA
          secName += "data.";
        }

        secName = UniquifySymbol(M, secName, *gl);
        gl->setSection(secName);
        modified = true;

        DEBUG(errs() << *gl << " - new section: " << secName << "\n");
      } else {
        DEBUG(errs() << "> " <<  *gl << ", linkage: "
                     << gl->getLinkage() << "\n");
        continue;
      }
    }
    
    return modified;
  }
  virtual const char *getPassName() const { return "Static variables in separate sections"; }
};

} /* end anonymous namespace */

char StaticVarSections::ID = 0;
INITIALIZE_PASS(StaticVarSections, "static-var-sections",
  "Put static variables into separate sections", false, false)

namespace llvm {
  ModulePass *createStaticVarSectionsPass() { return new StaticVarSections(); }
}

