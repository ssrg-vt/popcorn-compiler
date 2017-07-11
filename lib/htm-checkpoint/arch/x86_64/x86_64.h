#ifndef _HTM_X86_H
#define _HTM_X86_H

#ifndef __RTM__
# error "Cannot use x86/Intel-TSX extensions for this target or CPU"
#endif

#include <immintrin.h>

/*
 * Start transaction & convert TSX-NI's xbegin instruction status codes into a
 * generic format.
 *
 * BEGIN:
 *   _XBEGIN_STARTED - successfully started the transaction
 *
 * TRANSIENT:
 *   _XABORT_RETRY - hardware thinks transaction may succeed on retry
 *   code == 0 - transaction aborted for other reason (e.g., page fault)
 *
 * CAPACITY:
 *   _XABORT_CAPACITY - hardware buffers reached capacity during transaction
 *
 * CONFLICT:
 *   _XABORT_CONFLICT - memory cache line conflict detected
 *
 * PERSISTENT:
 *   _XABORT_EXPLICIT - aborted by xabort instruction
 *
 * Abort reasons we should never experience:
 *   _XABORT_DEBUG - aborted due to debug trap
 *   _XABORT_NESTED - aborted in execution of nested transaction
 */
static inline transaction_status start_transaction()
{
  unsigned int code = _xbegin();

  if(code == _XBEGIN_STARTED) return BEGIN;
  else if((code & _XABORT_RETRY) || !code) return TRANSIENT;
  else if(code & _XABORT_CAPACITY) return CAPACITY;
  else if(code & _XABORT_CONFLICT) return CONFLICT;
  else if(code & _XABORT_EXPLICIT) return PERSISTENT;
  return OTHER;
}

/* End a transaction. */
#define stop_transaction() _xend()

/* Return non-zero if in a transaction, or zero if executing normally. */
#define in_transaction() _xtest()

#endif /* _HTM_X86_H */

