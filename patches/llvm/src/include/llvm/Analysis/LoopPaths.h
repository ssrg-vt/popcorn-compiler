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
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/BasicBlock.h"

namespace llvm {

/// A path through the loop, which begins/ends either on the loop's header, the
/// loop's backedge(s) or equivalence points.
class LoopPath {
private:
  /// Basic blocks that comprise the path, ordered so that iterating through
  /// the vector is equivalent to traversing the path.
  std::vector<BasicBlock *> Blocks;

  /// The path begins & ends on specific instructions.  Note that start *must*
  /// be inside Blocks.front() and End *must* be inside Blocks.back().
  Instruction *Start, *End;

  /// Does the path start at the loop header?  If not, it by definition starts
  /// at an equivalence point.
  bool StartsAtHeader;

  /// Does the path end at a backedge?  If not, it by definition ends at an
  /// equivalence point.
  bool EndsAtBackedge;

public:
  LoopPath() = delete;
  LoopPath(const std::vector<BasicBlock *> &Blocks,
           Instruction *Start, Instruction *End,
           bool StartsAtHeader, bool EndsAtBackedge);

  /// Get the starting point of the path, guaranteed to be either the loop
  /// header or an equivalence point.
  BasicBlock *startBlock() const { return Blocks.front(); }
  Instruction *startInst() const { return Start; }

  /// Get the the ending point of the path, guaranteed to be either an
  /// equivalence point or a backedge.
  BasicBlock *endBlock() const { return Blocks.back(); }
  Instruction *endInst() const { return End; }

  /// Iterators over the path's blocks.
  std::vector<BasicBlock *>::iterator begin() { return Blocks.begin(); }
  std::vector<BasicBlock *>::iterator end() { return Blocks.end(); }
  std::vector<BasicBlock *>::const_iterator cbegin() { return Blocks.cbegin(); }
  std::vector<BasicBlock *>::const_iterator cend() { return Blocks.cend(); }

  /// Return whether the path starts at the loop header or equivalence point.
  bool startsAtHeader() const { return StartsAtHeader; }

  /// Return whether the path ends at a backedge block or equivalence point.
  bool endsAtBackedge() const { return EndsAtBackedge; }

  void print(raw_ostream &O) const;
};

/// Analyze all paths within a loop nest
class EnumerateLoopPaths : public LoopPass {
private:
  /// All calculated paths for each analyzed loops.
  DenseMap<const Loop *, std::vector<LoopPath> > Paths;

  /// Information about the loop currently being analyzed
  BasicBlock *Header;
  SmallPtrSet<BasicBlock *, 4> Latches;

  /// Depth-first search traversal information for generating path objects.
  struct LoopDFSInfo {
  public:
    Instruction *Start;
    std::vector<BasicBlock *> PathBlocks;
    bool StartsAtHeader;
  };

  /// Run a depth-first search for paths in the loop starting at an instruction.
  /// Any paths found are added to the vector of paths, and new paths to explore
  /// are added to the search queue.
  void loopDFS(Instruction *,
               LoopDFSInfo &,
               std::vector<LoopPath> &,
               std::queue<Instruction *> &);

public:
  static char ID;

  EnumerateLoopPaths() : LoopPass(ID) {}

  /// Pass interface implementation.
  void getAnalysisUsage(AnalysisUsage &) const override;
  bool runOnLoop(Loop *, LPPassManager &) override;

  bool hasPaths(const Loop *L) const { return Paths.count(L); }
  const std::vector<LoopPath> &getPaths(const Loop *L) const;
};

}

#endif

