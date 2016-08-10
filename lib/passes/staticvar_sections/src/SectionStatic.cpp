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
 * Change the linkage of static global variables to put them in their own
 * sections.
 */
bool SectionStatic::runOnModule(Module &M) {
  bool modified = false;
  Module::iterator it, ite;
  Module::global_iterator gl, gle; // for global variables

  DEBUG(errs() << "SectionStatic: entering module " << M.getName() << "\n");

  // Iterate over all static globals and place them in their own section
  for(gl = M.global_begin(), gle = M.global_end(); gl != gle; gl++) {
    std::string secName = ".";
    if(gl->isThreadLocal()) secName += "t";

    // Change the linkage of clang-generated OpenMP threadprivate caches to
    // internal
    if(gl->hasCommonLinkage() &&
       gl->getName().find(".cache.") != std::string::npos) {
      gl->setLinkage(GlobalValue::InternalLinkage);
      this->numInstrumented++;
    }

    // InternalLinkage is specifically for STATIC variables
    if(gl->hasInternalLinkage()) {
      DEBUG(errs() << "\nInternal: " <<  *gl << "\n");
      if(gl->isConstant()) {
        //Belongs in RODATA
        assert(!gl->isThreadLocal() && "TLS data should not be in .rodata");
        secName += "rodata.";
        DEBUG(errs() << "RO Name: " << secName + gl->getName().str() << "\n");
      }
      else if(gl->getInitializer()->isZeroValue()) {
        //Belongs in BSS
        secName += "bss.";
        DEBUG(errs() << "Zero Value or No def: " << gl->getValueType() <<"\n");
        DEBUG(errs() << "BSS Name: " << secName + gl->getName().str() << "\n");
      }
      else {
        //Belongs in DATA
        secName += "data.";
        DEBUG(errs() << "DATA Name: " << secName + gl->getName().str() << "\n");
      }
    } else {
      DEBUG(errs() << "> " <<  *gl << "\n");
      DEBUG(errs() << "Linkage: " <<  gl->getLinkage() << "\n");
      continue;
    }
    gl->setSection(secName + gl->getName().str());
    this->numInstrumented++;
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

