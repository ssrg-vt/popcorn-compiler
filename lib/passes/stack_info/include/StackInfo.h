#ifndef _STACK_INFO_H
#define _STACK_INFO_H

class StackInfo : public llvm::ModulePass
{
public:
  static char ID;
  size_t callSiteID;
  size_t numInstrumented;

  StackInfo();
  ~StackInfo();

  /* ModulePass virtual methods */
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
  virtual bool runOnModule(llvm::Module &M);

private:
  /* Stack map instruction creation */
  const llvm::StringRef SMName;
  llvm::Function *SMFunc;
  llvm::FunctionType *SMTy; // Used for creating function declaration

  /* Sort values based on name */
  struct ValueComp {
    bool operator() (const llvm::Value *lhs, const llvm::Value *rhs) const
    { return lhs->getName().compare(rhs->getName()) < 0; }
  };

  /* Utility functions */
  void createSMType(const llvm::Module &M);
  bool addSMDeclaration(llvm::Module &M);
};

#endif /* _STACK_INFO_H */

