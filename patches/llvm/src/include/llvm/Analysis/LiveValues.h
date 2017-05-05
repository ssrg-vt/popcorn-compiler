/*
 * Calculate live-value sets for functions.
 *
 * Liveness-analysis is based on the non-iterative dataflow algorithm for
 * reducible graphs by Brandner et. al in:
 *
 * "Computing Liveness Sets for SSA-Form Programs"
 * URL: https://hal.inria.fr/inria-00558509v1/document
 * Accessed: 5/19/2016
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/19/2016
 */

#ifndef _LIVE_VALUES_H
#define _LIVE_VALUES_H

#include <map>
#include <set>
#include <list>
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopNestingTree.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

class LiveValues : public FunctionPass
{
public:
  typedef std::pair<const BasicBlock *, const BasicBlock *> Edge;

  static char ID;

  /**
   * Default constructor.
   */
  LiveValues(void);

  /**
   * Default destructor.
   */
  ~LiveValues(void) {}

  /**
   * Return whether or not a given type should be included in the analysis.
   * @return true if the type is included in liveness sets, false otherwise
   */
  bool includeAsm(void) const { return inlineasm; }
  bool includeBitcasts(void) const { return bitcasts; }
  bool includeComparisons(void) const { return comparisons; }
  bool includeConstants(void) const { return constants; }
  bool includeMetadata(void) const { return metadata; }

  /**
   * Set whether or not to include the specified type in the analysis (all
   * are set to false by default by the constructor).
   * @param include true if it should be included, false otherwise
   */
  void includeAsm(bool include) { inlineasm = include; }
  void includeBitcasts(bool include) { bitcasts = include; }
  void includeComparisons(bool include) { comparisons = include; }
  void includeConstants(bool include) { constants = include; }
  void includeMetadata(bool include) { metadata = include; }

  /**
   * Register which analysis passes we need.
   * @param AU an analysis usage object
   */
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  /**
   * Calculate liveness sets for a function.
   * @param F a function for which to calculate live values.
   * @return false, always
   */
  virtual bool runOnFunction(Function &F);

  /**
   * Get the human-readable name of the pass.
   * @return the pass name
   */
  virtual const char *getPassName() const { return "Live value analysis"; }

  /**
   * Print a human-readable version of the analysis.
   * @param O an output stream
   * @param F the function for which to print analysis
   */
  virtual void print(raw_ostream &O, const Function *F) const;

  /**
   * Return the live-in set for a basic block.
   * @param BB a basic block
   * @return a set of live-in values for the basic block; this set must be
   *         freed by the user.
   */
  std::set<const Value *> *getLiveIn(const BasicBlock *BB) const;

  /**
   * Return the live-out set for a basic block.
   * @param BB a basic block
   * @return a set of live-out values for the basic block; this set must be
   *         freed by the user.
   */
  std::set<const Value *> *getLiveOut(const BasicBlock *BB) const;

  /**
   * Get the live values across a given instruction, i.e., values live right
   * after the invocation of the instruction (excluding the value defined by
   * the instruction itself).
   * @param inst an instruction
   * @return the set of values live directly before the instruction; this set
   *         must be freed by the user.
   */
  std::set<const Value *> *
  getLiveValues(const Instruction *inst) const;

private:
  /* Should values of each type be included? */
  bool inlineasm;
  bool bitcasts;
  bool comparisons;
  bool constants;
  bool metadata;

  /* A loop nesting forest composed of 0 or more loop nesting trees. */
  typedef std::list<LoopNestingTree> LoopNestingForest;

  /* Maps live values to a basic block. */
  typedef std::map<const BasicBlock *, std::set<const Value *> > LiveVals;
  typedef std::pair<const BasicBlock *, std::set<const Value *> > LiveValsPair;

  /* Store analysis for all functions. */
  std::map<const Function *, LiveVals> FuncBBLiveIn;
  std::map<const Function *, LiveVals> FuncBBLiveOut;

  /**
   * Return whether or not a value is a variable that should be tracked.
   * @param val a value
   * @return true if the value is a variable to be tracked, false otherwise
   */
  bool includeVal(const Value *val) const;

  /**
   * Insert the values used in phi-nodes at the beginning of basic block S (as
   * values live from B) into the set uses.
   * @param B a basic block which passes live values into phi-nodes in S
   * @param S a basic block, successor to B
   * @param uses set in which to add values used in phi-nodes in B
   * @return the number of values added to the set
   */
  unsigned phiUses(const BasicBlock *B,
                   const BasicBlock *S,
                   std::set<const Value *> &uses);

  /**
   * Insert the values defined by the phi-nodes at the beginning of basic block
   * B into the set defs.
   * @param B a basic block
   * @param defs set in which to add values defined by phi-nodes in B
   * @return the number of values added to the set
   */
  unsigned phiDefs(const BasicBlock *B,
                   std::set<const Value *> &defs);

  /**
   * Do a post-order traversal of the control flow graph to calculate partial
   * liveness sets.
   * @param F a function for which to calculate per-basic block partial
   *          liveness sets
   * @param liveIn per-basic block live-in values
   * @param liveOut per-basic block live-out values
   */
  void dagDFS(Function &F, LiveVals &liveIn, LiveVals &liveOut);

  /**
   * Construct the loop-nesting forest for a function.
   * @param F a function for which to calculate the loop-nesting forest.
   * @param LNF a loop nesting forest to populate with loop nesting trees.
   */
  void constructLoopNestingForest(Function &F, LoopNestingForest &LNF);

  /**
   * Propagate live values throughout the loop-nesting tree.
   * @param loopNest a loop-nesting tree
   * @param liveIn per-basic block live-in values
   * @param liveOut per-basic block live-out values
   */
  void propagateValues(const LoopNestingTree &loopNest,
                       LiveVals &liveIn,
                       LiveVals &liveOut);

  /**
   * Propagate live values within loops for all loop-nesting trees in the
   * function's loop-nesting forest.
   * @param LNF a loop nesting forest
   * @param liveIn per-basic block live-in values
   * @param liveOut per-basic block live-out values
   */
  void loopTreeDFS(LoopNestingForest &LNF,
                   LiveVals &liveIn,
                   LiveVals &liveOut);
};

} /* llvm namespace */

#endif /* _LIVE_VALUES_H */

