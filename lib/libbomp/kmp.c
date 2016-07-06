/*
 * Provides an ABI-compatible interface to libbomp functions using
 * compiler-generated calls to the Intel OpenMP runtime.
 *
 * Declarations & definitions are taken from the LLVM OpenMP runtime v3.8.0
 *
 * Copyright Rob Lyerly, SSRG, VT, 2016
 */

// TODO what's the difference between global & local/bound TID?

// TODO for function which take a kmp_critical_name: lock using the name
// instead of falling back on GOMP_critical_start/end (may cause false waiting)

// TODO GOMP_ordered_start/end are empty...shouldn't these be setting ordering
// thread execution?

#include <assert.h>
#include "libbomp.h"
#include "backend.h"
#include "kmp.h"

/* Enable debugging information */
//#define _DEBUG 1

#ifdef _DEBUG
# define DEBUG( ... ) fprintf(stderr, __VA_ARGS__)
#else
# define DEBUG( ... )
#endif

///////////////////////////////////////////////////////////////////////////////
// Parallel region
///////////////////////////////////////////////////////////////////////////////

/* Source location information for OpenMP parallel construct. */
typedef struct ident {
  int32_t reserved_1;
  int32_t flags;
  int32_t reserved_2;
  int32_t reserved_3;
  char const *psource;
} ident_t;

/* 
 * Outlined functions comprising the OpenMP parallel code regions (kmp).
 * @param global_tid the global thread identity of the thread executing the function.
 * @param bound_tid the local identity of the thread executing the function.
 * @param ... pointers to shared variables accessed by the function
 */
typedef void (*kmpc_micro) (int32_t *global_tid, int32_t *bound_tid, ...);
typedef void (*kmpc_micro_bound) (int32_t *bound_tid, int32_t *bound_nth, ...);

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

/*
 * Converts calls to GNU OpenMP runtime outlined regions to Intel OpenMP
 * runtime outlined regions (which includes the global & bound thread ID).
 * @param data wrapped data which includes the outlined function, the global
 *        thread ID and the data to pass to the function.
 */
void __kmp_wrapper_fn(void *data)
{
  int32_t tid = omp_get_thread_num();
  __kmp_data_t *wrapped = (__kmp_data_t *)data;

  DEBUG("__kmp_wrapper_fn: %p %p\n", wrapped->task, wrapped->data);

  wrapped->task(&tid, &tid, wrapped->data);
}

/*
 * Begin OpenMP parallel region.
 * @param loc source location information
 * @param argc number of arguments in vararg
 * @param microtask the outlined OpenMP code
 * @param ... pointers to shared variables
 */
void __kmpc_fork_call(ident_t *loc, int32_t argc, kmpc_micro microtask, ...)
{
  int32_t i, mtid = 0, ltid = 0;
  va_list vl;
  void *shared_data;
  __kmp_data_t wrapper_data;

  DEBUG("__kmp_fork_call: %s calling %p\n", loc->psource, microtask);

  /* Marshal data for spawned microtask */
  va_start(vl, microtask);
  if(argc > 1)
  {
    void **args = malloc(sizeof(void*) * argc);
    for(i = 0; i < argc; i++)
      args[i] = va_arg(vl, void *);
    shared_data = (void *)args;
  }
  else shared_data = va_arg(vl, void *);
  va_end(vl);

  wrapper_data.task = microtask;
  wrapper_data.mtid = &mtid;
  wrapper_data.data = shared_data;

  /* Start workers & run the task */
  GOMP_parallel_start(__kmp_wrapper_fn, &wrapper_data, bomp_num_threads);
  microtask(&mtid, &ltid, shared_data);
  GOMP_parallel_end();

  if(argc > 1) free(shared_data);
}

///////////////////////////////////////////////////////////////////////////////
// Work-sharing
///////////////////////////////////////////////////////////////////////////////

/*
 * Compute the upper and lower bounds and stride to be used for the set of
 * iterations to be executed by the current thread from the statically
 * scheduled loop that is described by the initial values of the bounds,
 * stride, increment and chunk size.
 * @param loc source code location
 * @param gtid global thread ID of this thread
 * @param schedtype scheduling type
 * @param plastiter pointer to the "last iteration" flag
 * @param plower pointer to the lower bound
 * @param pupper pointer to the upper bound
 * @param pstride pointer to the stride
 * @param incr loop increment
 * @param chunk the chunk size
 */
#define __kmpc_for_static_init(NAME, TYPE)                                    \
void __kmpc_for_static_init_##NAME(ident_t *loc,                              \
                                   int32_t gtid,                              \
                                   int32_t schedtype,                         \
                                   int32_t *plastiter,                        \
                                   TYPE *plower,                              \
                                   TYPE *pupper,                              \
                                   TYPE *pstride,                             \
                                   TYPE incr,                                 \
                                   TYPE chunk)                                \
{                                                                             \
  int nthreads = omp_get_num_threads();                                       \
  TYPE total_trips;                                                           \
                                                                              \
  DEBUG("__kmpc_for_static_init_"#NAME": %s %d %d %d %ld %ld %ld %ld %ld\n",  \
        loc->psource, gtid, schedtype, *plastiter, (int64_t)*plower,          \
        (int64_t)*pupper, (int64_t)*pstride, (int64_t)incr, (int64_t)chunk);  \
                                                                              \
  if(incr == 1) total_trips = (*pupper - *plower) + 1;                        \
  else if(incr == -1) total_trips = (*plower - *pupper) + 1;                  \
  else if(incr > 1) total_trips = ((*pupper - *plower) / incr) + 1;           \
  else total_trips = ((*plower - *pupper) / (-incr)) + 1;                     \
                                                                              \
  switch(schedtype)                                                           \
  {                                                                           \
  case kmp_sch_static: {                                                      \
    if(total_trips < nthreads)                                                \
    {                                                                         \
      if(gtid < total_trips) *pupper = *plower = *plower + gtid * incr;       \
      else *plower = *pupper + incr;                                          \
      if(plastiter != NULL) *plastiter = (gtid == total_trips - 1);           \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      TYPE chunk = total_trips / nthreads;                                    \
      TYPE extras = total_trips % nthreads;                                   \
      *plower += incr * (gtid * chunk + (gtid < extras ? gtid : extras));     \
      *pupper = *plower + chunk * incr - (gtid < extras ? 0 : incr);          \
      if(plastiter != NULL) *plastiter = (gtid == nthreads - 1);              \
    }                                                                         \
    break;                                                                    \
  }                                                                           \
  case kmp_sch_static_chunked: {                                              \
    TYPE span;                                                                \
    if(chunk < 1) chunk = 1;                                                  \
    span = chunk * incr;                                                      \
    *pstride = span * nthreads;                                               \
    *plower = *plower + (span * gtid);                                        \
    *pupper = *plower + span - incr;                                          \
    if(plastiter != NULL)                                                     \
      *plastiter = (gtid == ((total_trips - 1)/chunk) % nthreads);            \
    break;                                                                    \
  }                                                                           \
  default:                                                                    \
    assert(false && "Unknown scheduling algorithm");                          \
  }                                                                           \
}

/* Generate the above function for int32_t, uint32_t, int64_t, && uint64_t. */
__kmpc_for_static_init(4, int32_t)
__kmpc_for_static_init(4u, uint32_t)
__kmpc_for_static_init(8, int64_t)
__kmpc_for_static_init(8u, uint64_t)

/*
 * Mark the end of a statically scheduled loop.
 * @param loc source location
 * @param global_tid global thread ID
 */
void __kmpc_for_static_fini(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_for_static_fini: %s %d\n", loc->psource, global_tid);
}

void __kmpc_dispatch_init_4(ident_t *loc,
                            int32_t gtid,
                            enum sched_type schedule,
                            int32_t lb,
                            int32_t ub,
                            int32_t st,
                            int32_t chunk)
{
  //TODO
  assert(false && "Dynamically-scheduled loops not implemented");
}

int __kmpc_dispatch_next_4(ident_t *loc,
                           int32_t gtid,
                           int32_t *p_last,
                           int32_t *p_lb,
                           int32_t *p_ub,
                           int32_t *p_st)
{
  // TODO
  assert(false && "Dynamically-scheduled loops not implemented");
  return 0;
}

void __kmpc_dispatch_fini_4(ident_t *loc, int32_t gtid)
{
  // TODO
  assert(false && "Dynamically-scheduled loops not implemented");
}
/*
 * Start execution of an ordered construct.
 * @param loc source location information
 * @param gtid global thread number
 */
void __kmpc_ordered(ident_t *loc, int32_t gtid)
{
  DEBUG("__kmpc_ordered: %s %d\n", loc->psource, global_tid);

  GOMP_ordered_start();
}

/*
 * End execution of an ordered construct.
 * @param loc source location information
 * @param gtid global thread number
 */
void __kmpc_end_ordered(ident_t *loc, int32_t gtid)
{
  DEBUG("__kmpc_end_ordered: %s %d\n", loc->psource, global_tid);

  GOMP_ordered_end();
}

/*
 * Enter code protected by a critical construct.  This function blocks until
 * the executing thread can enter the critical section.
 * @param loc source location information
 * @param global_tid global thread number
 * @param crit identity of the critical section.  This could be a pointer to a
 *             lock associated with the critical section, or some other suitably
 *             unique value.
 */
void __kmpc_critical(ident_t *loc, int32_t global_tid, kmp_critical_name *crit)
{
  DEBUG("__kmpc_critical: %s %d %p\n", loc->psource, global_tid, crit);

  GOMP_critical_start();
}

/*
 * Leave a critical section, releasing any lock that was held during its
 * execution.
 * @param loc source location information
 * @param global_tid global thread number
 * @param crit identity of the critical section.  This could be a pointer to a
 *             lock associated with the critical section, or some other suitably
 *             unique value.
 */
void __kmpc_end_critical(ident_t *loc,
                         int32_t global_tid,
                         kmp_critical_name *crit)
{
  DEBUG("__kmpc_end_critical: %s %d %p\n", loc->psource, global_tid, crit);

  GOMP_critical_end();
}

/*
 * Test whether or not this thread should execute the master section
 * @param loc source location information
 * @param global_tid global thread number
 * @return 1 if this thread should execute the master block, 0 otherwise
 */
int32_t __kmpc_master(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_master: %s %d\n", loc->psource, global_tid);

  if(global_tid == 0) return 1;
  else return 0;
}

/*
 * Mark the end of a master region.  This should only be called by the thread
 * that executes the master region.
 * @param loc source location information
 * @param global_tid global thread number
 */
void __kmpc_end_master(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_end_master: %s %d\n", loc->psource, global_tid);
}

///////////////////////////////////////////////////////////////////////////////
// Synchronization
///////////////////////////////////////////////////////////////////////////////

/*
 * Barrier with cancellation point to send threads from the barrier to the end
 * of the parallel region.  Needs a special code pattern as document in the
 * design document for the cancellation feature.
 * @param loc source location information
 * @param gtid global thread ID
 * @return returns true if a matching cancellation request has been flagged in
 *         the RTL and the encountering thread has to cancel.
 */
int32_t __kmpc_cancel_barrier(ident_t* loc, int32_t gtid)
{
  DEBUG("__kmpc_cancel_barrier: %s %d\n", loc->psource, gtid);

  // Note: needed for OpenMP 4.0 cancellation points (not required for us)
  GOMP_barrier();
  return 0;
}

/*
 * Execute a barrier.
 * @param loc source location information
 * @param global_tid thread id
 */
void __kmpc_barrier(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_barrier: %s %d\n", loc->psource, global_tid);

  GOMP_barrier();
}

///////////////////////////////////////////////////////////////////////////////
// Reductions
///////////////////////////////////////////////////////////////////////////////

typedef void (*reduce_func)(void *lhs_data, void *rhs_data);

/*
 * A blocking reduce that includes an implicit barrier.
 * @param loc source location information
 * @param global_tid global thread number
 * @param num_vars number of items (variables) to be reduced
 * @param reduce_size size of data in bytes to be reduced
 * @param pointer to data to be reduced
 * @param reduce_func callback function providing reduction operation on two
 *        operands and returning result of reduction in lhs_data
 * @param lck pointer to the unique lock data structure
 * @result 1 for threads that need to do reduction (i.e. a critical section
 *         reduction), 2 for an atomic reduction, or 0 if none is needed
 */
int32_t __kmpc_reduce(ident_t *loc,
                      int32_t global_tid,
                      int32_t num_vars,
                      size_t reduce_size,
                      void *reduce_data,
                      reduce_func func,
                      kmp_critical_name *lck)
{
  DEBUG("__kmpc_reduce: %s %d %d %lu %p %p %p\n", loc->psource,
        global_tid, num_vars, reduce_size, reduce_data, func, lck);

  // Note: Intel's runtime does some smart selection of reduction algorithms,
  // but we'll do just a basic "every thread reduces their own value" by
  // entering a critical section.
  GOMP_critical_start();
  return 1;
}

/*
 * Finish the execution of a blocking reduce.
 * @param loc source location information
 * @param global_tid global thread ID
 * @param lck pointer to the unique lock data structre
 */
void __kmpc_end_reduce(ident_t *loc,
                       int32_t global_tid,
                       kmp_critical_name *lck)
{
  DEBUG("__kmpc_reduce_nowait: %s %d %p\n", loc->psource, global_tid, lck);

  GOMP_critical_end();
  GOMP_barrier();
}

/*
 * The nowait version is used for a reduce clause with the nowait argument.
 * @param loc source location information
 * @param global_tid global thread number
 * @param num_vars number of items (variables) to be reduced
 * @param reduce_size size of data in bytes to be reduced
 * @param pointer to data to be reduced
 * @param reduce_func callback function providing reduction operation on two
 *        operands and returning result of reduction in lhs_data
 * @param lck pointer to the unique lock data structure
 * @result 1 for threads that need to do reduction (i.e. a critical section
 *         reduction), 2 for an atomic reduction, or 0 if none is needed
 */
int32_t __kmpc_reduce_nowait(ident_t *loc,
                             int32_t global_tid,
                             int32_t num_vars,
                             size_t reduce_size,
                             void *reduce_data,
                             reduce_func func,
                             kmp_critical_name *lck)
{
  DEBUG("__kmpc_reduce_nowait: %s %d %d %lu %p %p %p\n", loc->psource,
        global_tid, num_vars, reduce_size, reduce_data, func, lck);

  // Note: Intel's runtime does some smart selection of reduction algorithms,
  // but we'll do just a basic "every thread reduces their own value" by
  // entering a critical section.
  GOMP_critical_start();
  return 1;
}

/*
 * Finish the execution of a reduce nowait.
 * @param loc source location information
 * @param global_tid global thread ID
 * @param lck pointer to the unique lock data structre
 */
void __kmpc_end_reduce_nowait(ident_t *loc,
                              int32_t global_tid,
                              kmp_critical_name *lck)
{
  DEBUG("__kmpc_reduce_nowait: %s %d %p\n", loc->psource, global_tid, lck);

  GOMP_critical_end();
}

///////////////////////////////////////////////////////////////////////////////
// Information retrieval
///////////////////////////////////////////////////////////////////////////////

/*
 * Get the global thread num for the OpenMP parallel region.
 * @param loc source location information
 */
int32_t __kmpc_global_thread_num(ident_t *loc)
{
  DEBUG("__kmpc_global_thread_num: %s\n", loc->psource);

  return omp_get_thread_num();
}

/*
 * Allocate private storage for threadprivate data.  Note that there is a cache
 * for every variable declared threadprivate
 * @param loc source location information
 * @param global_tid global thread number
 * @param data pointer to data to privatize
 * @param size size of data to privatize
 * @param cache pointer to cache
 * @param return pointer to private storage
 */
void *__kmpc_threadprivate_cached(ident_t *loc,
                                  int32_t global_tid,
                                  void *data,
                                  size_t size,
                                  void ***cache)
{
  void *ret;

  DEBUG("__kmpc_threadprivate_cached: %s %d %p %lu %p\n", loc->psource,
        global_tid, data, size, cache);

  /* Allocate a cache if not previously allocated for the variable. */
  if(*cache == NULL)
  {
    GOMP_critical_start();
    if(*cache == NULL)
      *cache = calloc(1, sizeof(void*) * MAX_THREADS);
    GOMP_critical_end();
  }

  /* Allocate (if necessary) & initialize this thread's data. */
  if((ret = (*cache)[global_tid]) == 0)
  {
    ret = malloc(size);
    memcpy(ret, data, size);
    (*cache)[global_tid] = ret;
  }

  return ret;
}

