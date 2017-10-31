//===- LoopPaths.h - Enumerate paths in loops -------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements analysis which enumerates paths in loops.  In
// particular, this pass calculates all paths in loops which are of the
// following form:
//
//  - Header to backedge, with no equivalence points on the path
//  - Header to with equivalence point
//  - Equivalence point to equivalence point
//  - Equivalence point to backedge
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_LOOPPATHS_H
#define LLVM_ANALYSIS_LOOPPATHS_H

#include <set>
#include <vector>
#include <list>
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

namespace llvm {

//===----------------------------------------------------------------------===//
// Utilities
//===----------------------------------------------------------------------===//

/// Sort loops based on nesting depth, with deeper-nested loops coming first.
/// If the depths are equal, sort based on pointer value so that distinct loops
/// with equal depths are not considered equivalent during insertion.
struct LoopNestCmp {
  bool operator() (const Loop * const &A, const Loop * const &B) {
    unsigned DepthA = A->getLoopDepth(), DepthB = B->getLoopDepth();
    if(DepthA > DepthB) return true;
    else if(DepthA < DepthB) return false;
    else return (uint64_t)A < (uint64_t)B;
  }
};

/// A loop nest, sorted by depth (deeper loops are first).
typedef std::set<Loop *, LoopNestCmp> LoopNest;

/// A set of basic blocks.
typedef SmallPtrSet<const BasicBlock *, 16> BlockSet;

namespace LoopPathUtilities {

/// Populate a LoopNest by traversing the loop L and its children.  Does *not*
/// traverse loops containing L (e.g., loops for which L is a child).
void populateLoopNest(Loop *L, LoopNest &Nest);

/// Get blocks contained in all sub-loops of a loop, including loops nested
/// deeper than those in immediate sub-loops (e.g., blocks of loop depth 3
/// inside loop depth 1).
void getSubBlocks(Loop *L, BlockSet &SubBlocks);

}

//===----------------------------------------------------------------------===//
// LoopPath helper class
//===----------------------------------------------------------------------===//

/// Nodes along a path in a loop, represented by a basic block.
class PathNode {
private:
  /// The block encapsulated by the node.
  const BasicBlock *Block;

  /// Whether or not the block is an exiting block from a sub-loop inside of
  /// the current path.
  bool SubLoopExit;

public:
  PathNode() = delete;
  PathNode(const BasicBlock *Block, bool SubLoopExit = false)
    : Block(Block), SubLoopExit(SubLoopExit) {}

  const BasicBlock *getBlock() const { return Block; }
  bool isSubLoopExit() const { return SubLoopExit; }
  bool operator<(const PathNode &RHS) const { return Block < RHS.Block; }
};

/// A path through the loop, which begins/ends either on the loop's header, the
/// loop's backedge(s) or equivalence points.
class LoopPath {
private:
  /// Nodes that comprise the path.  Iteration over the container is equivalent
  /// to traversing the path, but container has set semantics for quick
  /// existence checks.
  // TODO use a DenseSet for the set template argument, which requires defining
  // a custom comparator
  SetVector<PathNode, std::vector<PathNode>, std::set<PathNode> > Nodes;

  /// The path begins & ends on specific instructions.  Note that Start *must*
  /// be inside the starting block and End *must* be inside the ending block.
  const Instruction *Start, *End;

  /// Does the path start at the loop header?  If not, it by definition starts
  /// at an equivalence point.
  bool StartsAtHeader;

  /// Does the path end at a backedge?  If not, it by definition ends at an
  /// equivalence point.
  bool EndsAtBackedge;

public:
  LoopPath() = delete;
  LoopPath(const std::vector<PathNode> &NodeVector,
           const Instruction *Start, const Instruction *End,
           bool StartsAtHeader, bool EndsAtBackedge);

  bool contains(BasicBlock *BB) const { return Nodes.count(PathNode(BB)); }
  bool contains(const BasicBlock *BB) const
  { return Nodes.count(PathNode(BB)); }

  /// Get the starting point of the path, guaranteed to be either the loop
  /// header or an equivalence point.
  const PathNode &startNode() const { return Nodes.front(); }
  const Instruction *startInst() const { return Start; }

  /// Get the the ending point of the path, guaranteed to be either an
  /// equivalence point or a backedge.
  const PathNode &endNode() const { return Nodes.back(); }
  const Instruction *endInst() const { return End; }

  /// Iterators over the path's blocks.
  SetVector<PathNode>::iterator begin() { return Nodes.begin(); }
  SetVector<PathNode>::iterator end() { return Nodes.end(); }
  SetVector<PathNode>::const_iterator cbegin() const { return Nodes.begin(); }
  SetVector<PathNode>::const_iterator cend() const { return Nodes.end(); }

  /// Return whether the path starts at the loop header or equivalence point.
  bool startsAtHeader() const { return StartsAtHeader; }

  /// Return whether the path ends at a backedge block or equivalence point.
  bool endsAtBackedge() const { return EndsAtBackedge; }

  /// Return whether this is a spanning path.
  bool isSpanningPath() const { return StartsAtHeader && EndsAtBackedge; }

  /// Return whether this is an equivalence point path.
  bool isEqPointPath() const { return !StartsAtHeader || !EndsAtBackedge; }

  std::string toString() const;
  void print(raw_ostream &O) const;
  void dump() const { print(dbgs()); }
};

//===----------------------------------------------------------------------===//
// Pass implementation
//===----------------------------------------------------------------------===//

/// Analyze all paths within a loop nest
class EnumerateLoopPaths : public FunctionPass {
private:
  /// Loop information analysis.
  LoopInfo *LI;

  /// All calculated paths for each analyzed loop.
  DenseMap<const Loop *, std::vector<LoopPath> > Paths;

  /// Whether there are paths of each type through a basic block in a loop.
  /// These are *only* maintained for the current loop, not any sub-loops.
  DenseMap<const Loop *, DenseMap<const BasicBlock *, bool> >
  HasSpPath, HasEqPointPath;

  /// Information about the loop currently being analyzed
  Loop *CurLoop;
  SmallPtrSet<const BasicBlock *, 4> Latches;
  BlockSet SubLoopBlocks;

  /// Depth-first search information for the current path being explored.
  struct LoopDFSInfo {
  public:
    const Instruction *Start;
    std::vector<PathNode> PathNodes;
    bool StartsAtHeader;
  };

  /// Search exit blocks of the loop containing Successor.  Add the terminating
  /// instruction of the block to either of the two vectors, depending if there
  /// is a path of either type through the exit block.
  inline void getSubLoopSuccessors(const BasicBlock *Successor,
                                   std::vector<const Instruction *> &EqPoint,
                                   std::vector<const Instruction *> &Spanning);

  /// Run a depth-first search for paths in the loop starting at an instruction.
  /// Any paths found are added to the vector of paths, and new paths to explore
  /// are added to the search list.
  void loopDFS(const Instruction *I,
               LoopDFSInfo &DFSI,
               std::vector<LoopPath> &CurPaths,
               std::list<const Instruction *> &NewPaths);

  /// Enumerate all paths within a loop, stored in the vector argument.
  void analyzeLoop(Loop *L, std::vector<LoopPath> &CurPaths);

public:
  static char ID;
  EnumerateLoopPaths() : FunctionPass(ID) {}

  /// Pass interface implementation.
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnFunction(Function &F) override;

  /// Re-run analysis to enumerate paths through a loop.  Invalidates all APIs
  /// below which populate containers with paths (for this loop only).
  void rerunOnLoop(Loop *L);

  bool hasPaths(const Loop *L) const { return Paths.count(L); }

  /// Get all the paths through a loop.  Paths in the vector are ordered as
  /// they were discovered in the depth-first traversal of the loop.
  void getPaths(const Loop *L, std::vector<const LoopPath *> &P) const;

  /// Get all paths through a loop that end at a backedge.
  void getBackedgePaths(const Loop *L, std::vector<const LoopPath *> &P) const;
  void getBackedgePaths(const Loop *L, std::set<const LoopPath *> &P) const;

  /// Get all spanning paths through a loop, where a spanning path is defined
  /// as starting at the first instruction of the header of the loop and ending
  /// at the branch in a latch.
  void getSpanningPaths(const Loop *L, std::vector<const LoopPath *> &P) const;
  void getSpanningPaths(const Loop *L, std::set<const LoopPath *> &P) const;

  /// Get all the paths through the loop that begin and/or end at an
  /// equivalence point.
  void getEqPointPaths(const Loop *L, std::vector<const LoopPath *> &P) const;
  void getEqPointPaths(const Loop *L, std::set<const LoopPath *> &P) const;

  /// Get all the paths through a loop that contain a given basic block.
  void getPathsThroughBlock(const Loop *L, BasicBlock *BB,
                            std::vector<const LoopPath *> &P) const;
  void getPathsThroughBlock(const Loop *L, BasicBlock *BB,
                            std::set<const LoopPath *> &P) const;

  /// Return whether there is each type of path through a basic block.
  bool spanningPathThroughBlock(const Loop *L, const BasicBlock *BB) const;
  bool eqPointPathThroughBlock(const Loop *L, const BasicBlock *BB) const;
};

}

#endif

