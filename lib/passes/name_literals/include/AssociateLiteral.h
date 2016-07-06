#ifndef _ASSOCIATE_LITERAL_H
#define _ASSOCIATE_LITERAL_H

using namespace std;

namespace
{

class AssociateLiteral : public llvm::ModulePass
{
public:
	static char ID;
	size_t numInstrumented;

	AssociateLiteral();
	~AssociateLiteral();

	/* ModulePass virtual methods */
	virtual bool runOnModule(llvm::Module &M);
};

}

#endif /* _ASSOCIATE_LITERAL_H */
