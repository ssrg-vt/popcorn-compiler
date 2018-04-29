/*
 * Library-internal prefetching definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#ifndef _DEFINITIONS_H
#define _DEFINITIONS_H

#include <stdint.h>

/* A span of memory for which a thread has requested prefetching. */
typedef struct {
  uint64_t low, high;
} memory_span_t;

#define MAX( a, b ) (a > b ? a : b)
#define MIN( a, b ) (a < b ? a : b)

/* Get the size of a memory span. */
#define SPAN_SIZE( mem ) ((mem).high - (mem).low)

/* Get the number of pages in the span */
#define SPAN_NUM_PAGES( mem ) \
  ((PAGE_ROUND_UP((mem).high) - PAGE_ROUND_DOWN((mem).low)) / PAGESZ)

/* DSM advice values */
#define MADV_READ 20 // Request write permissions
#define MADV_WRITE 19 // Request read permissions
#define MADV_RELEASE 18 // Forfeit current permissions

/* Enable/disable printing debugging messages */
#ifdef _DEBUG
#include <stdio.h>
#define debug( ... ) fprintf(stderr, __VA_ARGS__)
#define warn( ... ) fprintf(stderr, "WARNING: " __VA_ARGS__)
#else
#define debug( ... )
#define warn( ... )
#endif

/* Shorthand for manual asynchronous prefetching */
#if defined(_MANUAL_PREFETCH) && defined(_MANUAL_ASYNC)
#define _MAPREFETCH
#endif

/* Convert a struct timespec to raw nanoseconds */
#define NS( ts ) ((ts.tv_sec * 1000000000UL) + ts.tv_nsec)

/* Environment variable to set log file for statistics */
#define ENV_STAT_LOG_FN "POPCORN_PREFETCH_STATS_FN"

/*
 * Size of statically-allocated per-node cache.  Should be a multiple of 128 to
 * ensure caches pages for different nodes are placed on different pages.
 */
#define NODE_CACHE_SIZE 256

#endif

