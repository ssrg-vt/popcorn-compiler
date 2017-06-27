/*
 * Transactional execution instrumentation.  Start transactions at equivalence
 * points, with results to be committed at next encountered equivalence point.
 * Log transaction execution results (if enabled at compile time).
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/16/2017
 */

#include <stddef.h>
#include "transaction.h"

#ifdef _STATISTICS

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include "statistics.h"
#include "tsx_assert.h"

static htm_log log;
static volatile int log_initialized = 0;
static pthread_mutex_t lock;
static htm_log_entry app_makespan;
static volatile int tid_ctr = 0;
static __thread int tid = -1;
static __thread htm_log_entry entry;

// Note: the following are implemented as macros in order to better support
// finding the call site (i.e., __builtin_return_address())

// Note: the following macros must *only* be called outside of transactions
// because timing & log functions may cause abort.

// TODO locking in the following macros is a bottleneck when multiple threads
// execute many function calls

/* Start log entry for a transaction. */
#define LOG_START( _log_entry, _fn ) \
  do { \
    struct timespec start; \
    tsx_assert(!in_transaction()); \
    clock_gettime(CLOCK_MONOTONIC, &start); \
    _log_entry.start = TS_TO_NS(start); \
    _log_entry.fn = _fn; \
    _log_entry.pc = __builtin_return_address(0); \
  } while(0)

/* Store a log entry's status. */
#define LOG_STATUS( _log_entry, _status ) \
  do { tsx_assert(!in_transaction()); _log_entry.status = _status; } while(0)

/* Take an ending timestamp & record the entry in the log. */
#define LOG_END( _log_entry ) \
  do { \
    struct timespec end; \
    if(_log_entry.fn) { \
      tsx_assert(!in_transaction()); \
      clock_gettime(CLOCK_MONOTONIC, &end); \
      _log_entry.end = TS_TO_NS(end); \
      pthread_mutex_lock(&lock); \
      htm_log_push_back(&log, &_log_entry); \
      pthread_mutex_unlock(&lock); \
    } \
  } while(0)

/* Log a transaction -- save it's status, take ending timestamp & record in
 * log.  This does the whole shebang for successful transactions. */
#define LOG_SUCCESS( _log_entry ) \
  do { LOG_STATUS( _log_entry, SUCCESS ); LOG_END( _log_entry ); } while(0)

/* Initialize the log. */
// Note: not thread safe!
static inline void init_log()
{
  const char *fn;
  if(!log_initialized)
  {
    if(!(fn = getenv(HTM_STAT_FN_ENV))) fn = HTM_STAT_DEFAULT_FN;
    htm_log_init(&log, fn);
    LOG_START(app_makespan, (void *)UINT64_MAX);
    log_initialized = 1;
  }
}

/* Initialize per-thread statistics information. */
static inline void init_thread_stats()
{
  // Rather than grabbing the thread's ID through a system call (which triggers
  // an abort) use a simple TID counter.
  tid = __atomic_fetch_add(&tid_ctr, 1, __ATOMIC_SEQ_CST);
  entry.tid = tid;
  entry.fn = NULL;

  // Initialize the log.  We call this here because it's possible we get a log
  // entry before __htm_stats_init() is called due to constructor ordering
  init_log();
}

#else

#define LOG_START( _log_entry, _fn ) {}
#define LOG_STATUS( _log_entry, _status ) {}
#define LOG_END( _log_entry ) {}
#define LOG_SUCCESS( _log_entry ) {}

#endif /* _STATISTICS */

/* Get human-readable names of transaction statuses. */
const char *status_name(transaction_status status)
{
  static const char *transaction_status_names[] = {
#define X(status) #status,
    TRANSACTION_STATUSES
#undef X
  };

  switch(status)
  {
#define X(status) case status: 
  TRANSACTION_STATUSES
#undef X
    return transaction_status_names[status];
  default:
    return "(unknown status)";
  }
}

/*
 * End current transaction (if still inside transactional region) and begin new
 * transaction.  Retries a transaction multiple times if abort status indicates
 * a transient cause.
 */
void __cyg_profile_func_enter(void *fn, void *cs)
{
  size_t i;
  transaction_status status;

#ifdef _STATISTICS
  if(tid == -1) init_thread_stats();
#endif /* _STATISTICS */

  // If executing transactionally, return to normal execution.  Record
  // transaction log entry for all statuses.
  if(in_transaction())
  {
    stop_transaction();
    LOG_SUCCESS(entry);
  }
  else LOG_END(entry);

  // Start the next transaction.  Because we can't log inside a transaction,
  // add entry for beginning of transaction before loop.  Subsequent log
  // entries will be added as aborts occur (which exit the transaction).
  LOG_START(entry, fn);
  status = TRANSIENT;
  for(i = 0; status == TRANSIENT && i < NUM_RETRY_TRANSIENT; i++)
    status = start_transaction();

  // Save the status, but don't take the end timestamp until the end of the
  // section, i.e., at the next equivalence point.
  if(!in_transaction()) LOG_STATUS(entry, status);
}

/* Use same implementation as above. */
void __attribute__((alias("__cyg_profile_func_enter")))
__cyg_profile_func_exit(void *, void *);

#ifdef _STATISTICS
/* Prepare statistics gathering machinery & initialize main thread. */
static void __attribute__((constructor)) __htm_stats_init()
{
  init_thread_stats();
}
#endif /* _STATISTICS */

/* Finish any final transactions & clean up. */
static void __attribute__((destructor)) __htm_cleanup()
{
  if(in_transaction())
  {
    stop_transaction();
    LOG_SUCCESS(entry);
  }
  else LOG_END(entry);

#ifdef _STATISTICS
  LOG_STATUS(app_makespan, APP_MAKESPAN);
  LOG_END(app_makespan);
  htm_log_write_entries(&log);
  htm_log_free(&log);
#endif /* _STATISTICS */
}

