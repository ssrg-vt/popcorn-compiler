//=- PrefetchDataflow.cpp - Dataflow analysis for prefetching ------------*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the dataflow of expressions as required for prefetching
// analysis.  This is required to correctly discover how variables are used in
// memory accesses in order to construct memory access ranges.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/ParentMap.h"
#include "clang/Sema/PrefetchDataflow.h"
#include "clang/Sema/PrefetchExprBuilder.h"
#include "llvm/Support/Debug.h"
#include <queue>

using namespace clang;

//===----------------------------------------------------------------------===//
// Common utilities
//

/// Return whether a statement is a loop construct.
static inline bool isLoopStmt(const Stmt *S) {
  if(isa<DoStmt>(S) || isa<ForStmt>(S) || isa<WhileStmt>(S)) return true;
  else return false;
}

/// Return whether a binary operator is an assign expression.
static inline bool isAssign(const BinaryOperator *B) {
  if(B->getOpcode() == BO_Assign) return true;
  return false;
}

/// Return whether a binary operator is an operation + assign expression.
static inline bool isMathAssign(const BinaryOperator *B) {
  switch(B->getOpcode()) {
  case BO_MulAssign: case BO_DivAssign: case BO_RemAssign:
  case BO_AddAssign: case BO_SubAssign: case BO_ShlAssign:
  case BO_ShrAssign: case BO_AndAssign: case BO_XorAssign:
  case BO_OrAssign:
    return true;
  default: return false;
  }
}

/// Return the variable referenced by the expression E, or a nullptr if none
/// were referenced.
static const VarDecl *getVariableIfReference(const Expr *E) {
  const DeclRefExpr *DR;
  const VarDecl *VD;

  DR = dyn_cast<DeclRefExpr>(E->IgnoreImpCasts());
  if(!DR) return nullptr;
  VD = dyn_cast<VarDecl>(DR->getDecl());
  return VD;
}

//===----------------------------------------------------------------------===//
// Expression dataflow API
//

PrefetchDataflow::PrefetchDataflow() : Ctx(nullptr) {}
PrefetchDataflow::PrefetchDataflow(ASTContext *Ctx) : Ctx(Ctx) {}
PrefetchDataflow::PrefetchDataflow(const PrefetchDataflow &RHS)
  : Ctx(RHS.Ctx) {}

PrefetchDataflow &PrefetchDataflow::operator=(const PrefetchDataflow &RHS) {
  Ctx = RHS.Ctx;
  return *this;
}

void PrefetchDataflow::runDataflow(Stmt *S, VarSet &VarsToTrack) {
  Expr *Clone;
  const DeclStmt *DS;
  const BinaryOperator *BO;
  const VarDecl *VD;
  const CFGBlock *Block;
  CFGBlock::const_succ_iterator Succ, SE;
  Optional<CFGStmt> StmtNode;
  CFGBlockSet Seen;
  std::queue<const CFGBlock *> Work;
  BlockValuesMap::iterator BVIt;

  if(!VarsToTrack.size()) return;
  this->S = S;
  TheCFG = CFG::buildCFG(nullptr, S, Ctx, CFG::BuildOptions());

  Work.push(&TheCFG->getEntry());
  while(!Work.empty()) {
    Block = Work.front();
    Work.pop();
    Seen.insert(Block);

    // Find assignment operations within the block.  Because of the forward
    // dataflow algorithm, predecessors should have already pushed dataflow
    // expressions, if any, to this block.
    SymbolicValueMap &CurMap = VarValues[Block];
    for(auto &Elem : *Block) {
      StmtNode = Elem.getAs<CFGStmt>();
      if(!StmtNode) continue;

      DS = dyn_cast<DeclStmt>(StmtNode->getStmt());
      if(DS) {
        for(auto D : DS->getDeclGroup()) {
          VD = dyn_cast<VarDecl>(D);
          if(VD && VD->hasInit() && VarsToTrack.count(VD)) {
            // TODO unfortunately CFG & accompanying classes expose statements
            // & expressions with const qualifiers.  But, we *really* need them
            // to not be const qualified in order to clone them (in particular,
            // cloning DeclRefExprs is nasty).
            Clone = PrefetchExprBuilder::clone((Expr *)VD->getInit(), Ctx);
            if(Clone) CurMap[VD].insert(Clone);
          }
        }
        continue;
      }

      BO = dyn_cast<BinaryOperator>(StmtNode->getStmt());
      if(!BO) continue;

      // Check for assignment operation to a relevant variable.  If we had
      // previous expression(s) describing the variable's value the assignment
      // overwrites them.
      if(isAssign(BO)) {
        VD = getVariableIfReference(BO->getLHS());
        if(VD && VarsToTrack.count(VD)) {
          ExprList &VarExprs = CurMap[VD];
          VarExprs.clear();
          Clone = PrefetchExprBuilder::clone(BO->getRHS(), Ctx);
          if(Clone) VarExprs.insert(Clone);
        }
      }
      // TODO we currently don't handle math + assign operations, so the
      // dataflow analysis clamps to 'unknown' (i.e., no expressions).
      else if(isMathAssign(BO)) {
        VD = getVariableIfReference(BO->getLHS());
        if(VD && VarsToTrack.count(VD)) CurMap.erase(VD);
      }
    }

    // Push dataflow expressions to successors.
    for(Succ = Block->succ_begin(), SE = Block->succ_end();
        Succ != SE; Succ++) {
      if(!Succ->isReachable()) continue;
      SymbolicValueMap &SuccMap = VarValues[*Succ];
      for(auto &Pair : CurMap) {
        ExprList &VarExprs = SuccMap[Pair.first];
        for(auto Expr : Pair.second) VarExprs.insert(Expr);
      }
    }

    // TODO do we need to deal with sub-scopes, e.g., loops, here?

    // Add unvisited CFG blocks to the work queue.
    for(Succ = Block->succ_begin(), SE = Block->succ_end();
        Succ != SE; Succ++) {
      if(Succ->isReachable() && !Seen.count(Succ->getReachableBlock()))
        Work.push(Succ->getReachableBlock());
    }
  }

  // Make it easier to look up analysis for statements
  PMap.reset(new ParentMap(S));
  PMap->addStmt(S);
  StmtToBlock.reset(CFGStmtMap::Build(TheCFG.get(), PMap.get()));
}

Expr *PrefetchDataflow::getVariableValue(VarDecl *Var, const Stmt *Use) const {
  // TODO
  return nullptr;
}

void PrefetchDataflow::reset() {
  S = nullptr;
  StmtToBlock.reset();
  PMap.reset();
  TheCFG.reset();
  VarValues.clear();
}

void PrefetchDataflow::print(llvm::raw_ostream &O) const {
  if(!S || !TheCFG || !VarValues.size()) {
    O << "<Prefetch Dataflow> No analysis -- did you run with runDataflow()?\n";
    return;
  }

  O << "<Prefetch Dataflow> Analysis results:\n";
  PrintingPolicy PP(Ctx->getLangOpts());
  for(auto Node : *TheCFG) {
    Node->print(O, TheCFG.get(), Ctx->getLangOpts(), true);
    O << "\n";
    BlockValuesMap::const_iterator BVIt = VarValues.find(Node);
    if(BVIt != VarValues.end()) {
      for(auto VarValPair : BVIt->second) {
        O << "Values for '" << VarValPair.first->getName() << "':\n";
        for(auto E : VarValPair.second) {
          E->printPretty(O, nullptr, PP);
          O << "\n";
        }
      }
    }
    else O << "\n-> No dataflow values <-\n";
  }
}

void PrefetchDataflow::dump() const { print(llvm::dbgs()); }

