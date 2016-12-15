namespace llvm {

//===----------------------------------------------------------------------===//
//
// NameStringLiterals - Give symbol names to anonymous string literals so they
// can be aligned at link-time
//
ModulePass *createNameStringLiteralsPass();

//===----------------------------------------------------------------------===//
//
// StaticVarSections - Put static global variables into their own sections
//
ModulePass *createStaticVarSectionsPass();

}
