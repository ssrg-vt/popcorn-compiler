/*
 * Provides an ABI-compatible interface to libbomp functions using
 * compiler-generated calls to the Intel OpenMP runtime.
 *
 * Declarations & definitions are taken/adapted from the LLVM OpenMP runtime
 * v3.8.0
 *
 * Copyright Rob Lyerly, SSRG, VT, 2016
 */

#include <string.h>

/* Maximum number of threads supported by Intel OpenMP API shim. */
#define MAX_THREADS 128

/* The loop schedule to be used for a parallel for loop. */
enum sched_type {
  kmp_sch_static_chunked = 33, /* statically chunked algorithm */
  kmp_sch_static = 34, /* static unspecialized */
  kmp_sch_default = kmp_sch_static /* default scheduling algorithm */
};

/* The reduction method for reduction clauses. */
enum reduction_method {
  reduction_method_not_defined = 0,
  critical_reduce_block = (1 << 8),
  atomic_reduce_block = (2 << 8),
  tree_reduce_block = (3 << 8),
  empty_reduce_block = (4 << 8)
};

/* Flags for ident_t struct */
#define KMP_IDENT_ATOMIC_REDUCE 0x10

/* Lock structure */
typedef int32_t kmp_critical_name[8];

