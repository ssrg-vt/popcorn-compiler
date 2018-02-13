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

/* DSM advice values */
// TODO get actual values
#define MADV_READ -256 // Request write permissions
#define MADV_WRITE -256 // Request read permissions
#define MADV_RELEASE -255 // Forfeit current permissions

#endif

