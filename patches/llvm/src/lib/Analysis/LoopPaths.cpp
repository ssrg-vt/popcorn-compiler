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

void LoopPathUtilities::populateLoopNest(Loop *L, LoopNest &Nest) {
  std::queue<Loop *> ToVisit;
  Nest.clear();
  Nest.insert(L);
  ToVisit.push(L);

  while(ToVisit.size()) {
    const Loop *Sub = ToVisit.front();
    ToVisit.pop();
    for(auto L : Sub->getSubLoops()) {
      Nest.insert(L);
      ToVisit.push(L);
    }
  }
}

void LoopPathUtilities::getSubBlocks(Loop *L, BlockSet &SubBlocks) {
  SubBlocks.clear();
  LoopNest Nest;

  for(auto Sub : L->getSubLoops()) {
    populateLoopNest(Sub, Nest);
    for(auto Nested : Nest)
      for(auto BB : Nested->getBlocks())
        SubBlocks.insert(BB);
  }
}

LoopPath::LoopPath(const std::vector<PathNode> &NodeVector,
                   const Instruction *Start, const Instruction *End,
                   bool StartsAtHeader, bool EndsAtBackedge)
  : Start(Start), End(End), StartsAtHeader(StartsAtHeader),
    EndsAtBackedge(EndsAtBackedge) {
  assert(NodeVector.size() && "Trivial path");
  assert(Start && Start->getParent() == NodeVector.front().getBlock() &&
         "Invalid starting instruction");
  assert(End && End->getParent() == NodeVector.back().getBlock() &&
         "Invalid ending instruction");

  for(auto Node : NodeVector) Nodes.insert(Node);
}

std::string LoopPath::toString() const {
  std::string buf = "Path with " + std::to_string(Nodes.size()) + " nodes(s)\n";

  buf += "  Start: ";
  if(Start->hasName()) buf += Start->getName();
  else buf += "<unnamed instruction>";
  buf += "\n";

  buf += "  End: ";
  if(End->hasName()) buf += End->getName();
  else buf += "<unnamed instruction>";
  buf += "\n";

  buf += "  Nodes:\n";
  for(auto Node : Nodes) {
    buf += "    ";
    const BasicBlock *BB = Node.getBlock();
    if(BB->hasName()) buf += BB->getName();
    else buf += "<unnamed block>";
    if(Node.isSubLoopExit()) buf += " (sub-loop exit)";
    buf += "\n";
  }

  return buf;
}

void LoopPath::print(raw_ostream &O) const {
  O << "    Path with " << std::to_string(Nodes.size()) << " nodes(s)\n";
  O << "    Start:"; Start->print(O); O << "\n";
  O << "    End:"; End->print(O); O << "\n";
  O << "    Nodes:\n";
  for(auto Node : Nodes) {
    const BasicBlock *BB = Node.getBlock();
    if(BB->hasName()) O << "      " << BB->getName();
    else O << "      <unnamed block>";
    if(Node.isSubLoopExit()) O << " (sub-loop exit)";
    O << "\n";
  }
}

void EnumerateLoopPaths::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  AU.setPreservesAll();
}

/// Search over the instructions in a basic block (starting at I) for
/// equivalence points.  Return an equivalence point if found, or null
/// otherwise.
static const Instruction *hasEquivalencePoint(const Instruction *I) {
  if(!I) return nullptr;
  for(BasicBlock::const_iterator it(I), e = I->getParent()->end();
      it != e; ++it)
    if(Popcorn::isEquivalencePoint(it)) return it;
  return nullptr;
}

/// Add a value to a list if it's not already contained in the list.  This is
/// used to unique new instructions at which to start searches, as multiple
/// paths may end at the same equivalence point (but we don't need to search
/// it multiple times).
static inline void pushIfNotPresent(const Instruction *I,
                                    std::list<const Instruction *> &List) {
  if(std::find(List.begin(), List.end(), I) == List.end()) List.push_back(I);
}

// TODO this needs to be converted to iteration rather than recursion

void
EnumerateLoopPaths::getSubLoopSuccessors(const BasicBlock *Successor,
                                   std::vector<const Instruction *> &EqPoint,
                                   std::vector<const Instruction *> &Spanning) {
  const Instruction *Term;
  const Loop *SubLoop;
  SmallVector<BasicBlock *, 4> ExitBlocks;

  EqPoint.clear();
  Spanning.clear();
  assert(CurLoop->contains(Successor) && SubLoopBlocks.count(Successor) &&
         "Invalid sub-loop block");

  SubLoop = LI->getLoopFor(Successor);
  SubLoop->getExitingBlocks(ExitBlocks);
  for(auto Exit : ExitBlocks) {
    Term = Exit->getTerminator();
    if(HasSpPath[SubLoop][Exit]) Spanning.push_back(Term);
    if(HasEqPointPath[SubLoop][Exit]) EqPoint.push_back(Term);
  }
}

static inline void printNewPath(raw_ostream &O, const LoopPath &Path) {
  O << "Found path that start at ";
  if(Path.startsAtHeader()) O << "the header";
  else O << "an equivalence point";
  O << " and ends at ";
  if(Path.endsAtBackedge()) O << "a loop backedge";
  else O << "an equivalence point";
  Path.print(O);
}

void EnumerateLoopPaths::loopDFS(const Instruction *I,
                                 LoopDFSInfo &DFSI,
                                 std::vector<LoopPath> &CurPaths,
                                 std::list<const Instruction *> &NewPaths) {
  const Instruction *EqPoint;
  const BasicBlock *BB = I->getParent(), *PathBlock;
  std::vector<const Instruction *> EqPointInsts, SpanningInsts;

  if(!SubLoopBlocks.count(BB)) {
    DFSI.PathNodes.emplace_back(BB, false);
    if((EqPoint = hasEquivalencePoint(I))) {
      CurPaths.emplace_back(DFSI.PathNodes, DFSI.Start, EqPoint,
                            DFSI.StartsAtHeader, false);
      for(auto Node : DFSI.PathNodes) {
        PathBlock = Node.getBlock();
        if(!SubLoopBlocks.count(PathBlock))
          HasEqPointPath[CurLoop][PathBlock] = true;
      }

      // Add instruction after equivalence point (or at start of successor
      // basic blocks if EqPoint is the last instruction in its block) as start
      // of new equivalence point path to be searched.
      if(!EqPoint->isTerminator())
        pushIfNotPresent(EqPoint->getNextNode(), NewPaths);
      else {
        for(auto Succ : successors(BB)) {
          if(!CurLoop->contains(Succ) || // Skip exit blocks & latches
             Succ == CurLoop->getHeader()) continue;
          else if(!SubLoopBlocks.count(Succ)) // Successor is in same outer loop
            pushIfNotPresent(&Succ->front(), NewPaths);
          else { // Successor is in sub-loop
            getSubLoopSuccessors(Succ, EqPointInsts, SpanningInsts);
            for(auto SLE : EqPointInsts) pushIfNotPresent(SLE, NewPaths);
            for(auto SLE : SpanningInsts)
              loopDFS(SLE, DFSI, CurPaths, NewPaths);
          }
        }
      }

      DEBUG(printNewPath(dbgs(), CurPaths.back()));
    }
    else if(Latches.count(BB)) {
      CurPaths.emplace_back(DFSI.PathNodes, DFSI.Start, BB->getTerminator(),
                            DFSI.StartsAtHeader, true);
      if(DFSI.StartsAtHeader) {
        for(auto Node : DFSI.PathNodes) {
          PathBlock = Node.getBlock();
          if(!SubLoopBlocks.count(PathBlock))
            HasSpPath[CurLoop][PathBlock] = true;
        }
      }
      else {
        for(auto Node : DFSI.PathNodes) {
          PathBlock = Node.getBlock();
          if(!SubLoopBlocks.count(PathBlock))
            HasEqPointPath[CurLoop][PathBlock] = true;
        }
      }

      DEBUG(printNewPath(dbgs(), CurPaths.back()));
    }
    else {
      for(auto Succ : successors(BB)) {
        if(!CurLoop->contains(Succ)) continue;
        else if(!SubLoopBlocks.count(Succ))
          loopDFS(&Succ->front(), DFSI, CurPaths, NewPaths);
        else {
          getSubLoopSuccessors(Succ, EqPointInsts, SpanningInsts);
          for(auto SLE : EqPointInsts) {
            // Rather than stopping the path at the equivalence point inside
            // of a sub-loop, stop it at the end of the current block
            // TODO this can create duplicates for a path that reaches a
            // sub-loop with multiple exiting blocks, but the analysis in
            // MigrationPoints doesn't care about paths that don't end at a
            // backedge anyway
            CurPaths.emplace_back(DFSI.PathNodes, DFSI.Start,
                                  BB->getTerminator(),
                                  DFSI.StartsAtHeader, false);
            pushIfNotPresent(SLE, NewPaths);
            DEBUG(printNewPath(dbgs(), CurPaths.back()));
          }
          for(auto SLE : SpanningInsts) loopDFS(SLE, DFSI, CurPaths, NewPaths);
        }
      }
    }
    DFSI.PathNodes.pop_back();
  }
  else {
    // This is a sub-loop block; we only want to explore successors who are not
    // contained in this sub-loop but are still contained in the current loop.
    DFSI.PathNodes.emplace_back(BB, true);
    const Loop *WeedOutLoop = LI->getLoopFor(BB);
    for(auto Succ : successors(BB)) {
      if(WeedOutLoop->contains(Succ) || !CurLoop->contains(Succ)) continue;
      else if(!SubLoopBlocks.count(Succ))
        loopDFS(&Succ->front(), DFSI, CurPaths, NewPaths);
      else {
        getSubLoopSuccessors(Succ, EqPointInsts, SpanningInsts);
        for(auto SLE : EqPointInsts) {
          // TODO this can create duplicates for a path that reaches a sub-loop
          // with multiple exiting blocks, but the analysis in MigrationPoints
          // doesn't care about paths that don't end at a backedge anyway
          CurPaths.emplace_back(DFSI.PathNodes, DFSI.Start,
                                BB->getTerminator(),
                                DFSI.StartsAtHeader, false);
          DEBUG(printNewPath(dbgs(), CurPaths.back()));
          pushIfNotPresent(SLE, NewPaths);
        }
        for(auto SLE: SpanningInsts) loopDFS(SLE, DFSI, CurPaths, NewPaths);
      }
    }
    DFSI.PathNodes.pop_back();
  }
}

void EnumerateLoopPaths::analyzeLoop(Loop *L, std::vector<LoopPath> &CurPaths) {
  std::list<const Instruction *> NewPaths;
  SmallVector<BasicBlock *, 4> LatchVec;
  LoopDFSInfo DFSI;

  CurPaths.clear();
  HasSpPath[L].clear();
  HasEqPointPath[L].clear();

  DEBUG(
    DebugLoc DL(L->getStartLoc());
    dbgs() << "Enumerating paths";
    if(DL) {
      dbgs() << " for loop at ";
      DL.print(dbgs());
    }
    dbgs() << ": "; L->dump();
  );

  // Store information about the current loop, it's backedges, and sub-loops
  CurLoop = L;
  Latches.clear();
  L->getLoopLatches(LatchVec);
  for(auto L : LatchVec) Latches.insert(L);
  LoopPathUtilities::getSubBlocks(L, SubLoopBlocks);

  assert(Latches.size() && "No backedges, not a loop?");
  assert(!SubLoopBlocks.count(L->getHeader()) && "Header is in sub-loop?");

  DFSI.Start = &L->getHeader()->front();
  DFSI.StartsAtHeader = true;
  loopDFS(DFSI.Start, DFSI, CurPaths, NewPaths);
  assert(DFSI.PathNodes.size() == 0 && "Invalid traversal");

  DFSI.StartsAtHeader = false;
  while(!NewPaths.empty()) {
    DFSI.Start = NewPaths.front();
    NewPaths.pop_front();
    loopDFS(DFSI.Start, DFSI, CurPaths, NewPaths);
    assert(DFSI.PathNodes.size() == 0 && "Invalid traversal");
  }
}

bool EnumerateLoopPaths::runOnFunction(Function &F) {
  DEBUG(dbgs() << "\n********** ENUMERATE LOOP PATHS **********\n"
               << "********** Function: " << F.getName() << "\n\n");

  Paths.clear();
  LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  std::vector<LoopNest> Nests;

  // Discover all loop nests.
  for(LoopInfo::iterator I = LI->begin(), E = LI->end(); I != E; ++I) {
    if((*I)->getLoopDepth() != 1) continue;
    Nests.push_back(LoopNest());
    LoopPathUtilities::populateLoopNest(*I, Nests.back());
  }

  // Search all loops within all loop nests.
  for(auto Nest : Nests) {
    DEBUG(dbgs() << "Analyzing nest with " << std::to_string(Nest.size())
                 << " loops\n");

    for(auto L : Nest) {
      std::vector<LoopPath> &CurPaths = Paths[L];
      assert(CurPaths.size() == 0 && "Re-processing loop?");
      analyzeLoop(L, CurPaths);
    }
  }

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
  for(const LoopPath &Path : Paths.find(L)->second) P.push_back(&Path);
}

void
EnumerateLoopPaths::getBackedgePaths(const Loop *L,
                                     std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.endsAtBackedge()) P.push_back(&Path);
}

void
EnumerateLoopPaths::getBackedgePaths(const Loop *L,
                                     std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.endsAtBackedge()) P.insert(&Path);
}

void
EnumerateLoopPaths::getSpanningPaths(const Loop *L,
                                     std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.isSpanningPath()) P.push_back(&Path);
}

void
EnumerateLoopPaths::getSpanningPaths(const Loop *L,
                                     std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.isSpanningPath()) P.insert(&Path);
}

void
EnumerateLoopPaths::getEqPointPaths(const Loop *L,
                                    std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.isEqPointPath()) P.push_back(&Path);
}

void
EnumerateLoopPaths::getEqPointPaths(const Loop *L,
                                    std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.isEqPointPath()) P.insert(&Path);
}

void
EnumerateLoopPaths::getPathsThroughBlock(const Loop *L, BasicBlock *BB,
                                         std::vector<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  assert(L->contains(BB) && "Loop does not contain basic block");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.contains(BB)) P.push_back(&Path);
}

void
EnumerateLoopPaths::getPathsThroughBlock(const Loop *L, BasicBlock *BB,
                                         std::set<const LoopPath *> &P) const {
  assert(hasPaths(L) && "No paths for loop");
  assert(L->contains(BB) && "Loop does not contain basic block");
  P.clear();
  for(const LoopPath &Path : Paths.find(L)->second)
    if(Path.contains(BB)) P.insert(&Path);
}

bool EnumerateLoopPaths::spanningPathThroughBlock(const Loop *L,
                                                  const BasicBlock *BB) const {
  assert(hasPaths(L) && "No paths for loop");
  assert(L->contains(BB) && "Loop does not contain basic block");
  return HasSpPath.find(L)->second.find(BB)->second;
}

bool EnumerateLoopPaths::eqPointPathThroughBlock(const Loop *L,
                                                 const BasicBlock *BB) const {
  assert(hasPaths(L) && "No paths for loop");
  assert(L->contains(BB) && "Loop does not contain basic block");
  return HasEqPointPath.find(L)->second.find(BB)->second;
}

char EnumerateLoopPaths::ID = 0;
INITIALIZE_PASS_BEGIN(EnumerateLoopPaths, "looppaths",
                      "Enumerate paths in loops",
                      false, true)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(EnumerateLoopPaths, "looppaths",
                    "Enumerate paths in loops",
                    false, true)


namespace llvm {
  FunctionPass *createEnumerateLoopPathsPass()
  { return new EnumerateLoopPaths(); }
}

