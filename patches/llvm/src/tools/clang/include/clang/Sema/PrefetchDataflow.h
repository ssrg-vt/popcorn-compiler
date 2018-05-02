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

#ifndef _AST_PREFETCHDATAFLOW_H
#define _AST_PREFETCHDATAFLOW_H

#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CFGStmtMap.h"
#include "clang/AST/ParentMap.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <memory>

namespace clang {

/// Data structures for containing symbolic execution values.
typedef llvm::SmallPtrSet<Expr *, 1> ExprList;
typedef llvm::DenseMap<const VarDecl *, ExprList> SymbolicValueMap;
typedef std::pair<const VarDecl *, ExprList> SymbolicValuePair;
typedef llvm::SmallPtrSet<const CFGBlock *, 32> CFGBlockSet;

/// Class which runs dataflow analysis over the specified statement.  Tracks
/// the value of a given set of variables as they change throughout the
/// statement.
class PrefetchDataflow {
public:
  typedef llvm::SmallPtrSet<const VarDecl *, 8> VarSet;

  /// A lot of boilerplate so we can embed analysis into data structures like
  /// llvm::DenseMap.  Required because objects use std::unique_ptrs.
  PrefetchDataflow();
  PrefetchDataflow(ASTContext *Ctx);
  PrefetchDataflow(const PrefetchDataflow &RHS);

  PrefetchDataflow &operator=(const PrefetchDataflow &RHS);

  /// Run dataflow analysis over the statement specified at build time.
  void runDataflow(Stmt *S, VarSet &VarsToTrack);

  /// Get the value of a variable at a specific use in a statement, or nullptr
  /// if analysis could not calculate its value.
  Expr *getVariableValue(VarDecl *Var, const Stmt *Use) const;

  /// Reset any previous analysis.
  void reset();

  void print(llvm::raw_ostream &O) const;
  void dump() const;

private:
  ASTContext *Ctx;
  Stmt *S;

  /// Data used in analysis/retrieving results
  std::unique_ptr<CFG> TheCFG;
  std::unique_ptr<ParentMap> PMap;
  std::unique_ptr<CFGStmtMap> StmtToBlock;

  /// Analysis results -- keep an expression used to calculate a variable's
  /// value for each control flow block.
  typedef std::pair<const CFGBlock *, SymbolicValueMap> BlockValuesPair;
  typedef llvm::DenseMap<const CFGBlock *, SymbolicValueMap> BlockValuesMap;
  BlockValuesMap VarValues;
};

}

#endif

