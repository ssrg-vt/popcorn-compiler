#include <algorithm>
#include <cctype>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "name-string-literals"
#define CHARS_FOR_NAME 10

using namespace llvm;

namespace
{

/**
 * Generate unique name for private anonymous string literals.  Uses the
 * filename, LLVM's temporary name and (up to) the first 10 characters of the
 * string.  Converts non-alphanumeric characters to underscores.
 */
std::string UniquifySymbol(const Module &M, GlobalVariable &Sym)
{
  std::string newName;
  std::string::size_type loc;
  auto filter = [](char c){ return !isalnum(c); };

  newName = M.getName();
  loc = newName.find_last_of('.');
  newName = newName.substr(0, loc) + "_" + Sym.getName().str() + "_";
  std::replace_if(newName.begin(), newName.end(), filter, '_');

  // Check if it's a string, and if so use string content to uniquify
  if(Sym.hasInitializer()) {
    Constant *Initializer = Sym.getInitializer();
    if(isa<ConstantDataSequential>(Initializer)) {
      ConstantDataSequential *CDS = cast<ConstantDataSequential>(Initializer);
      if(CDS->isString()) {
        std::string data = CDS->getAsString().substr(0, CHARS_FOR_NAME);
        std::replace_if(data.begin(), data.end(), filter, '_');
        newName += data;
      }
    }
  }

  return newName;
}

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
    std::string newName;
    Module::global_iterator gl, gle; // for global variables

    DEBUG(errs() << "\n********** Begin NameStringLiterals **********\n"
                 << "********** Module: " << M.getName() << " **********\n\n");

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
        newName = UniquifySymbol(M, *gl);
        gl->setName(newName);

        // Also REMOVE unnamed_addr value
        if(gl->hasUnnamedAddr()) {
          gl->setUnnamedAddr(false);
        }

        modified = true;

        DEBUG(errs() << "New anonymous string name: " << newName << "\n";);
      } else {
        DEBUG(errs() << "> " <<  *gl << ", linkage: "
                     << gl->getLinkage() << "\n");
      }
    }
  
    return modified;
  }
  virtual const char *getPassName() const { return "Name string literals"; }
};

} /* end anonymous namespace */

char NameStringLiterals::ID = 0;
INITIALIZE_PASS(NameStringLiterals, "name-string-literals",
  "Generate symbols for anonymous string literals", false, false)

namespace llvm {
  ModulePass *createNameStringLiteralsPass() { return new NameStringLiterals(); }
}

