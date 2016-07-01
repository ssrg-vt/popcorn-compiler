#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/Debug.h"
#include "LiveValues.h"

#define DEBUG_TYPE "live-values"

using namespace llvm;
typedef std::pair<const BasicBlock *, const BasicBlock *> Edge;

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

LiveValues::LiveValues(void)
  : FunctionPass(ID), inlineasm(false), bitcasts(false), comparisons(false),
    constants(false), metadata(false) {}

LiveValues::~LiveValues(void)
{
  std::map<const BasicBlock *, std::set<const Value *> *>::iterator bbIt;
  std::list<LoopNestingTree *>::iterator treeIt;

  for(bbIt = liveIn.begin(); bbIt != liveIn.end(); bbIt++)
    delete bbIt->second;
  for(bbIt = liveOut.begin(); bbIt != liveOut.end(); bbIt++)
    delete bbIt->second;
  for(treeIt = loopNestingForest.begin();
      treeIt != loopNestingForest.end();
      treeIt++)
    delete *treeIt;
}

void LiveValues::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<LoopInfoWrapperPass>();
  AU.setPreservesAll();
}

bool LiveValues::runOnFunction(Function &F)
{
  DEBUG(errs() << "LiveValues: beginning live-value analysis\n\r"
                  "LiveValues: performing bottom-up dataflow analysis\n\r");

  /* 1. Compute partial liveness sets using a postorder traversal. */
  dagDFS(F);

  DEBUG(errs() << "LiveValues: constructing loop-nesting forest\n\r");

  /* 2. Construct loop-nesting forest. */
  constructLoopNestingForest(F);

  DEBUG(errs() << "LiveValues: propagating values within loop-nests\n\r");

  /* 3. Propagate live variables within loop bodies. */
  loopTreeDFS();

  DEBUG(
    print(errs(), F.getParent());
    errs() << "LiveValues: finished analysis\n\r"
  );

  return false;
}

void LiveValues::print(raw_ostream &O, const Module *M) const
{
  std::map<const BasicBlock *, std::set<const Value *> *>::const_iterator bbIt;
  std::set<const Value *>::const_iterator valIt;
  const BasicBlock *bb;
  const std::set<const Value *> *vals;

  O << "LiveValues: results of live-value analysis\n\r";

  for(bbIt = liveIn.cbegin(); bbIt != liveIn.cend(); bbIt++)
  {
    bb = bbIt->first;
    vals = bbIt->second;

    bbIt->first->printAsOperand(O, false, M);

    O << "\n\r  Live-in:";
    for(valIt = vals->cbegin(); valIt != vals->cend(); valIt++)
    {
      O << " ";
      (*valIt)->printAsOperand(O, false, M);
    }

    O << "\n\r  Live-out:";
    for(valIt = liveOut.at(bb)->cbegin();
        valIt != liveOut.at(bb)->cend();
        valIt++)
    {
      O << " ";
      (*valIt)->printAsOperand(O, false, M);
    }

    O << "\n\r";
  }
}

std::set<const Value *> *LiveValues::getLiveIn(const BasicBlock *BB) const
{
  return new std::set<const Value *>(*liveIn.at(BB));
}

std::set<const Value *> *LiveValues::getLiveOut(const BasicBlock *BB) const
{
  return new std::set<const Value *>(*liveOut.at(BB));
}

std::set<const Value *>
*LiveValues::getLiveValues(const Instruction *inst) const
{
  const BasicBlock *BB = inst->getParent();
  BasicBlock::const_reverse_iterator ri, rie;
  std::set<const Value *> *live = new std::set<const Value *>(*liveOut.at(BB));

  for(ri = BB->rbegin(), rie = BB->rend(); ri != rie; ri++)
  {
    if(&*ri == inst) break;

    live->erase(&*ri);
    for(User::const_op_iterator op = ri->op_begin();
        op != ri->op_end();
        op++)
      if(includeVal(*op))
        live->insert(*op);
  }
  live->erase(&*ri);

  return live;
}

/* Define pass ID & register pass w/ driver. */
char LiveValues::ID = 0;
static RegisterPass<LiveValues> RPLiveValues(
  "live-values",
  "Calculate live-value sets for basic blocks in functions",
  true,
  true
);

///////////////////////////////////////////////////////////////////////////////
// Private API
///////////////////////////////////////////////////////////////////////////////

bool LiveValues::includeVal(const llvm::Value *val) const
{
  IntegerType *IntTy;

  // TODO other values that should be filtered out?
  if(isa<BasicBlock>(val))
    return false;
  else if(isa<InlineAsm>(val) && !inlineasm)
    return false;
  else if(isa<BitCastInst>(val) && !bitcasts)
    return false;
  else if(isa<CmpInst>(val) && !comparisons)
    return false;
  else if(isa<Constant>(val) && !constants)
    return false;
  else if(isa<MetadataAsValue>(val) && !metadata)
    return false;
  else if((IntTy = dyn_cast<IntegerType>(val->getType())))
  {
    if(IntTy->getBitWidth() == 1 || IntTy->getBitWidth() == 8)
      return false;
  }
  return true;
}

unsigned LiveValues::phiUses(const BasicBlock *B,
                             const BasicBlock *S,
                             std::set<const Value *> &uses)
{
  const PHINode *phi;
  unsigned added = 0;

  for(BasicBlock::const_iterator it = S->begin(); it != S->end(); it++)
  {
    if((phi = dyn_cast<PHINode>(&*it))) {
      for(unsigned i = 0; i < phi->getNumIncomingValues(); i++)
        if(phi->getIncomingBlock(i) == B &&
           includeVal(phi->getIncomingValue(i)))
          if(uses.insert(phi->getIncomingValue(i)).second)
            added++;
    }
    else break; // phi-nodes are always at the start of the basic block
  }

  return added;
}

unsigned LiveValues::phiDefs(const BasicBlock *B,
                             std::set<const Value *> &uses)
{
  const PHINode *phi;
  unsigned added = 0;

  for(BasicBlock::const_iterator it = B->begin(); it != B->end(); it++)
  {
    if((phi = dyn_cast<PHINode>(&*it))) {
      if(includeVal(phi))
        if(uses.insert(&*it).second)
          added++;
    }
    else break; // phi-nodes are always at the start of the basic block
  }

  return added;
}

void LiveValues::dagDFS(Function &F)
{
  std::set<const Value *> live, phiDefined;
  std::set<Edge> loopEdges;
  SmallVector<Edge, 16> loopEdgeVec;

  /* Find loop edges & convert to set for existence checking. */
  FindFunctionBackedges(F, loopEdgeVec);
  for(SmallVectorImpl<Edge>::const_iterator eit = loopEdgeVec.begin();
      eit != loopEdgeVec.end();
      eit++)
    loopEdges.insert(*eit);

  /* Calculate partial liveness sets for CFG nodes. */
  for(auto B = po_iterator<const BasicBlock *>::begin(&F.getEntryBlock());
      B != po_iterator<const BasicBlock *>::end(&F.getEntryBlock());
      B++)
  {
    /* Calculate live-out set (lines 4-7 of Algorithm 2). */
    for(succ_const_iterator S = succ_begin(*B); S != succ_end(*B); S++)
    {
      // Note: skip self-loop-edges, as adding values from phi-uses of this
      // block causes use-def violations, and LLVM will complain.  This
      // shouldn't matter, as phi-defs will cover this case.
      if(*S == *B) continue;

      phiUses(*B, *S, live);
      if(!loopEdges.count(Edge(*B, *S)))
      {
        phiDefs(*S, phiDefined);
        for(std::set<const Value *>::const_iterator vi = liveIn[*S]->begin();
            vi != liveIn[*S]->end();
            vi++)
          if(!phiDefined.count(*vi) && includeVal(*vi)) live.insert(*vi);
        phiDefined.clear();
      }
    }
    liveOut[*B] = new std::set<const Value *>(live);

    /* Calculate live-in set (lines 8-11 of Algorithm 2). */
    for(BasicBlock::const_reverse_iterator inst = (*B)->rbegin();
        inst != (*B)->rend();
        inst++)
    {
      if(isa<PHINode>(&*inst)) break;

      live.erase(&*inst);
      for(User::const_op_iterator op = inst->op_begin();
          op != inst->op_end();
          op++)
        if(includeVal(*op)) live.insert(*op);
    }
    phiDefs(*B, live);
    liveIn[*B] = new std::set<const Value *>(live);

    live.clear();

    DEBUG(
      errs() << "  ";
      (*B)->printAsOperand(errs(), false);
      errs() << ":\n\r";
      errs() << "    Live-in:";
      std::set<const Value *>::const_iterator it;
      for(it = liveIn[*B]->begin(); it != liveIn[*B]->end(); it++)
      {
        errs() << " ";
        (*it)->printAsOperand(errs(), false);
      }
      errs() << "\n\r    Live-out:";
      for(it = liveOut[*B]->begin(); it != liveOut[*B]->end(); it++)
      {
        errs() << " ";
        (*it)->printAsOperand(errs(), false);
      }
      errs() << "\n\r";
    );
  }
}

void LiveValues::constructLoopNestingForest(Function &F)
{
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  LoopNestingTree *loopNest;

  for(scc_iterator<Function *> scc = scc_begin(&F);
      scc != scc_end(&F);
      ++scc)
  {
    const std::vector<BasicBlock *> &SCC = *scc;
    loopNest = new LoopNestingTree(SCC, LI);
    loopNestingForest.push_back(loopNest);

    DEBUG(
      errs() << "Loop nesting tree: "
             << loopNest->size() << " node(s), loop-nesting depth: "
             << loopNest->depth() << "\n\r";
      loopNest->print(errs());
      errs() << "\n\r"
    );
  }
}

void LiveValues::propagateValues(const LoopNestingTree *loopNest)
{
  std::set<const Value *> liveLoop;
  std::set<const Value *> phiDefined;

  /* Iterate over all loop nodes. */
  for(LoopNestingTree::loop_iterator loop = loopNest->loop_begin();
      loop != loopNest->loop_end();
      loop++)
  {
    /* Calculate LiveLoop (lines 3 & 4 of Algorithm 3). */
    phiDefs(*loop, phiDefined);
    for(std::set<const Value *>::const_iterator it = liveIn[*loop]->begin();
        it != liveIn[*loop]->end();
        it++)
      if(!phiDefined.count(*it) && includeVal(*it))
        liveLoop.insert(*it);

    /* Propagate values to children (lines 5-8 of Algorithm 3). */
    for(LoopNestingTree::child_iterator child = loopNest->children_begin(loop);
        child != loopNest->children_end(loop);
        child++) {
      for(std::set<const Value *>::const_iterator it = liveLoop.begin();
          it != liveLoop.end();
          it++) {
        liveIn[*child]->insert(*it);
        liveOut[*child]->insert(*it);
      }
    }

    liveLoop.clear();
  }
}

void LiveValues::loopTreeDFS()
{
  std::list<LoopNestingTree *>::const_iterator it;
  for(it = loopNestingForest.begin(); it != loopNestingForest.end(); it++)
    propagateValues(*it);
}

