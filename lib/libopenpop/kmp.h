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

/* Source location & generation information for OpenMP constructs. */
typedef struct ident {
  int32_t reserved_1;
  int32_t flags;
  int32_t reserved_2;
  int32_t reserved_3;
  char const *psource;
} ident_t;

/* Flags for ident_t struct */
#define KMP_IDENT_ATOMIC_REDUCE 0x10

/* Maximum number of threads supported by Intel OpenMP API shim. */
#define MAX_THREADS 1024

/* The loop schedule to be used for a parallel for loop. */
enum sched_type {
  kmp_sch_static_chunked = 33, /* statically chunked algorithm */
  kmp_sch_static = 34, /* static unspecialized */
  kmp_sch_dynamic_chunked = 35, /* dynamically chunked algorithm */
  kmp_sch_runtime = 37, /* runtime chooses from parsing OMP_SCHEDULE */
  kmp_sch_hetprobe = 39, /* probe heterogeneous machines */
  kmp_sch_default = kmp_sch_static, /* default scheduling algorithm */
  kmp_sch_static_hierarchy = 128, /* hierarhical static algorithm */
  kmp_sch_dynamic_chunked_hierarchy = 129 /* hierarhical dynamic chunked algorithm */
};

/* Return whether compiler generated fast reduction method for reduce clause. */
#define FAST_REDUCTION_ATOMIC_METHOD_GENERATED( loc ) \
  ((loc->flags & KMP_IDENT_ATOMIC_REDUCE) == KMP_IDENT_ATOMIC_REDUCE)

/* Return whether compiler generated a tree reduction method. */
#define FAST_REDUCTION_TREE_METHOD_GENERATED( data, func ) ((data) && (func))

/* The reduction method for reduction clauses. */
enum reduction_method {
  reduction_method_not_defined = 0,
  critical_reduce_block = (1 << 8),
  atomic_reduce_block = (2 << 8),
  tree_reduce_block = (3 << 8),
  empty_reduce_block = (4 << 8)
};

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
