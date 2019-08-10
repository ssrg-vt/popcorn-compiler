/*
 * Provides an ABI-compatible interface to libgomp functions using
 * compiler-generated calls to the Intel OpenMP runtime.
 *
 * Declarations & definitions are taken from the LLVM OpenMP runtime v3.7.1
 *
 * Copyright Rob Lyerly, SSRG, VT, 2017
 * 
 */

// TODO what's the difference between global & local/bound TID?

// TODO for function which take a kmp_critical_name: lock using the name
// instead of falling back on GOMP_critical_start/end (may cause false waiting)

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "config.h"
#include "libgomp_g.h"
#include "hierarchy.h"
#include "kmp.h"

/* Enable debugging information */
//#define _KMP_DEBUG 1

#ifdef _KMP_DEBUG
# define DEBUG( ... ) fprintf(stderr, __VA_ARGS__)
# define DEBUG_ONE( ... ) \
  do { if(gtid == 0) fprintf(stderr, __VA_ARGS__); } while (0);
#else
# define DEBUG( ... )
# define DEBUG_ONE( ... )
#endif

///////////////////////////////////////////////////////////////////////////////
// Parallel region
///////////////////////////////////////////////////////////////////////////////

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
// TODO Technically this is supposed to be a variable argument function, but it
// seems like we're not using va_lists properly on aarch64 (this function,
// OpenMP code generation or LLVM's va_list codegen) -- we're getting weird
// offsets and wrong context values.  However in clang/LLVM OpenMP 3.7.1,
// parallel sections are implemented as captures, meaning argc should always be
// 1 and we should only ever be passing in a context struct pointer.
void
__kmpc_fork_call(ident_t *loc, int32_t argc, kmpc_micro microtask, void *ctx)
{
  int32_t mtid = 0, ltid = 0;
#ifdef _TIME_PARALLEL
  struct timespec start, end;
#endif
  __kmp_data_t *wrapper_data = (__kmp_data_t *)malloc(sizeof(__kmp_data_t));

  DEBUG("__kmp_fork_call: %s calling %p\n", loc->psource, microtask);

  // TODO this should be uncommented when moving to va_list implementation
  //void *shared_data;
  //int32_t i;
  //va_list ap;
  ///* Marshal data for spawned microtask */
  //va_start(ap, microtask);
  //if(argc > 1)
  //{
  //  void **args = (void **)malloc(sizeof(void*) * argc);
  //  for(i = 0; i < argc; i++)
  //    args[i] = va_arg(ap, void *);
  //  shared_data = (void *)args;
  //}
  //else shared_data = va_arg(ap, void *);
  //va_end(ap);
  assert(argc == 1 && ctx && "Unsupported __kmpc_fork_call");

  wrapper_data->task = microtask;
  wrapper_data->mtid = &mtid;
  wrapper_data->data = ctx;

  /* Start workers & run the task */
#ifdef _TIME_PARALLEL
  clock_gettime(CLOCK_MONOTONIC, &start);
#endif
  GOMP_parallel_start(__kmp_wrapper_fn, wrapper_data, 0);
  DEBUG("%s: finished GOMP_parallel_start!\n",__func__);
  microtask(&mtid, &ltid, ctx);
  DEBUG("%s: finished microtask!\n",__func__);
  GOMP_parallel_end();
  DEBUG("%s: finished GOMP_parallel_end!\n",__func__);
#ifdef _TIME_PARALLEL
  clock_gettime(CLOCK_MONOTONIC, &end);
  popcorn_log("%s\t%p\t%lu\n", loc->psource, microtask, NS(end) - NS(start));
#endif

  /*
   * We've already set the core speed ratios to adjust for single-node
   * execution, change the configuration so that only threads on the
   * preferred node execute.
   */
  // TODO hardcoded for 2 nodes
  if(popcorn_global.popcorn_killswitch)
  {
    if(popcorn_preferred_node == 0)
    {
      omp_set_num_threads(popcorn_global.node_places[0]);
      popcorn_global.node_places[1] = 0;
      hierarchy_clear_node_team_state(1);
    }
    else
    {
      omp_set_num_threads(popcorn_global.node_places[1] + 1);
      popcorn_global.node_places[0] = 1;
      hierarchy_clear_node_team_state(0);
    }
  }

  if(argc > 1) free(ctx);
  free(wrapper_data);
}

///////////////////////////////////////////////////////////////////////////////
// Work-sharing
///////////////////////////////////////////////////////////////////////////////

// Note: clang generates libiomp calls such that lower bound is always smaller
// than upper bound by analyzing the loop condition & increment expressions

// Note: libgomp's APIs expect the end iteration to be non-inclusive while
// libiomp's APIs expect it to be inclusive.  There are +1/-1 values scattered
// across the work-sharing functions to translate between the two.

// TODO check that skewed range calculations are signed/unsigned safe

/*
 * Get the logical starting point & its range skewed based on the core rating
 * supplied by the user.
 * @param gtid global thread ID of this thread
 * @param start start of logical range for assigning loop iterations
 * @param range size of logical range for assigning loop iterations
 */
static void
get_scaled_range(int32_t gtid, unsigned long *start, unsigned long *range)
{
  int i, tcount = 0, diff;
  unsigned long cur_range = 0;
  *start = UINT64_MAX;
  *range = 0;

  for(i = 0; i < MAX_POPCORN_NODES; i++)
  {
    if(gtid < (int)(tcount + popcorn_global.threads_per_node[i]))
    {
      diff = gtid - tcount;
      *start = cur_range + diff * popcorn_global.core_speed_rating[i];
      *range = popcorn_global.core_speed_rating[i];
      break;
    }
    else
    {
      tcount += popcorn_global.threads_per_node[i];
      cur_range += popcorn_global.threads_per_node[i] *
                   popcorn_global.core_speed_rating[i];
    }
  }
}

/*
 * Compute the upper and lower bounds and stride to be used for the set of
 * iterations to be executed by the current thread from the statically
 * scheduled loop that is described by the initial values of the bounds,
 * stride, increment and chunk size.  Skew the iterations assigned according to
 * user-supplied per-node values.
 * @param nthreads number of threads in the team
 * @param gtid global thread ID of this thread
 * @param schedtype scheduling type
 * @param plastiter pointer to the "last iteration" flag
 * @param plower pointer to the lower bound
 * @param pupper pointer to the upper bound
 * @param pstride pointer to the stride
 * @param incr loop increment
 * @param chunk the chunk size
 * @param total_trips total number of loop iterations to be scheduled
 */
#define MIN( a, b ) ((a) < (b) ? (a) : (b))
#define for_static_skewed_init(NAME, TYPE)                                    \
static void for_static_skewed_init_##NAME(int32_t nthreads,                   \
                                          int32_t gtid,                       \
                                          int32_t schedtype,                  \
                                          int32_t *plastiter,                 \
                                          TYPE *plower,                       \
                                          TYPE *pupper,                       \
                                          TYPE *pstride,                      \
                                          TYPE incr,                          \
                                          TYPE chunk,                         \
                                          TYPE total_trips)                   \
{                                                                             \
  unsigned long start, range, scaled_thr = popcorn_global.scaled_thread_range;\
                                                                              \
  get_scaled_range(gtid, &start, &range);                                     \
  if(range == 0)                                                              \
  {                                                                           \
    *plower = *pupper + incr;                                                 \
    return;                                                                   \
  }                                                                           \
                                                                              \
  switch(schedtype)                                                           \
  {                                                                           \
  case kmp_sch_static: {                                                      \
    if(total_trips < scaled_thr)                                              \
    {                                                                         \
      if(start < total_trips)                                                 \
      {                                                                       \
        *plower += incr * start;                                              \
        *pupper = *plower + incr * MIN(range - 1, total_trips - 1 - start);   \
      }                                                                       \
      else *plower = *pupper + incr;                                          \
      if(plastiter != NULL) *plastiter = (start + range >= total_trips - 1);  \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      TYPE chunk = total_trips / scaled_thr;                                  \
      TYPE extras = total_trips % scaled_thr;                                 \
      TYPE my_extras = 0;                                                     \
      if(extras && start < extras) my_extras = MIN(range, extras - start);    \
      *plower += incr * (start * chunk + (start < extras ? start : extras));  \
      *pupper = *plower + incr * (chunk * range + my_extras - 1);             \
      if(plastiter != NULL) *plastiter = (gtid == nthreads - 1);              \
    }                                                                         \
    break;                                                                    \
  }                                                                           \
  case kmp_sch_static_chunked: {                                              \
    TYPE span;                                                                \
    if(chunk < 1) chunk = 1;                                                  \
    span = chunk * incr;                                                      \
    *pstride = span * scaled_thr;                                             \
    *plower = *plower + (span * start);                                       \
    *pupper = *plower + (span * range) - incr;                                \
    if(plastiter != NULL)                                                     \
      *plastiter = (start == ((total_trips - 1)/chunk) % scaled_thr);         \
    break;                                                                    \
  }                                                                           \
  default:                                                                    \
    assert(false && "Unknown scheduling algorithm");                          \
  }                                                                           \
}                                                                             \

/* Generate the above function for int32_t, uint32_t, int64_t, && uint64_t. */
for_static_skewed_init(4, int32_t)
for_static_skewed_init(4u, uint32_t)
for_static_skewed_init(8, int64_t)
for_static_skewed_init(8u, uint64_t)

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
#define __kmpc_for_static_init(NAME, TYPE, SPEC)                              \
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
  DEBUG("__kmpc_for_static_init_"#NAME": %s %d %d %d"                         \
        SPEC SPEC SPEC SPEC SPEC "\n",                                        \
        loc->psource, gtid, schedtype, *plastiter, *plower, *pupper,          \
        *pstride, incr, chunk);                                               \
                                                                              \
  if(incr == 1) total_trips = (*pupper - *plower) + 1;                        \
  else if(incr == -1) total_trips = (*plower - *pupper) + 1;                  \
  else if(incr > 1) total_trips = ((*pupper - *plower) / incr) + 1;           \
  else total_trips = ((*plower - *pupper) / (-incr)) + 1;                     \
                                                                              \
  if(popcorn_log_statistics)                                                  \
    hierarchy_init_statistics(gomp_thread()->popcorn_nid);                    \
                                                                              \
  if(popcorn_global.het_workshare)                                            \
  {                                                                           \
    for_static_skewed_init_##NAME(nthreads, gtid, schedtype, plastiter,       \
                                  plower, pupper, pstride, incr, chunk,       \
                                  total_trips);                               \
    return;                                                                   \
  }                                                                           \
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
    *plower = *pupper + incr;                                                 \
    break;                                                                    \
  }                                                                           \
}

/* Generate the above function for int32_t, uint32_t, int64_t, && uint64_t. */
__kmpc_for_static_init(4, int32_t, " %d")
__kmpc_for_static_init(4u, uint32_t, " %u")
__kmpc_for_static_init(8, int64_t, " %ld")
__kmpc_for_static_init(8u, uint64_t, " %lu")

/*
 * Mark the end of a statically scheduled loop.
 * @param loc source location
 * @param global_tid global thread ID
 */
void __kmpc_for_static_fini(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_for_static_fini: %s %d\n", loc->psource, global_tid);

  if(popcorn_log_statistics)
    hierarchy_log_statistics(gomp_thread()->popcorn_nid, loc->psource);
}

/*
 * Select loop iteration scheduler; only applies when application specifies the
 * "runtime" scheduler.
 */
static inline enum sched_type select_runtime_schedule()
{
  enum sched_type schedule;
  switch(gomp_global_icv.run_sched_var)
  {
  default: /* Fall through to static */
    DEBUG("Unknown/unsupported scheduler %d, reverting to static\n",
          gomp_global_icv.run_sched_var);
  case GFS_STATIC:
    if(gomp_global_icv.run_sched_chunk_size <= 1 &&
       gomp_global_icv.run_sched_chunk_size >= -1)
      schedule = kmp_sch_static;
    else
      schedule = kmp_sch_static_chunked;
    break;
  case GFS_DYNAMIC: schedule = kmp_sch_dynamic_chunked; break;
  case GFS_HETPROBE: schedule = kmp_sch_hetprobe; break;
  }
  return schedule;
}

/* Percent of loop iterations to spend on probing */
float popcorn_probe_percent;

static inline long calc_chunk_size_long(long lb,
                                        long ub,
                                        long stride,
                                        int nthreads)
{
  long total_trips, chunk;
  if(stride == 1) total_trips = (ub - lb) + 1;
  else if(stride == -1) total_trips = (lb - ub) + 1;
  else if(stride > 1) total_trips = ((ub - lb) / stride) + 1;
  else total_trips = ((lb - ub) / (-stride)) + 1;
  chunk = ((float)total_trips * popcorn_probe_percent) / nthreads;
  if(chunk == 0) chunk = 1;
  return chunk;
}

static inline unsigned long long calc_chunk_size_ull(unsigned long long lb,
                                                     unsigned long long ub,
                                                     unsigned long long stride,
                                                     int nthreads)
{
  unsigned long long total_trips, chunk;
  if(stride == 1) total_trips = (ub - lb) + 1;
  else if(stride == -1) total_trips = (lb - ub) + 1;
  else if(stride > 1) total_trips = ((ub - lb) / stride) + 1;
  else total_trips = ((lb - ub) / (-stride)) + 1;
  chunk = ((float)total_trips * popcorn_probe_percent) / nthreads;
  if(chunk == 0) chunk = 1;
  return chunk;
}

/*
 * Initialize a dynamic work-sharing construct using a given lower bound, upper
 * bound, stride and chunk.
 * @param loc source code location
 * @param gtid global thread ID of this thread
 * @param schedtype scheduling type
 * @param lb the loop iteration range's lower bound
 * @param ub the loop iteration range's upper bound
 * @param st the stride
 * @param chunk the chunk size
 */
#define __kmpc_dispatch_init(NAME, TYPE, SPEC, GOMP_TYPE,                     \
                             STATIC_INIT,                                     \
                             STATIC_HIERARCHY_INIT,                           \
                             DYN_INIT,                                        \
                             DYN_HIERARCHY_INIT,                              \
                             HETPROBE_INIT)                                   \
void __kmpc_dispatch_init_##NAME(ident_t *loc,                                \
                                 int32_t gtid,                                \
                                 enum sched_type schedule,                    \
                                 TYPE lb,                                     \
                                 TYPE ub,                                     \
                                 TYPE st,                                     \
                                 TYPE chunk)                                  \
{                                                                             \
  struct gomp_thread *thr = gomp_thread();                                    \
  struct gomp_team *team = thr->ts.team;                                      \
  int nthreads = team ? team->nthreads : 1;                                   \
  bool distributed = popcorn_distributed();                                   \
                                                                              \
  DEBUG("__kmpc_dispatch_init_"#NAME": %s %d %d"SPEC SPEC SPEC SPEC"\n",      \
        loc->psource, gtid, schedule, lb, ub, st, chunk);                     \
                                                                              \
  if(schedule == kmp_sch_runtime)                                             \
  {                                                                           \
    schedule = select_runtime_schedule();                                     \
    chunk = gomp_global_icv.run_sched_chunk_size;                             \
    DEBUG("__kmpc_dispatch_init_"#NAME": %d %d -> %d, chunk ="SPEC"\n",       \
          gtid, kmp_sch_runtime, schedule, chunk);                            \
  }                                                                           \
                                                                              \
  if(nthreads == 1) {                                                         \
    st = 1;                                                                   \
    chunk = (ub + 1) - lb;                                                    \
    schedule = kmp_sch_dynamic_chunked;                                       \
    DEBUG("Single-thread team, assigning all iterations\n");                  \
  }                                                                           \
  else if((schedule == kmp_sch_static || schedule == kmp_sch_static_chunked)  \
          && distributed) {                                                   \
    schedule = kmp_sch_static_hierarchy;                                      \
    DEBUG_ONE("Switching to hierarchical static scheduler\n");                \
  }                                                                           \
  else if(schedule == kmp_sch_dynamic_chunked && distributed) {               \
    schedule = kmp_sch_dynamic_chunked_hierarchy;                             \
    DEBUG_ONE("Switching to hierarchical dynamic scheduler\n");               \
  }                                                                           \
  else if(schedule == kmp_sch_hetprobe)                                       \
  {                                                                           \
    if(!distributed) {                                                        \
      schedule = kmp_sch_dynamic_chunked;                                     \
      DEBUG_ONE("Reverting to normal dynamic scheduler (not distributed)\n"); \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      TYPE probe_size = nthreads * chunk * st;                                \
      if(probe_size > (TYPE)((float)(ub - lb) * 0.25)) {                      \
        schedule = kmp_sch_static_hierarchy;                                  \
        DEBUG_ONE("Probe chunk too big (" SPEC "), reverting to "             \
                  "hierarchical static scheduler\n", probe_size);             \
      }                                                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  switch(schedule)                                                            \
  {                                                                           \
  case kmp_sch_static: /* Fall through */                                     \
  case kmp_sch_static_chunked:                                                \
    STATIC_INIT(lb, ub + 1, st, chunk);                                       \
    thr->ts.static_trip = 0;                                                  \
    break;                                                                    \
  case kmp_sch_static_hierarchy:                                              \
    STATIC_HIERARCHY_INIT(thr->popcorn_nid, lb, ub + 1, st, chunk);           \
    thr->ts.static_trip = 0;                                                  \
    break;                                                                    \
  case kmp_sch_dynamic_chunked:                                               \
    DYN_INIT(lb, ub + 1, st, chunk);                                          \
    break;                                                                    \
  case kmp_sch_dynamic_chunked_hierarchy:                                     \
    if(chunk <= 1) /* Auto-select dynamic chunk size */                       \
    {                                                                         \
      chunk = calc_chunk_size_##GOMP_TYPE(lb, ub, st, nthreads);              \
      DEBUG("__kmpc_dispatch_init_"#NAME": %d chunk"SPEC"\n", gtid, chunk);   \
    }                                                                         \
    DYN_HIERARCHY_INIT(thr->popcorn_nid, lb, ub + 1, st, chunk);              \
    break;                                                                    \
  case kmp_sch_hetprobe:                                                      \
    if(chunk <= 1) /* Auto-select probe size */                               \
    {                                                                         \
      chunk = calc_chunk_size_##GOMP_TYPE(lb, ub, st, nthreads);              \
      DEBUG("__kmpc_dispatch_init_"#NAME": %d chunk"SPEC"\n", gtid, chunk);   \
    }                                                                         \
    HETPROBE_INIT(thr->popcorn_nid, loc->psource, lb, ub + 1, st, chunk);     \
    break;                                                                    \
  default:                                                                    \
    assert(false && "Unknown scheduling algorithm");                          \
    break;                                                                    \
  }                                                                           \
}

__kmpc_dispatch_init(4, int32_t, " %d", long,
                     GOMP_loop_static_init,
                     hierarchy_init_workshare_static,
                     GOMP_loop_dynamic_init,
                     hierarchy_init_workshare_dynamic,
                     hierarchy_init_workshare_hetprobe)
__kmpc_dispatch_init(4u, uint32_t, " %u", ull,
                     GOMP_loop_ull_static_init,
                     hierarchy_init_workshare_static_ull,
                     GOMP_loop_ull_dynamic_init,
                     hierarchy_init_workshare_dynamic_ull,
                     hierarchy_init_workshare_hetprobe_ull)
__kmpc_dispatch_init(8, int64_t, " %ld", long,
                     GOMP_loop_static_init,
                     hierarchy_init_workshare_static,
                     GOMP_loop_dynamic_init,
                     hierarchy_init_workshare_dynamic,
                     hierarchy_init_workshare_hetprobe)
__kmpc_dispatch_init(8u, uint64_t, " %lu", ull,
                     GOMP_loop_ull_static_init,
                     hierarchy_init_workshare_static_ull,
                     GOMP_loop_ull_dynamic_init,
                     hierarchy_init_workshare_dynamic_ull,
                     hierarchy_init_workshare_hetprobe_ull)

/*
 * Mark the end of a dynamically scheduled loop.
 * @param loc source code location
 * @param gtid global thread ID of this thread
 */
#define __kmpc_dispatch_fini(NAME) \
void __kmpc_dispatch_fini_##NAME(ident_t *loc, int32_t gtid)                  \
{                                                                             \
  struct gomp_thread *thr = gomp_thread();                                    \
                                                                              \
  DEBUG("__kmpc_dispatch_fini_"#NAME": %s %d\n", loc->psource, gtid);         \
                                                                              \
  switch(thr->ts.work_share->sched)                                           \
  {                                                                           \
  case GFS_STATIC: /* Fall through */                                         \
  case GFS_DYNAMIC: GOMP_loop_end(); break;                                   \
  case GFS_HIERARCHY_STATIC:                                                  \
    hierarchy_loop_end(thr->popcorn_nid, loc->psource, false);                \
    break;                                                                    \
  case GFS_HIERARCHY_DYNAMIC: /* Fall through */                              \
  case GFS_HETPROBE:                                                          \
    hierarchy_loop_end(thr->popcorn_nid, loc->psource, true);                 \
    break;                                                                    \
  default:                                                                    \
    assert(false && "Unknown scheduling algorithm");                          \
  }                                                                           \
}

__kmpc_dispatch_fini(4)
__kmpc_dispatch_fini(4u)
__kmpc_dispatch_fini(8)
__kmpc_dispatch_fini(8u)

/*
 * Grab the next batch of iterations according to the previously initialized
 * work-sharing construct.
 * @param loc source code location
 * @param gtid global thread ID of this thread
 * @param p_last pointer to a flag set to 1 if this is the last chunk or 0
 *               otherwise
 * @param p_lb pointer to the lower bound
 * @param p_ub pointer to the upper bound
 * @param p_st (unused)
 */
#define __kmpc_dispatch_next(NAME, TYPE, GOMP_TYPE, SPEC,                     \
                             DYN_NEXT, DYN_LAST,                              \
                             DYN_HIERARCHY_NEXT,                              \
                             HETPROBE_NEXT,                                   \
                             HIERARCHY_LAST)                                  \
int __kmpc_dispatch_next_##NAME(ident_t *loc,                                 \
                                int32_t gtid,                                 \
                                int32_t *p_last,                              \
                                TYPE *p_lb,                                   \
                                TYPE *p_ub,                                   \
                                TYPE *p_st)                                   \
{                                                                             \
  bool ret;                                                                   \
  GOMP_TYPE istart, iend;                                                     \
  struct gomp_thread *thr = gomp_thread();                                    \
  int nid = thr->popcorn_nid;                                                 \
  struct gomp_work_share *ws = thr->ts.work_share;                            \
                                                                              \
  switch(ws->sched)                                                           \
  {                                                                           \
  case GFS_STATIC: /* Fall through */                                         \
  case GFS_HIERARCHY_STATIC: {                                                \
    if(thr->ts.static_trip)                                                   \
    {                                                                         \
      istart = iend = 0;                                                      \
      ret = false;                                                            \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      enum sched_type sch = ws->chunk_size > 1 ? kmp_sch_static_chunked :     \
                                                 kmp_sch_static;              \
      *p_lb = ws->next;                                                       \
      *p_ub = ws->end - 1;                                                    \
      __kmpc_for_static_init_##NAME(loc, gtid, sch, p_last, p_lb, p_ub, p_st, \
                                    ws->incr, ws->chunk_size);                \
      istart = *p_lb;                                                         \
      iend = *p_ub + 1;                                                       \
      ret = istart <= iend;                                                   \
      thr->ts.static_trip = 1;                                                \
    }                                                                         \
    break;                                                                    \
  }                                                                           \
  case GFS_DYNAMIC:                                                           \
    ret = DYN_NEXT(&istart, &iend);                                           \
    *p_last = DYN_LAST(iend);                                                 \
    break;                                                                    \
  case GFS_HIERARCHY_DYNAMIC:                                                 \
    ret = DYN_HIERARCHY_NEXT(nid, &istart, &iend);                            \
    *p_last = HIERARCHY_LAST(iend);                                           \
    break;                                                                    \
  case GFS_HETPROBE:                                                          \
    ret = HETPROBE_NEXT(nid, loc->psource, &istart, &iend);                   \
    *p_last = HIERARCHY_LAST(iend);                                           \
    break;                                                                    \
  default:                                                                    \
    assert(false && "Unknown scheduling algorithm");                          \
    break;                                                                    \
  }                                                                           \
                                                                              \
  *p_lb = istart;                                                             \
  *p_ub = iend - 1;                                                           \
  if(!ret)                                                                    \
  {                                                                           \
    *p_lb = 0;                                                                \
    *p_ub = 0;                                                                \
    *p_st = 0;                                                                \
    __kmpc_dispatch_fini_##NAME(loc, gtid);                                   \
  }                                                                           \
                                                                              \
  DEBUG("__kmpc_dispatch_next_"#NAME": %s %d %d %d %d"SPEC SPEC SPEC"\n",     \
        loc->psource, gtid, ret, ws->sched, *p_last, *p_lb, *p_ub, *p_st);    \
                                                                              \
  return ret;                                                                 \
}

__kmpc_dispatch_next(4, int32_t, long, " %d",
                     GOMP_loop_dynamic_next, gomp_iter_is_last,
                     hierarchy_next_dynamic, hierarchy_next_hetprobe,
                     hierarchy_last)
__kmpc_dispatch_next(4u, uint32_t, unsigned long long, " %u",
                     GOMP_loop_ull_dynamic_next, gomp_iter_is_last_ull,
                     hierarchy_next_dynamic_ull, hierarchy_next_hetprobe_ull,
                     hierarchy_last_ull)
__kmpc_dispatch_next(8, int64_t, long, " %ld",
                     GOMP_loop_dynamic_next, gomp_iter_is_last,
                     hierarchy_next_dynamic, hierarchy_next_hetprobe,
                     hierarchy_last)
__kmpc_dispatch_next(8u, uint64_t, unsigned long long, " %lu",
                     GOMP_loop_ull_dynamic_next, gomp_iter_is_last_ull,
                     hierarchy_next_dynamic_ull, hierarchy_next_hetprobe_ull,
                     hierarchy_last_ull)

/*
 * Start execution of an ordered construct.
 * @param loc source location information
 * @param gtid global thread number
 */
void __kmpc_ordered(ident_t *loc, int32_t gtid)
{
  DEBUG("__kmpc_ordered: %s %d\n", loc->psource, gtid);

  GOMP_ordered_start();
}

/*
 * End execution of an ordered construct.
 * @param loc source location information
 * @param gtid global thread number
 */
void __kmpc_end_ordered(ident_t *loc, int32_t gtid)
{
  DEBUG("__kmpc_end_ordered: %s %d\n", loc->psource, gtid);

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

int32_t __kmpc_single(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_single: %s %d\n", loc->psource, global_tid);

  return (int32_t)GOMP_single_start();
}

void __kmpc_end_single(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_end_single: %s %d\n", loc->psource, global_tid);
}

void __kmpc_flush(ident_t *loc)
{
  DEBUG("__kmpc_flush: %s\n", loc->psource);

  __sync_synchronize();
}

///////////////////////////////////////////////////////////////////////////////
// Synchronization
///////////////////////////////////////////////////////////////////////////////

/*
 * Barrier with cancellation point to send threads from the barrier to the end
 * of the parallel region.  Needs a special code pattern as documented in the
 * design document for the cancellation feature.
 * @param loc source location information
 * @param gtid global thread ID
 * @return returns true if a matching cancellation request has been flagged in
 *         the RTL and the encountering thread has to cancel.
 */
int32_t __kmpc_cancel_barrier(ident_t* loc, int32_t gtid)
{
  DEBUG("__kmpc_cancel_barrier: %s %d\n", loc->psource, gtid);

  if(popcorn_global.hybrid_barrier)
    return hierarchy_hybrid_cancel_barrier(gomp_thread()->popcorn_nid);
  else return GOMP_barrier_cancel();
}

/*
 * Execute a barrier.
 * @param loc source location information
 * @param global_tid thread id
 */
void __kmpc_barrier(ident_t *loc, int32_t global_tid)
{
  DEBUG("__kmpc_barrier: %s %d\n", loc->psource, global_tid);

  if(popcorn_global.hybrid_barrier)
    hierarchy_hybrid_barrier(gomp_thread()->popcorn_nid);
  else GOMP_barrier();
}

///////////////////////////////////////////////////////////////////////////////
// Reductions
///////////////////////////////////////////////////////////////////////////////

/*
 * The code generated by clang performs the following operations depending on
 * __kmpc_reduce*()'s return value:
 *
 *  1. Perform the reduction without any atomics or locking
 *
 *     - Used when there's only 1 thread in the team, or if multiple threads,
 *       they've been released from __kmpc_reduce*() after entering a critical
 *       section.  In the latter case, __kmpc_end_reduce() exits the critical
 *       section so other thread can make progress.
 *
 *  2. Perform the reduction using atomics
 *
 *  Default. Do nothing
 */

typedef void (*reduce_func)(void *lhs_data, void *rhs_data);

/*
 * Return the reduction method based on what the compiler generated.
 * @param loc source location information, including codegen flags
 * @param data argument data passed to reduce func (if available)
 * @param func function that implements reduction functionality (if available)
 * @return type of reduction to employ
 */
static inline enum reduction_method get_reduce_method(ident_t *loc,
                                                      void *data,
                                                      reduce_func func)
{
  enum reduction_method retval = critical_reduce_block;
  int teamsize_cutoff = 4, teamsize = omp_get_num_threads();
  bool atomic_available = FAST_REDUCTION_ATOMIC_METHOD_GENERATED(loc),
       tree_available = FAST_REDUCTION_TREE_METHOD_GENERATED(data, func) &&
                        popcorn_global.hybrid_reduce;

  // Note: adapted from the logic in __kmp_determine_reduction_method for
  // AArch64/PPC64/x86_64 on Linux
  if(teamsize == 1) retval = empty_reduce_block;
  else
  {
    if(tree_available) {
      if(teamsize <= teamsize_cutoff) {
        if(atomic_available) retval = atomic_reduce_block;
      }
      else retval = tree_reduce_block;
    }
    else if(atomic_available) retval = atomic_reduce_block;
  }

  return retval;
}

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
  struct gomp_thread *thr;

  DEBUG("__kmpc_reduce: %s %d %d %lu %p %p %p\n", loc->psource,
        global_tid, num_vars, reduce_size, reduce_data, func, lck);

  thr = gomp_thread();
  thr->reduction_method = get_reduce_method(loc, reduce_data, func);
  switch(thr->reduction_method)
  {
  case critical_reduce_block: GOMP_critical_start(); return 1;
  case atomic_reduce_block: return 2;
  case tree_reduce_block:
    if(hierarchy_reduce(thr->popcorn_nid, reduce_data, func)) return 1;
    else
    {
      /*
       * This thread is not the final thread, meaning we need to wait until the
       * final thread has finished all reductions.  Due to how clang emits
       * OpenMP calls, this thread *won't* call __kmpc_end_reduce(), so wait on
       * the barrier here.  The final thread will release after all reductions
       * have been completed.
       */
      __kmpc_barrier(loc, global_tid);
      return 0;
    }
  default: return 1;
  }
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
  struct gomp_thread *thr;

  DEBUG("__kmpc_end_reduce: %s %d %p\n", loc->psource, global_tid, lck);

  thr = gomp_thread();
  assert(thr->reduction_method != reduction_method_not_defined);
  if(thr->reduction_method == critical_reduce_block) GOMP_critical_end();
  thr->reduction_method = reduction_method_not_defined;
  __kmpc_barrier(loc, global_tid);
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
  struct gomp_thread *thr;

  DEBUG("__kmpc_reduce_nowait: %s %d %d %lu %p %p %p\n", loc->psource,
        global_tid, num_vars, reduce_size, reduce_data, func, lck);

  thr = gomp_thread();
  thr->reduction_method = get_reduce_method(loc, reduce_data, func);
  switch(thr->reduction_method)
  {
  case critical_reduce_block: GOMP_critical_start(); return 1;
  case atomic_reduce_block: return 2;
  case tree_reduce_block:
    return hierarchy_reduce(thr->popcorn_nid, reduce_data, func);
  default: return 1;
  }
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
  struct gomp_thread *thr;

  DEBUG("__kmpc_end_reduce_nowait: %s %d %p\n", loc->psource, global_tid, lck);

  thr = gomp_thread();
  assert(thr->reduction_method != reduction_method_not_defined);
  if(thr->reduction_method == critical_reduce_block) GOMP_critical_end();
  thr->reduction_method = reduction_method_not_defined;
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

  // TODO if we migrated the thread to a new node, move TLS heap data to new
  // node's heap

  /* Allocate (if necessary) & initialize this thread's data. */
  if((ret = (*cache)[global_tid]) == 0)
  {
    if(popcorn_distributed())
      ret = popcorn_malloc(size, gomp_thread()->popcorn_nid);
    else ret = malloc(size);

    assert(ret && "Could not allocate thread private data");
    memcpy(ret, data, size);
    (*cache)[global_tid] = ret;
  }

  return ret;
}

