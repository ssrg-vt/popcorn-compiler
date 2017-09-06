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
#include "llvm/Analysis/PopcornUtil.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "looppaths"

LoopPath::LoopPath(const std::vector<BasicBlock *> &BlockVector,
                   Instruction *Start, Instruction *End,
                   bool StartsAtHeader, bool EndsAtBackedge)
  : Start(Start), End(End), StartsAtHeader(StartsAtHeader),
    EndsAtBackedge(EndsAtBackedge) {
  assert(BlockVector.size() && "Trivial path");
  assert(Start && Start->getParent() == BlockVector.front() &&
         "Invalid starting instruction");
  assert(End && End->getParent() == BlockVector.back() &&
         "Invalid ending instruction");

  for(auto Block : BlockVector) Blocks.insert(Block);
}

std::string LoopPath::toString() const {
  std::string buf = "Path with " + std::to_string(Blocks.size()) +
                    " block(s)\n";

  buf += "  Start: ";
  if(Start->hasName()) buf += Start->getName();
  else buf += "<unnamed instruction>";
  buf += "\n";

  buf += "  End: ";
  if(End->hasName()) buf += End->getName();
  else buf += "<unnamed instruction>";
  buf += "\n";

  buf += "  Blocks:\n";
  for(auto Block : Blocks) {
    buf += "    ";
    if(Block->hasName()) buf += Block->getName();
    else buf += "<unnamed block>";
    buf += "\n";
  }

  return buf;
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
    if(Popcorn::isEquivalencePoint(it)) return it;
  return nullptr;
}

void EnumerateLoopPaths::loopDFS(Instruction *I,
                                 LoopDFSInfo &DFSI,
                                 std::vector<LoopPath> &CurPaths,
                                 std::queue<Instruction *> &NewPaths) {
  Instruction *EqPoint;
  BasicBlock *BB = I->getParent();
  SmallVector<BasicBlock *, 4> ExitBlocks;
  DenseMap<const BasicBlock *, const Loop *>::const_iterator SubIt;

  DFSI.PathBlocks.push_back(BB);
  if((EqPoint = hasEquivalencePoint(I))) {
    CurPaths.emplace_back(DFSI.PathBlocks, DFSI.Start, EqPoint,
                          DFSI.StartsAtHeader, false);
    NewPaths.push(EqPoint->getNextNode());

    DEBUG(
      dbgs() << "Found path that starts at ";
      if(DFSI.StartsAtHeader) dbgs() << "the header";
      else dbgs() << "an equivalence point";
      dbgs() << " and ends at an equivalence point:\n";
      CurPaths.back().print(dbgs())
    );
  }
  else if(Latches.count(BB)) {
    CurPaths.emplace_back(DFSI.PathBlocks, DFSI.Start, BB->getTerminator(),
                          DFSI.StartsAtHeader, true);

    DEBUG(
      dbgs() << "Found path that start at ";
      if(DFSI.StartsAtHeader) dbgs() << "the header";
      else dbgs() << "an equivalence point";
      dbgs() << " and ends at a backedge:\n";
      CurPaths.back().print(dbgs())
    );
  }
  else {
    for(auto SuccBB : successors(BB)) {
      if(CurLoop->contains(SuccBB)) {
        if((SubIt = SubLoopBlocks.find(SuccBB)) != SubLoopBlocks.end()) {
          // Conceptually, glob all sub-loop blocks into a single virtual node
          // on the path so we only need to search the sub-loop's exit blocks
          // TODO this needs some work...
          SubIt->second->getExitBlocks(ExitBlocks);
          for(auto SubExit : ExitBlocks) {
            if(CurLoop->contains(SubExit)) {
              assert(SubLoopBlocks.find(SubExit) == SubLoopBlocks.end() &&
                     "Not expecting sub-loop to exit to another sub-loop");
              loopDFS(&SubExit->front(), DFSI, CurPaths, NewPaths);
            }
          }
        }
        else loopDFS(&SuccBB->front(), DFSI, CurPaths, NewPaths);
      }
    }
  }
  DFSI.PathBlocks.pop_back();
}

void EnumerateLoopPaths::analyzeLoop(Loop *L, std::vector<LoopPath> &CurPaths) {
  std::queue<Instruction *> NewPaths;
  SmallVector<BasicBlock *, 4> LatchVec;
  LoopDFSInfo DFSI;
  CurPaths.clear();

  DEBUG(dbgs() << "Enumerating paths for "; L->print(dbgs()));

  // Store information about the current loop, it's backedges, and sub-loops
  CurLoop = L;
  Header = L->getHeader();
  Latches.clear();
  L->getLoopLatches(LatchVec);
  for(auto L : LatchVec) Latches.insert(L);
  SubLoopBlocks.clear();
  for(auto SubLoop : L->getSubLoopsVector())
    for(auto SubBB : SubLoop->getBlocks())
      SubLoopBlocks[SubBB] = SubLoop;

  assert(Latches.size() && "No backedges, not a loop?");

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
}

bool EnumerateLoopPaths::runOnLoop(Loop *L, LPPassManager &LPPM) {
  // We *should* be analyzing a loop for the first time
  std::vector<LoopPath> &CurPaths = Paths[L];
  assert(CurPaths.size() == 0 && "Re-processing loop?");
  analyzeLoop(L, CurPaths);
  return false;
}

void EnumerateLoopPaths::rerunOnLoop(Loop *L) {
  // We *should* be analyzing a loop for the 2+ time
  std::vector<LoopPath> &CurPaths = Paths[L];
  DEBUG(if(!CurPaths.size()) dbgs() << "  -> No previous analysis?\n");
  analyzeLoop(L, CurPaths);
}

void EnumerateLoopPaths::getPaths(const Loop *L,
                                  std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(auto Path : Paths.find(L)->second) P.push_back(&Path);
}

void
EnumerateLoopPaths::getBackedgePaths(const Loop *L,
                                     std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.endsAtBackedge()) P.push_back(&Path);
}

void
EnumerateLoopPaths::getBackedgePaths(const Loop *L,
                                     std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.endsAtBackedge()) P.insert(&Path);
}

void
EnumerateLoopPaths::getSpanningPaths(const Loop *L,
                                     std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.isSpanningPath()) P.push_back(&Path);
}

void
EnumerateLoopPaths::getSpanningPaths(const Loop *L,
                                     std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.isSpanningPath()) P.insert(&Path);
}

void
EnumerateLoopPaths::getEqPointPaths(const Loop *L,
                                    std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.isEqPointPath()) P.push_back(&Path);
}

void
EnumerateLoopPaths::getEqPointPaths(const Loop *L,
                                    std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.isEqPointPath()) P.insert(&Path);
}

void
EnumerateLoopPaths::getPathsThroughBlock(Loop *L, BasicBlock *BB,
                                         std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  assert(L->contains(BB) && "Loop does not contain basic block");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.contains(BB)) P.push_back(&Path);
}

void
EnumerateLoopPaths::getPathsThroughBlock(Loop *L, BasicBlock *BB,
                                         std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  assert(L->contains(BB) && "Loop does not contain basic block");
  P.clear();
  for(auto Path : Paths.find(L)->second)
    if(Path.contains(BB)) P.insert(&Path);
}

char EnumerateLoopPaths::ID = 0;
INITIALIZE_PASS(EnumerateLoopPaths, "looppaths",
                "Enumerate paths between equivalence points in loops",
                false, true)

namespace llvm {
  LoopPass *createEnumerateLoopPathsPass() { return new EnumerateLoopPaths(); }
}

