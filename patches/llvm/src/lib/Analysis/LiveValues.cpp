#include "llvm/Analysis/LiveValues.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "live-values"

using namespace llvm;

char LiveValues::ID = 0;
INITIALIZE_PASS_BEGIN(LiveValues, "live-values", 
                    "Live-value set calculation", true, true)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(LiveValues, "live-values", 
                    "Live-value set calculation", true, true)

namespace llvm {
  FunctionPass *createLiveValuesPass() { return new LiveValues(); }
}

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

LiveValues::LiveValues(void)
  : FunctionPass(ID), inlineasm(false), bitcasts(false), comparisons(true),
    constants(false), metadata(false) {}

void LiveValues::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<LoopInfoWrapperPass>();
  AU.setPreservesAll();
}

bool LiveValues::runOnFunction(Function &F)
{
  if(FuncBBLiveIn.count(&F))
  {
    DEBUG(
      errs() << "\nFound previous analysis for " << F.getName() << "\n\n";
      print(errs(), &F);
    );
  }
  else
  {
    DEBUG(errs() << "\n********** Beginning LiveValues **********\n"
                 << "********** Function: " << F.getName() << " **********\n\n"
                    "LiveValues: performing bottom-up dataflow analysis\n");

    LoopNestingForest LNF;
    FuncBBLiveIn.emplace(&F, LiveVals());
    FuncBBLiveOut.emplace(&F, LiveVals());

    /* 1. Compute partial liveness sets using a postorder traversal. */
    dagDFS(F, FuncBBLiveIn[&F], FuncBBLiveOut[&F]);

    DEBUG(errs() << "LiveValues: constructing loop-nesting forest\n");

    /* 2. Construct loop-nesting forest. */
    constructLoopNestingForest(F, LNF);

    DEBUG(errs() << "LiveValues: propagating values within loop-nests\n");

    /* 3. Propagate live variables within loop bodies. */
    loopTreeDFS(LNF, FuncBBLiveIn[&F], FuncBBLiveOut[&F]);

    DEBUG(
      print(errs(), &F);
      errs() << "LiveValues: finished analysis\n"
    );
  }

  return false;
}

void
LiveValues::print(raw_ostream &O, const Function *F) const
{
  LiveVals::const_iterator bbIt;
  std::set<const Value *>::const_iterator valIt;
  const Module *M = F->getParent();

  O << "LiveValues: results of live-value analysis\n";

  if(!FuncBBLiveIn.count(F) || !FuncBBLiveOut.count(F))
  {
    if(F->hasName())
      O << "No liveness information for function " << F->getName() << "\n";
    else
      O << "No liveness information for requested function\n";
  }
  else
  {
    for(bbIt = FuncBBLiveIn.at(F).cbegin();
        bbIt != FuncBBLiveIn.at(F).cend();
        bbIt++)
    {
      const BasicBlock *bb = bbIt->first;
      const std::set<const Value *> &liveInVals = bbIt->second;
      const std::set<const Value *> &liveOutVals = FuncBBLiveOut.at(F).at(bb);

      bb->printAsOperand(O, false, M);
      O << "\n  Live-in:\n    ";
      for(valIt = liveInVals.cbegin(); valIt != liveInVals.cend(); valIt++)
      {
        (*valIt)->printAsOperand(O, false, M);
        O << " ";
      }

      O << "\n  Live-out:\n    ";
      for(valIt = liveOutVals.cbegin(); valIt != liveOutVals.cend(); valIt++)
      {
        (*valIt)->printAsOperand(O, false, M);
        O << " ";
      }

      O << "\n";
    }
  }
}

std::set<const Value *> *LiveValues::getLiveIn(const BasicBlock *BB) const
{
  const Function *F = BB->getParent();
  return new std::set<const Value *>(FuncBBLiveIn.at(F).at(BB));
}

std::set<const Value *> *LiveValues::getLiveOut(const BasicBlock *BB) const
{
  const Function *F = BB->getParent();
  return new std::set<const Value *>(FuncBBLiveOut.at(F).at(BB));
}

std::set<const Value *>
*LiveValues::getLiveValues(const Instruction *inst) const
{
  const BasicBlock *BB = inst->getParent();
  const Function *F = BB->getParent();
  BasicBlock::const_reverse_iterator ri, rie;
  std::set<const Value *> *live =
    new std::set<const Value *>(FuncBBLiveOut.at(F).at(BB));

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

///////////////////////////////////////////////////////////////////////////////
// Private API
///////////////////////////////////////////////////////////////////////////////

bool LiveValues::includeVal(const llvm::Value *val) const
{
  bool include = true;

  // TODO other values that should be filtered out?
  if(isa<BasicBlock>(val))
    include = false;
  else if(isa<InlineAsm>(val) && !inlineasm)
    include = false;
  else if(isa<BitCastInst>(val) && !bitcasts)
    include = false;
  else if(isa<CmpInst>(val) && !comparisons)
    include = false;
  else if(isa<Constant>(val) && !constants)
    include = false;
  else if(isa<MetadataAsValue>(val) && !metadata)
    include = false;

  return include;
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

void LiveValues::dagDFS(Function &F, LiveVals &liveIn, LiveVals &liveOut)
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
        for(std::set<const Value *>::const_iterator vi = liveIn[*S].begin();
            vi != liveIn[*S].end();
            vi++)
          if(!phiDefined.count(*vi) && includeVal(*vi)) live.insert(*vi);
        phiDefined.clear();
      }
    }
    liveOut.insert(LiveValsPair(*B, std::set<const Value *>(live)));

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
    liveIn.insert(LiveValsPair(*B, std::set<const Value *>(live)));

    live.clear();

    DEBUG(
      errs() << "  ";
      (*B)->printAsOperand(errs(), false);
      errs() << ":\n";
      errs() << "    Live-in:\n      ";
      std::set<const Value *>::const_iterator it;
      for(it = liveIn[*B].begin(); it != liveIn[*B].end(); it++)
      {
        (*it)->printAsOperand(errs(), false);
        errs() << " ";
      }
      errs() << "\n    Live-out:\n      ";
      for(it = liveOut[*B].begin(); it != liveOut[*B].end(); it++)
      {
        (*it)->printAsOperand(errs(), false);
        errs() << " ";
      }
      errs() << "\n";
    );
  }
}

void LiveValues::constructLoopNestingForest(Function &F,
                                            LoopNestingForest &LNF)
{
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  for(scc_iterator<Function *> scc = scc_begin(&F);
      scc != scc_end(&F);
      ++scc)
  {
    const std::vector<BasicBlock *> &SCC = *scc;
    LNF.emplace_back(SCC, LI);

    DEBUG(
      errs() << "Loop nesting tree: "
             << LNF.back().size() << " node(s), loop-nesting depth: "
             << LNF.back().depth() << "\n";
      LNF.back().print(errs());
      errs() << "\n"
    );
  }
}

void LiveValues::propagateValues(const LoopNestingTree &loopNest,
                                 LiveVals &liveIn,
                                 LiveVals &liveOut)
{
  std::set<const Value *> liveLoop, phiDefined;

  /* Iterate over all loop nodes. */
  for(LoopNestingTree::loop_iterator loop = loopNest.loop_begin();
      loop != loopNest.loop_end();
      loop++)
  {
    /* Calculate LiveLoop (lines 3 & 4 of Algorithm 3). */
    phiDefs(*loop, phiDefined);
    for(std::set<const Value *>::const_iterator it = liveIn[*loop].begin();
        it != liveIn[*loop].end();
        it++)
      if(!phiDefined.count(*it) && includeVal(*it))
        liveLoop.insert(*it);

    /* Propagate values to children (lines 5-8 of Algorithm 3). */
    for(LoopNestingTree::child_iterator child = loopNest.children_begin(loop);
        child != loopNest.children_end(loop);
        child++) {
      for(std::set<const Value *>::const_iterator it = liveLoop.begin();
          it != liveLoop.end();
          it++) {
        liveIn[*child].insert(*it);
        liveOut[*child].insert(*it);
      }
    }

    liveLoop.clear();
  }
}

void LiveValues::loopTreeDFS(LoopNestingForest &LNF,
                             LiveVals &liveIn,
                             LiveVals &liveOut)
{
  LoopNestingForest::const_iterator it;
  for(it = LNF.begin(); it != LNF.end(); it++)
    propagateValues(*it, liveIn, liveOut);
}

