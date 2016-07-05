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

  /*
   * Maximum per-architecture number of live values (necessary due to LLVM
   * bug, see https://llvm.org/bugs/show_bug.cgi?id=23306).
   */
  static std::map<llvm::Triple::ArchType, size_t> MaxLive;

  /* Utility functions */
  void createSMType(const llvm::Module &M);
  bool addSMDeclaration(llvm::Module &M);
};

std::map<llvm::Triple::ArchType, size_t> StackInfo::MaxLive = {
  {llvm::Triple::ArchType::aarch64, (size_t) - 1},
  {llvm::Triple::ArchType::x86_64, 13}
};

#endif /* _STACK_INFO_H */

