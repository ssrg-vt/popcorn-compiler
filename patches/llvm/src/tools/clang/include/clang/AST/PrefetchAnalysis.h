//===- PrefetchAnalysis.h - Prefetching Analysis for Statements ---*- C++ --*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interface for prefetching analysis over structured
// blocks.  The analysis traverses the AST to determine how arrays are accessed
// in structured blocks and generates expressions defining ranges of elements
// accessed inside arrays.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PREFETCHANALYSIS_H
#define LLVM_CLANG_AST_PREFETCHANALYSIS_H

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

namespace clang {

class ASTContext;
class InductionVariable;

/// A range of memory to be prefetched.
class PrefetchRange {
public:
  /// Access type for array.
  enum Type { Read, Write };

  PrefetchRange(enum Type Ty, VarDecl *Array, Expr *Start, Expr *End)
    : Ty(Ty), Array(Array), Start(Start), End(End) {}

  enum Type getType() const { return Ty; }
  VarDecl *getArray() const { return Array; }
  Expr *getStart() const { return Start; }
  Expr *getEnd() const { return End; }
  void setType(enum Type Ty) { this->Ty = Ty; }
  void setArray(VarDecl *Array) { this->Array = Array; }
  void setStart(Expr *Start) { this->Start = Start; }
  void setEnd(Expr *Start) { this->End = End; }

  // TODO print & dump
  const char *getTypeName() const {
    switch(Ty) {
    case Read: return "read";
    case Write: return "write";
    default: return "unknown";
    }
  }

private:
  enum Type Ty;
  VarDecl *Array;
  Expr *Start, *End;
};

class PrefetchAnalysis {
public:
  typedef std::shared_ptr<InductionVariable> InductionVariablePtr;
  typedef llvm::DenseMap<VarDecl *, InductionVariablePtr> IVMap;
  typedef std::pair<VarDecl *, InductionVariablePtr> IVPair;

  /// How a statement should be modified.
  struct ExprModifier {
    enum Type { Add, Sub, Mul, Div, None, Unknown };
    void ClassifyModifier(Expr *E, const ASTContext &Ctx);
    enum Type getType() const { return Ty; }
    const llvm::APInt &getVal() const { return Val; }
  private:
    enum Type Ty;
    llvm::APInt Val;
  };

  /// Default constructor, really only defined to enable storage in a DenseMap.
  PrefetchAnalysis() : Ctx(nullptr), S(nullptr) {}

  /// Construct a new prefetch analysis object to analyze a statement.  Doesn't
  /// run the analysis.
  PrefetchAnalysis(ASTContext *Ctx, Stmt *S) : Ctx(Ctx), S(S) {}

  /// Ignore a set of variables during access analysis.  In other words, ignore
  /// memory accesses which use these variables as their base.
  void ignoreVars(const llvm::SmallPtrSet<VarDecl *, 4> &Ignore)
  { this->Ignore = Ignore; }

  /// Analyze the statement.
  void analyzeStmt();

  /// Get prefetch ranges discovered by analysis.
  const SmallVector<PrefetchRange, 8> &getArraysToPrefetch() const
  { return ToPrefetch; }

  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }

private:
  ASTContext *Ctx;
  Stmt *S;
  llvm::SmallPtrSet<VarDecl *, 4> Ignore;

  llvm::SmallVector<PrefetchRange, 8> ToPrefetch;

  /// Analyze individual types of statements.
  void analyzeForStmt();

  /// Clean up prefetch analysis by merging overlapping or duplicate accesses.
  void mergeArrayAccesses();

  /// Remove trivial array accesses.
  void pruneEmptyArrayAccesses();

  /// Reconstruct expressions with induction variable uses replaced by their
  /// upper (Upper = true) & lower bounds (Upper = false).
  Expr *cloneWithIV(Expr *E, const IVMap &IVs, bool Upper) const;

  /// Clone an expression, but don't replace any variables.
  Expr *clone(Expr *E) const;

  /// Clone a binary operation.
  Expr *cloneBinaryOperation(BinaryOperator *B,
                             const IVMap &IVs,
                             bool Upper) const;

  /// Clone a unary operation.
  Expr *cloneUnaryOperation(UnaryOperator *U,
                            const IVMap &IVs,
                            bool Upper) const;

  /// Clone a declaration reference.  If it's an induction variable, replace
  /// with the bound specified by the Upper flag.
  Expr *cloneDeclRefExpr(DeclRefExpr *D, const IVMap &IVs, bool Upper) const;

  /// Clone an implicit cast.
  Expr *cloneImplicitCastExpr(ImplicitCastExpr *E,
                              const IVMap &IVs,
                              bool Upper) const;

  /// Clone an integer literal.
  Expr *cloneIntegerLiteral(IntegerLiteral *L,
                            const IVMap &IVs,
                            bool Upper) const;

  /// Modify an expression according to a configuration.
  Expr *modifyExpr(Expr *E, const ExprModifier &Mod) const;
};

} // end namespace clang

#endif

