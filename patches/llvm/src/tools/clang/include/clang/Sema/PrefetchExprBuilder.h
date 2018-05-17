//===- PrefetchExprBuilder.h - Prefetching expression builder -----*- C++ --*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a set of utilities for building expressions for
// prefetching.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PREFETCHEXPRBUILDER_H
#define LLVM_CLANG_AST_PREFETCHEXPRBUILDER_H

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "llvm/ADT/DenseMap.h"

namespace clang {

class ASTContext;

typedef std::pair<VarDecl *, Expr *> ReplacePair;
typedef llvm::DenseMap<VarDecl *, Expr *> ReplaceMap;

/// Utilities for compairing expressions by value.
namespace PrefetchExprEquality {

/// Compare two expressions by value to see if they're equal.
bool exprEqual(const Expr *A, const Expr *B);

}

/// Utilities for building expressions.
namespace PrefetchExprBuilder {

/// Information describing how a statement should be modified.
struct Modifier {
  enum Type { Add, Sub, Mul, Div, None, Unknown };
  void ClassifyModifier(const Expr *E, const ASTContext *Ctx);
  enum Type getType() const { return Ty; }
  const llvm::APInt &getVal() const { return Val; }
private:
  enum Type Ty;
  llvm::APInt Val;
};

/// Information needed for building expressions.
struct BuildInfo {
public:
  BuildInfo(ASTContext *Ctx, ReplaceMap &VarReplace, bool dumpInColor)
    : Ctx(Ctx), VarReplace(VarReplace), dumpInColor(dumpInColor) {}

  ASTContext *Ctx;
  ReplaceMap &VarReplace;
  llvm::SmallPtrSet<VarDecl *, 8> SeenVars;
  bool dumpInColor;

  void reset() {
    VarReplace.clear();
    SeenVars.clear();
  }
};

/// Reconstruct expressions with variables replaced by user-supplied
/// expressions (in Info.VarReplacements).
Expr *cloneWithReplacement(Expr *E, BuildInfo &Info);

/// Clone an expression, but don't replace any variables.
Expr *clone(Expr *E, ASTContext *Ctx);

/// Clone a binary operation.
Expr *cloneBinaryOperator(BinaryOperator *B, BuildInfo &Info);

/// Clone a unary operation.
Expr *cloneUnaryOperator(UnaryOperator *U, BuildInfo &Info);

/// Clone an array subscript.
Expr *cloneArraySubscriptExpr(ArraySubscriptExpr *A, BuildInfo &Info);

/// Clone a declaration reference.  If it's an induction variable, replace
/// with the bound specified by the Upper flag.
Expr *cloneDeclRefExpr(DeclRefExpr *D, BuildInfo &Info);

/// Clone an implicit cast.
Expr *cloneImplicitCastExpr(ImplicitCastExpr *E, BuildInfo &Info);

/// Clone an integer literal.
Expr *cloneIntegerLiteral(IntegerLiteral *L, BuildInfo &Info);

/// Modify an expression according to a configuration.
Expr *cloneAndModifyExpr(Expr *E, const Modifier &Mod, ASTContext *Ctx);

} // end namespace PrefetchExprBuilder

} // end namespace clang

#endif

