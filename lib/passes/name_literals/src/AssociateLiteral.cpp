#include <vector>

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "AssociateLiteral.h"

#define DEBUG_TYPE "associate-literal"

using namespace llvm;

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/**
 * Default constructor.
 */
AssociateLiteral::AssociateLiteral()
  : ModulePass(ID), strID(0), numInstrumented(0) {}

/**
 * Default destructor.
 */
AssociateLiteral::~AssociateLiteral() {}

/**
 * Make RO Data & inlined strings have symbols to be aligned
 */
bool AssociateLiteral::runOnModule(Module &M) {
  bool modified = false;
  Module::iterator it, ite;
  Module::global_iterator gl, gle; //for global variables?
  //could also handle by just iterating through .str - .str.1 - .str.2 ....

  DEBUG(errs() << "AssociateLiteral: entering module " << M.getName() << "\n");

  // Iterate over all globals and transform to associate symbol to strings for
  // each module
  for(gl = M.global_begin(), gle = M.global_end(); gl != gle; gl++) {
    // DONT NEED TO CHANGE NAME PER-SAY just change type
    // PrivateLinkage does NOT show up in any symbol table in the object file!
    if(gl->getLinkage() == GlobalValue::PrivateLinkage) {
      DEBUG(errs() << "\nPRIVATE: " <<  *gl << "\n");
      //change Linkage
      //FROM private unnamed_addr constant [21 x i8]
      //TO global [59 x i8]
      gl->setLinkage(GlobalValue::ExternalLinkage);
      //Also REMOVE unnamed_addr value
      if(gl->hasUnnamedAddr()) {
        gl->setUnnamedAddr(false);
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

char AssociateLiteral::ID = 0;
static RegisterPass<AssociateLiteral> RPAssociateLiteral(
  "associate-literal",
  "Associate symbol to anonomous string literal",
  false,
  false
);

