#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "AssociateLiteral.h"

#define DEBUG_TYPE "associate-literal"

using namespace llvm;

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

/**
 * Default constructor.
 */
AssociateLiteral::AssociateLiteral() : ModulePass(ID), numInstrumented(0) {}

/**
 * Default destructor.
 */
AssociateLiteral::~AssociateLiteral() {}

/**
 * Make RO Data & inlined strings have symbols to be aligned
 */
bool AssociateLiteral::runOnModule(Module &M) {
  bool modified = false;
  string root1,root2, newName;
  Module::iterator it, ite;
  Module::global_iterator gl, gle; //for global variables?
  //could also handle by just iterating through .str - .str.1 - .str.2 ....

  //DEBUG(outs() << "AssociateLiteral: entering module " << M.getName() << "\n");
  //outs() << "AssociateLiteral: entering module " << M.getName() << "\n";

  // Iterate over all globals and transform to associate symbol to strings for
  // each module
  for(gl = M.global_begin(), gle = M.global_end(); gl != gle; gl++) {
    // DONT NEED TO CHANGE NAME PER-SAY just change type
    // PrivateLinkage does NOT show up in any symbol table in the object file!
    if(gl->getLinkage() == GlobalValue::PrivateLinkage) {
      //DEBUG(outs() << "\nPRIVATE: " <<  *gl << "\n");
      //outs() << "\nPRIVATE: " <<  *gl << "\n";
      //change Linkage
      //FROM private unnamed_addr constant [21 x i8]
      //TO global [59 x i8]
      gl->setLinkage(GlobalValue::ExternalLinkage);

      // Make the global's name unique so we don't clash when linking with
      // other files
      string::size_type minusPath = M.getName().find_last_of('/'); 
      //root = M.getName().substr(minusPath+1, pos);
      root1 = M.getName().substr(minusPath+1);
      //outs() << "Old string name: " << M.getName() << "\n";
      //outs() << "Root: " << root1 << "\n";
      string::size_type pos = root1.find_first_of('.'); 
      root2 = root1.substr(0,pos);
      //outs() << "Root: " << root2 << "\n";
      newName = root2 + "_" + gl->getName().str();
      gl->setName(newName);
      //DEBUG(outs() << "New anonymous string name: " << newName << "\n";);
      //outs() << "New anonymous string name: " << newName << "\n";

      // Also REMOVE unnamed_addr value
      if(gl->hasUnnamedAddr()) {
        gl->setUnnamedAddr(false);
      }
      //DEBUG(errs() << "New: " <<  *gl << "\n");
      //outs() << "New: " <<  *gl << "\n";
      this->numInstrumented++;
    } else {
      //DEBUG(errs() << "> " <<  *gl << "\n");
      //DEBUG(errs() << "Linkage: " <<  gl->getLinkage() << "\n");
      //outs() << "> " <<  *gl << "\n";
      //outs() << "Linkage: " <<  gl->getLinkage() << "\n";
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

