/*
 * Provides an ABI-compatible interface to libgomp functions using
 * compiler-generated calls to the Intel OpenMP runtime.
 *
 * Declarations & definitions are taken/adapted from the LLVM OpenMP runtime
 * v3.7.1
 *
 * Copyright Rob Lyerly, SSRG, VT, 2017
 */

#include <stdint.h>

/* Maximum number of threads supported by Intel OpenMP API shim. */
#define MAX_THREADS 1024

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

/* 
 * Outlined functions comprising the OpenMP parallel code regions (kmp).
 * @param global_tid the global thread identity of the thread executing the function.
 * @param bound_tid the local identity of the thread executing the function.
 * @param ... pointers to shared variables accessed by the function
 */
typedef void (*kmpc_micro) (int32_t *global_tid, int32_t *bound_tid, ...);
typedef void (*kmpc_micro_bound) (int32_t *bound_tid, int32_t *bound_nth, ...);
void __kmp_wrapper_fn(void *data);

/*
 * Data passed to __kmp_wrapper_fn() to invoke microtask via Intel OpenMP runtime's
 * outline function API.
 */
typedef struct __kmp_data {
  union {
    kmpc_micro task;
    kmpc_micro_bound bound_task;
  };
  int32_t *mtid;
  void *data;
} __kmp_data_t;
