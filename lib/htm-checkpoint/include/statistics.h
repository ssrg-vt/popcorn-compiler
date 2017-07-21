/*
 * Definitions for collecting statistics about HTM execution.
 *
 * Note: htm_log_* APIs are *not* thread safe!
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2017
 */

#ifndef _STATISTICS_H
#define _STATISTICS_H

#include <stdio.h>

/* Environment variable controlling output filename & default filename in which
 * to write results. */
#define HTM_STAT_FN_ENV "HTM_STAT_FN"
#define HTM_STAT_DEFAULT_FN "htm-stats.csv"

/* Convert struct timespecs to raw nanoseconds. */
#define TS_TO_NS( ts ) (ts.tv_sec * 1e9 + ts.tv_nsec)
#define TSPTR_TO_NS( tsptr ) TS_TO_NS( (*tsptr) )

/* A log entry recording HTM statistics. */
typedef struct htm_log_entry {
  int tid; // thread ID
  unsigned long start, end; // start and end time stamp
  transaction_status status; // transaction result
  void *fn; // function in which transaction starts
  void *pc; // call site of beginning of transaction
} htm_log_entry;

/* Default initial capacity for the log. */
#define DEFAULT_CAPACITY 8192

/* Rate-limit adding entries to the log based on MIN_PERIOD. */
#define RATE_LIMIT 1

/* Minimum sampling period, entries sampled under this time limit are
 * discarded. */
#define MIN_PERIOD (1 * 1e6)

/* A log purpose built for holding htm_log_t entries. */
typedef struct htm_log {
  size_t capacity; // The amount of allocated storage
  size_t size; // The number of elements currently in the htm_log
  htm_log_entry *entries; // Log entries
  FILE *file; // Backing file
} htm_log;

/* Initialize an empty htm_log. Opens up the specified file as a backing
 * store for streaming log entries as log grows. */
void htm_log_init(htm_log *l, const char *fn);

/* Free the resources used by a htm_log. */
void htm_log_free(htm_log *l);

/* Add an element to the back of the htm_log. */
void htm_log_push_back(htm_log *l, htm_log_entry *e);

/* Get an entry stored in the htm_log. */
const htm_log_entry *htm_log_get(htm_log *l, size_t elem);

/* Write all log entries to the backing file. */
void htm_log_write_entries(htm_log *l);

#endif /* _STATISTICS_H */

