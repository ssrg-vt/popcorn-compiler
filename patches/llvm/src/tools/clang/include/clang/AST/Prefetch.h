//===--- Prefetch.h - Prefetching classes for Popcorn Linux -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines classes for enabling prefetching analysis & instrumentation
// on Popcorn Linux.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PREFETCH_H
#define LLVM_CLANG_AST_PREFETCH_H

namespace clang {

/// Statements for which prefetching analysis & instrumentation can be performed
/// should inherit this class.
class Prefetchable {
protected:
  /// \brief Whether prefetching has been enabled for the statement.
  bool PrefetchEnabled;

public:
  Prefetchable(bool PrefetchEnabled = false)
    : PrefetchEnabled(PrefetchEnabled) {}

  void setPrefetchEnabled(bool Enable) { PrefetchEnabled = Enable; }
  bool prefetchEnabled() const { return PrefetchEnabled; }
};

} // end namespace clang

#endif

