/*
 * Provides a transaction-compatible assert.  Normal asserts do I/O, meaning
 * they'll abort the transaction & elide the assertion failure (unless it
 * happens again transactionally).  This version first exits the transaction
 * before executing the abort.
 *
 * Based on tsx_assert() in tsx-tools by Andi Kleen, available at
 * https://github.com/andikleen/tsx-tools
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/16/2017
 */

#ifndef _TSX_ASSERT_H
#define _TSX_ASSERT_H

#ifndef NDEBUG

#include <stdlib.h>
#include <stdio.h>

#define tsx_assert( cond ) \
  do { \
    if(!(cond)) { \
      while(in_transaction()) stop_transaction(); \
      fprintf(stderr, "Assert failure: %s:%d: " #cond, __FILE__, __LINE__); \
      abort(); \
    } \
  } while(0)

#endif /* NDEBUG */

#endif /* _TSX_ASSERT_H */

