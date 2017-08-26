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
//  - Header to backedge block, with no equivalence points on the path
//  - Header to block with equivalence point
//  - Block with equivalence point to block with equivalence point
//  - Block with equivalence point to backedge block
//
// Note that backedge blocks may or may not also be exit blocks.
//
//===----------------------------------------------------------------------===//

#include <queue>
#include "llvm/Analysis/LoopPaths.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "looppaths"

LoopPath::LoopPath(const std::vector<BasicBlock *> &Blocks,
                   Instruction *Start, Instruction *End,
                   bool StartsAtHeader, bool EndsAtBackedge)
  : Blocks(Blocks), Start(Start), End(End), StartsAtHeader(StartsAtHeader),
    EndsAtBackedge(EndsAtBackedge) {
  assert(Blocks.size() && "Trivial path");
  assert(Start && Start->getParent() == Blocks.front() &&
         "Invalid starting instruction");
  assert(End && End->getParent() == Blocks.back() &&
         "Invalid ending instruction");
}

void LoopPath::print(raw_ostream &O) const {
  O << "  Path with " << std::to_string(Blocks.size()) << " block(s)\n";
  O << "  Start:"; Start->print(O); O << "\n";
  O << "  End:"; End->print(O); O << "\n";
  O << "  Blocks:\n";
  for(auto Block : Blocks)
    O << "    " << Block->getName() << "\n";
}

void EnumerateLoopPaths::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

/// Search over the instructions in a basic block (starting at I) for
/// equivalence points.  Return an equivalence point if found, or null
/// otherwise.
static Instruction *hasEquivalencePoint(Instruction *I) {
  if(!I) return nullptr;
  for(BasicBlock::iterator it(I), e = I->getParent()->end(); it != e; ++it)
    if(isa<CallInst>(it) || isa<InvokeInst>(it)) return it;
  return nullptr;
}

void EnumerateLoopPaths::loopDFS(Instruction *I,
                                 LoopDFSInfo &DFSI,
                                 std::vector<LoopPath> &CurPaths,
                                 std::queue<Instruction *> &NewPaths) {
  Instruction *EqPoint;
  BasicBlock *BB = I->getParent();
  DFSI.PathBlocks.push_back(BB);

  if((EqPoint = hasEquivalencePoint(I))) {
    CurPaths.emplace_back(DFSI.PathBlocks, DFSI.Start, EqPoint,
                          DFSI.StartsAtHeader, false);
    NewPaths.push(EqPoint->getNextNode());
    DEBUG(dbgs() << "Found path that ends at an equivalence point:\n";
          CurPaths.back().print(dbgs()));
  }
  else if(Latches.count(BB)) {
    CurPaths.emplace_back(DFSI.PathBlocks, DFSI.Start, BB->getTerminator(),
                          DFSI.StartsAtHeader, true);
    DEBUG(dbgs() << "Found path that ends at a backedge:\n";
          CurPaths.back().print(dbgs()));
  }
  else {
    // TODO if successor is in child loop, glob all child loop blocks together
    // as a virtual path node and only search successors of the loop
    for(auto SuccBB : successors(BB))
      loopDFS(&SuccBB->front(), DFSI, CurPaths, NewPaths);
  }

  DFSI.PathBlocks.pop_back();
}

bool EnumerateLoopPaths::runOnLoop(Loop *L, LPPassManager &LPPM) {
  std::vector<LoopPath> &CurPaths = Paths[L];
  std::queue<Instruction *> NewPaths;
  SmallVector<BasicBlock *, 4> LatchVec;
  LoopDFSInfo DFSI;

  DEBUG(dbgs() << "Enumerating paths for "; L->print(dbgs()));

  Header = L->getHeader();
  L->getLoopLatches(LatchVec);
  for(auto L : LatchVec) Latches.insert(L);

  assert(CurPaths.size() == 0 && "Re-processing loop?");
  assert(Latches.size() && "Not a loop? (No backedges)");

  DFSI.Start = &Header->front();
  DFSI.StartsAtHeader = true;
  loopDFS(DFSI.Start, DFSI, CurPaths, NewPaths);
  assert(DFSI.PathBlocks.size() == 0 && "Invalid traversal");

  DFSI.StartsAtHeader = false;
  while(!NewPaths.empty()) {
    DFSI.Start = NewPaths.front();
    NewPaths.pop();
    loopDFS(DFSI.Start, DFSI, CurPaths, NewPaths);
    assert(DFSI.PathBlocks.size() == 0 && "Invalid traversal");
  }

  return false;
}

const std::vector<LoopPath> &EnumerateLoopPaths::getPaths(const Loop *L) const {
  assert(hasPaths(L) && "No paths for loop");
  return Paths.find(L)->second;
}

char EnumerateLoopPaths::ID = 0;
INITIALIZE_PASS(EnumerateLoopPaths, "looppaths",
                "Enumerate paths between equivalence points in loops",
                false, true)

namespace llvm {
  LoopPass *createEnumerateLoopPathsPass() { return new EnumerateLoopPaths(); }
}

