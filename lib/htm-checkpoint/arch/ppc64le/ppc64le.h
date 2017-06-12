/*
 * Hardware transactional memory definitions for PowerPC-based architectures,
 * i.e., POWER8.
 */

#ifndef _HTM_PPC64_H
#define _HTM_PPC64_H

#ifndef __HTM__
# error "Cannot use PowerPC HTM for this target or CPU"
#endif

#include <htmintrin.h>

// Note: there's a typo in GCC/clang's htmintrin.h, correct here
#ifdef _TEXASR_IMPLEMENTAION_SPECIFIC
# define _TEXASR_IMPLEMENTATION_SPECIFIC _TEXASR_IMPLEMENTAION_SPECIFIC
#endif
#ifdef _TEXASRU_IMPLEMENTAION_SPECIFIC
# define _TEXASRU_IMPLEMENTATION_SPECIFIC _TEXASRU_IMPLEMENTAION_SPECIFIC
#endif

/*
 * PowerPC HTM contains a special rollback-only transaction mode with the
 * following properties:
 *
 *  - No memory barriers at beginning/end of transaction
 *  - No integrated cumulative barrier for reads & writes
 *  - Rollback-only transactions are not serialized
 *  - No tracking of memory loads
 *
 *  This is basically pure hardware checkpointing, and thus what we use.
 */
#define PPC_ROLLBACK_ONLY_TRANSACTION 1

/*
 * Start transaction.  If a failure occurs, convert PowerPC's status codes into
 * a generic format.
 *
 * BEGIN:
 *   - transaction started successfully
 *
 * TRANSIENT:
 *   - implementation-specific reason to abort
 *
 * CAPACITY:
 *   - footprint overflow - transactional state overflowed buffers (should only
 *                          apply to stores since we're executing in ROT mode)
 *
 * CONFLICT:
 *   - conflicting write with another thread executing non-transactionally
 *   - conflicting write with another thread executing transactionally
 *   - conflicting write to page with invalidated TLB entry
 *   - conflicting fetch from an instruction block changed transactionally
 *
 * PERSISTENT:
 *   - disallowed instruction/access type
 *   - aborted by tabort* instruction
 *
 * Abort reasons we should never experience:
 *   - self-induced conflicts - had conflicting access in suspend state
 *   - nesting overflows - nested transaction depth too deep
 */
static inline transaction_status start_transaction()
{
  unsigned int started = __builtin_tbegin(PPC_ROLLBACK_ONLY_TRANSACTION);
  unsigned long texasru;

  if(started) return BEGIN;
  else
  {
    texasru = __builtin_get_texasru();
    if(_TEXASRU_IMPLEMENTATION_SPECIFIC(texasru)) return TRANSIENT;
    else if(_TEXASRU_FOOTPRINT_OVERFLOW(texasru)) return CAPACITY;
    else if(_TEXASRU_NON_TRANSACTIONAL_CONFLICT(texasru) ||
            _TEXASRU_TRANSACTION_CONFLICT(texasru) ||
            _TEXASRU_TRANSLATION_INVALIDATION_CONFLICT(texasru) ||
            _TEXASRU_INSTRUCTION_FETCH_CONFLICT(texasru)) return CONFLICT;
    else if(_TEXASRU_DISALLOWED(texasru) ||
            _TEXASRU_ABORT(texasru)) return PERSISTENT;
    return OTHER;
  }
}

/* Stop a transaction. */
#define stop_transaction() __builtin_tend(0)

/* Return non-zero if in transaction, or zero if executing normally. */
static inline int in_transaction()
{
  unsigned char tx_state = _HTM_STATE(__builtin_ttest());
  if(tx_state == _HTM_TRANSACTIONAL) return 1;
  else return 0;
}

#endif /* _HTM_PPC64_H */

