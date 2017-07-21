/*
 * Transactional execution definitions & APIs.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2017
 */

#ifndef _TRANSACTION_H
#define _TRANSACTION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Number of times to retry transient-like aborts before continuing
 * non-transactionally. */
#define NUM_RETRY_TRANSIENT 3

/* Transaction statuses.  Architecture-specific code should convert
 * per-processor statuses to one of these. */
#define TRANSACTION_STATUSES \
  X(BEGIN) /* Beginning of transaction */ \
  X(SUCCESS) /* Successful transaction */ \
  X(CONFLICT) /* Memory access conflict */ \
  X(CAPACITY) /* Transactional memory buffers reached capacity */ \
  X(TRANSIENT) /* Aborted for reason in which a retry will likely succeed */ \
  X(PERSISTENT) /* Aborted for reason which will continue to cause aborts */ \
  X(OTHER) /* Some other abort reason we don't care about */ \
  X(APP_MAKESPAN) /* Application's run time, from start to finish */ \

/* Enumeration of statuses using X-macros above. */
typedef enum transaction_status {
#define X(status) status,
  TRANSACTION_STATUSES
#undef X
  NUM_STATUS,
} transaction_status;

/* Get human-readable names of statuses enumerated above. */
extern const char *status_name(transaction_status status);

#ifdef _STATISTICS
#include "statistics.h"
#endif

/*
 * Per-architecture HTM definitions.  Each header should define that ISA's
 * implementation for:
 *
 *   start_transaction(): Start transactional execution & return status code.
 *                        This is also where aborted transactions return.
 *   stop_transaction() : Stop transactional execution.
 *   in_transaction()   : Return non-zero if in transaction, or zero otherwise.
 */
#ifdef __powerpc64__
# include "ppc64le.h"
#elif defined __x86_64__
# include "x86_64.h"
#else
# error "Unsupported target architecture"
#endif

// TODO Equivalence point APIs

#ifdef __cplusplus
}
#endif

#endif /* _TRANSACTION_H */

