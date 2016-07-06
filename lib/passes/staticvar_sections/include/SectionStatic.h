#ifndef _SECTION_STATIC_H
#define _SECTION_STATIC_H

using namespace std;

namespace
{

class SectionStatic : public llvm::ModulePass
{
public:
	static char ID;
	size_t numInstrumented;

	SectionStatic();
	~SectionStatic();

	/* ModulePass virtual methods */
	virtual bool runOnModule(llvm::Module &M);
};

}

#endif /* _SECTION_STATIC_H */
