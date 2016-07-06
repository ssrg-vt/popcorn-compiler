#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "SectionStatic.h"

#define DEBUG_TYPE "section-static"

using namespace llvm;

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/**
 * Default constructor.
 */
SectionStatic::SectionStatic() : ModulePass(ID), numInstrumented(0) {}

/**
 * Default destructor.
 */
SectionStatic::~SectionStatic() {}

/**
 * Make RO Data & inlined strings have symbols to be aligned
 */
bool SectionStatic::runOnModule(Module &M) {
  bool modified = false;
  Module::iterator it, ite;
  Module::global_iterator gl, gle; // for global variables

  DEBUG(errs() << "SectionStatic: entering module " << M.getName() << "\n");

  // Iterate over all static globals and place them in their own section
  for(gl = M.global_begin(), gle = M.global_end(); gl != gle; gl++) {
    // InternalLinkage is specifically for STATIC variables
    if(gl->getLinkage() == GlobalValue::InternalLinkage) {  
      DEBUG(errs() << "\nInternal: " <<  *gl << "\n");
      if(gl->isConstant()) {
        //Belongs in RODATA
        DEBUG(errs() << "RO Name:" << ".rodata." + gl->getName().str() << "\n");
        gl->setSection(".rodata." + gl->getName().str());
      }
      else if(gl->getInitializer()->isZeroValue()) {
        //Belongs in BSS
        DEBUG(errs() << "Zero Value or No def:" << gl->getValueType() <<"\n");
        DEBUG(errs() << "BSS Name:" << ".bss." + gl->getName().str() << "\n");
        gl->setSection(".bss." + gl->getName().str());
      }
      else {
        //Belongs in DATA
        DEBUG(errs() << "D Name:" << ".data." + gl->getName().str() << "\n");
        gl->setSection(".data." + gl->getName().str());
      }
      DEBUG(errs() << "New: " <<  *gl << "\n");
      this->numInstrumented++;
    } else {
      DEBUG(errs() << "> " <<  *gl << "\n");
      DEBUG(errs() << "Linkage: " <<  gl->getLinkage() << "\n");
      continue;
    }
  }

  if(numInstrumented > 0) modified = true;
  
  return modified;
}

char SectionStatic::ID = 0;
static RegisterPass<SectionStatic> RPSectionStatic(
  "section-static",
  "Allow static variables to have their own sections so that they can be aligned",
  false,
  false
);

