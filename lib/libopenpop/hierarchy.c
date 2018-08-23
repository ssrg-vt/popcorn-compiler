/*
 * Provides hierarchy abstractions for threads executing in Popcorn Linux.
 *
 * Copyright Rob Lyerly, SSRG, VT, 2018
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <float.h>
#include "hierarchy.h"

global_info_t ALIGN_PAGE popcorn_global;
node_info_t ALIGN_PAGE popcorn_node[MAX_POPCORN_NODES];

///////////////////////////////////////////////////////////////////////////////
// Global information getters/setters
///////////////////////////////////////////////////////////////////////////////

bool popcorn_distributed() { return popcorn_global.distributed; }
bool popcorn_finished() { return popcorn_global.finished; }
bool popcorn_hybrid_barrier() { return popcorn_global.hybrid_barrier; }
bool popcorn_hybrid_reduce() { return popcorn_global.hybrid_reduce; }
bool popcorn_het_workshare() { return popcorn_global.het_workshare; }

unsigned long omp_popcorn_threads()
{
  int i;
  unsigned long num = 0;
  for(i = 0; i < MAX_POPCORN_NODES; i++)
    num += popcorn_global.threads_per_node[i];
  return num;
}

unsigned long omp_popcorn_threads_per_node(int nid)
{
  if(nid >= 0 && nid < MAX_POPCORN_NODES)
    return popcorn_global.threads_per_node[nid];
  else return UINT64_MAX;
}

unsigned long omp_popcorn_core_speed(int nid)
{
  if(nid >= 0 && nid < MAX_POPCORN_NODES)
    return popcorn_global.core_speed_rating[nid];
  else return UINT64_MAX;
}

void popcorn_set_distributed(bool flag) { popcorn_global.distributed = flag; }
void popcorn_set_finished(bool flag) { popcorn_global.finished = flag; }
void popcorn_set_hybrid_barrier(bool flag) { popcorn_global.hybrid_barrier = flag; }
void popcorn_set_hybrid_reduce(bool flag) { popcorn_global.hybrid_reduce = flag; }
void popcorn_set_het_workshare(bool flag) { popcorn_global.het_workshare = flag; }

///////////////////////////////////////////////////////////////////////////////
// Leader selection
///////////////////////////////////////////////////////////////////////////////

void hierarchy_init_global(int nodes)
{
  popcorn_global.sync.remaining = popcorn_global.sync.num =
  popcorn_global.opt.remaining = popcorn_global.opt.num = nodes;
  /* Note: *must* use reinit_all, otherwise there's a race condition between
     leaders who have been released are reading generation in the barrier's
     do-while loop and the main thread resetting barrier's generation */
  gomp_barrier_reinit_all(&popcorn_global.bar, nodes);
}

void hierarchy_init_node(int nid)
{
  size_t num = popcorn_global.threads_per_node[nid];
  popcorn_node[nid].sync.remaining = popcorn_node[nid].sync.num =
  popcorn_node[nid].opt.remaining = popcorn_node[nid].opt.num = num;
  /* See note in hierarchy_init_global() above */
  gomp_barrier_reinit_all(&popcorn_node[nid].bar, num);
}

int hierarchy_assign_node(unsigned tnum)
{
  unsigned cur = 0, thr_total = 0;
  for(cur = 0; cur < MAX_POPCORN_NODES; cur++)
  {
    thr_total += popcorn_global.node_places[cur];
    if(tnum < thr_total)
    {
      popcorn_global.threads_per_node[cur]++;
      return cur;
    }
  }

  /* If we've exhausted the specification default to origin */
  popcorn_global.threads_per_node[0]++;
  return 0;
}

/****************************** Internal APIs *******************************/

static bool select_leader_optimistic(leader_select_t *l, size_t *ticket)
{
  size_t rem = __atomic_fetch_add(&l->remaining, -1, MEMMODEL_ACQ_REL);
  if(ticket) *ticket = rem - 1;
  return rem == l->num;
}

static bool select_leader_synchronous(leader_select_t *l,
                                      gomp_barrier_t *bar,
                                      bool final,
                                      size_t *ticket)
{
  unsigned awaited;
  size_t rem = __atomic_add_fetch(&l->remaining, -1, MEMMODEL_ACQ_REL);
  if(ticket) *ticket = rem;
  if(rem) return false;
  else
  {
    /* Wait for non-leader threads to enter barrier */
    if(final)
    {
      do awaited = __atomic_load_n(&bar->awaited_final, MEMMODEL_ACQUIRE);
      while(awaited != 1);
    }
    else
    {
      do awaited = __atomic_load_n(&bar->awaited, MEMMODEL_ACQUIRE);
      while(awaited != 1);
    }
    return true;
  }
}

static void hierarchy_leader_cleanup(leader_select_t *l)
{
  __atomic_store_n(&l->remaining, l->num, MEMMODEL_RELEASE);
}

///////////////////////////////////////////////////////////////////////////////
// Barriers
///////////////////////////////////////////////////////////////////////////////

void hierarchy_hybrid_barrier(int nid, const char *desc)
{
  bool leader;

  leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                     &popcorn_node[nid].bar,
                                     false, NULL);
  if(leader)
  {
    gomp_team_barrier_wait_nospin(&popcorn_global.bar);
    hierarchy_leader_cleanup(&popcorn_node[nid].sync);
  }
  gomp_team_barrier_wait(&popcorn_node[nid].bar);
}

bool hierarchy_hybrid_cancel_barrier(int nid, const char *desc)
{
  bool ret = false, leader;

  leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                     &popcorn_node[nid].bar,
                                     false, NULL);
  if(leader)
  {
    ret = gomp_team_barrier_wait_cancel_nospin(&popcorn_global.bar);
    // TODO if cancelled at global level need to cancel local barrier
    hierarchy_leader_cleanup(&popcorn_node[nid].sync);
  }
  ret |= gomp_team_barrier_wait_cancel(&popcorn_node[nid].bar);
  return ret;
}

/* End-of-parallel section barriers are a little tricky because upon starting
   the next section the main thread will reset the per-node synchronization
   data.  We need to ensure that all non-leader threads reach the per-node
   barrier *before* performing global synchronization.  Once this is
   accomplished the leader can unconditionally release the threads waiting at
   the per-node barrier.  Note that this function *requires* the main thread
   to call hierarchy_init_node() upon starting the next section, as we leave
   the per-node barrier in an inconsistent state to avoid race conditions. */
void hierarchy_hybrid_barrier_final(int nid, const char *desc)
{
  bool leader;

  leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                     &popcorn_node[nid].bar,
                                     true, NULL);
  if(leader)
  {
    gomp_team_barrier_wait_final_nospin(&popcorn_global.bar);
    gomp_team_barrier_wait_final_last(&popcorn_node[nid].bar);
  }
  else gomp_team_barrier_wait_final(&popcorn_node[nid].bar);
}

///////////////////////////////////////////////////////////////////////////////
// Reductions
///////////////////////////////////////////////////////////////////////////////

static inline bool
hierarchy_reduce_leader(int nid,
                        void *reduce_data,
                        void (*reduce_func)(void *lhs, void *rhs))
{
  bool global_leader;
  size_t i, reduced = 1, nthreads = popcorn_node[nid].opt.num, max_entry;
  void *thr_data;

  /* First, reduce from all threads locally.  The basic strategy is to loop
     through all reduction entries, waiting for a thread to populate it with
     data.  Keep looping until all local threads have been combined. */
  max_entry = nthreads < REDUCTION_ENTRIES ? nthreads : REDUCTION_ENTRIES;
  while(reduced < nthreads)
  {
    // TODO only execute this a set number of times then donate leadership?
    for(i = 0; i < max_entry; i++)
    {
      thr_data = __atomic_load_n(&popcorn_node[nid].reductions[i].p,
                                 MEMMODEL_ACQUIRE);
      if(!thr_data) continue;

      reduce_func(reduce_data, thr_data);
      __atomic_store_n(&popcorn_node[nid].reductions[i].p, NULL,
                       MEMMODEL_RELEASE);
      reduced++;
      thr_data = NULL;
    }
  }

  /* Now, select a global leader & do the same thing on the global data. */
  global_leader = select_leader_optimistic(&popcorn_global.opt, NULL);
  if(global_leader)
  {
    reduced = 1;
    while(reduced < popcorn_global.opt.num)
    {
      for(i = 0; i < MAX_POPCORN_NODES; i++)
      {
        if(!popcorn_global.threads_per_node[i]) continue;
        thr_data = __atomic_load_n(&popcorn_global.reductions[i].p,
                                   MEMMODEL_ACQUIRE);
        if(!thr_data) continue;

        reduce_func(reduce_data, thr_data);
        __atomic_store_n(&popcorn_global.reductions[i].p, NULL,
                         MEMMODEL_RELEASE);
        reduced++;
        thr_data = NULL;
      }
    }
    hierarchy_leader_cleanup(&popcorn_global.opt);
  }
  else /* Each node should get its own reduction entry, no need to loop */
    __atomic_store_n(&popcorn_global.reductions[nid].p, reduce_data,
                     MEMMODEL_RELEASE);
  return global_leader;
}

static inline void
hierarchy_reduce_local(int nid, size_t ticket, void *reduce_data)
{
  bool set = false;
  void *expected;
  unsigned long long i;

  /* All we need to do is make our reduction data available to the per-node
     leader (it's the leader's job to clean up).  However, we could be sharing
     a reduction entry on manycore machines so spin until it's open. */
  ticket %= REDUCTION_ENTRIES;
  while(true)
  {
    expected = NULL;
    set = __atomic_compare_exchange_n(&popcorn_node[nid].reductions[ticket].p,
                                      &expected,
                                      reduce_data,
                                      false,
                                      MEMMODEL_ACQ_REL,
                                      MEMMODEL_RELAXED);
    if(set) break;

    /* Spin for a bit (adapted from "config/linux/wait.h") */
    for(i = 0; i < gomp_spin_count_var; i++)
    {
      if(__atomic_load_n(&popcorn_node[nid].reductions[ticket].p,
                         MEMMODEL_RELAXED) == NULL) break;
      else __asm volatile("" : : : "memory");
    }
  }
}

bool hierarchy_reduce(int nid,
                      void *reduce_data,
                      void (*reduce_func)(void *lhs, void *rhs))
{
  bool leader;
  size_t ticket;

  leader = select_leader_optimistic(&popcorn_node[nid].opt, &ticket);
  if(leader)
  {
    leader = hierarchy_reduce_leader(nid, reduce_data, reduce_func);
    hierarchy_leader_cleanup(&popcorn_node[nid].opt);
  }
  else hierarchy_reduce_local(nid, ticket, reduce_data);
  return leader;
}

///////////////////////////////////////////////////////////////////////////////
// Work sharing
///////////////////////////////////////////////////////////////////////////////

/**************************** Hash table APIs ********************************/

// TODO are the malloc uses here causing interference?  I think so...

/* Cache heterogeneous probing results.  Can be configured to eliminate the
   need to continue probing for previously-seen regions. */
#define _CACHE_HETPROBE

/* Core speed ratings for a particular work-sharing region */
typedef struct workshare_csr
{
  const void *ident;
  size_t trips;
  union {
    long remaining;
    unsigned long long remaining_ull;
  };
  union {
    long chunk_size;
    unsigned long long chunk_size_ull;
  };
  float uspf;
  float scaled_thread_range;
  float core_speed_rating[MAX_POPCORN_NODES];
} workshare_csr_t;

typedef workshare_csr_t *hash_entry_type;
static inline void *htab_alloc(size_t size) { return malloc(size); }
static inline void htab_free(void *ptr) { free(ptr); }

#include "hashtab.h"

static inline hashval_t htab_hash(hash_entry_type element)
{ return hash_pointer(element->ident); }
static inline bool htab_eq(hash_entry_type a, hash_entry_type b)
{ return a->ident == b->ident; }

static inline hash_entry_type new_hash_value(const void *ident)
{
  /* Note: don't use a node-specific malloc as this will probably be
     read/updated on multiple nodes */
  hash_entry_type new_val = (hash_entry_type)malloc(sizeof(workshare_csr_t));
  new_val->ident = ident;
  new_val->trips = 0;
  new_val->remaining = 0;
  new_val->chunk_size = 0;
  new_val->uspf = 0.0;
  new_val->scaled_thread_range = 0.0;
  memset(&new_val->core_speed_rating, 0, sizeof(float) * MAX_POPCORN_NODES);
  return new_val;
}

void popcorn_init_workshare_cache(size_t size)
{ popcorn_global.workshare_cache = htab_create(size); }

size_t popcorn_max_probes;
const char *popcorn_prime_region;
int popcorn_preferred_node;

#ifndef _CACHE_HETPROBE
/* If not using a cache, use a single global core speed rating struct which
   gets updated every probing period. */
static workshare_csr_t global_csr;
#endif

static hash_entry_type get_entry(const void *ident)
{
  workshare_csr_t tmp = { .ident = ident };
  return htab_find(popcorn_global.workshare_cache, &tmp);
}

static hash_entry_type get_or_create_entry(const void *ident, bool *new)
{
  hash_entry_type ret;
  workshare_csr_t tmp = { .ident = ident };

  if(!ident)
  {
    popcorn_log("Somebody sent me a bad identity\n");
    assert(false && "bad identity");
  }

  *new = false;
  ret = htab_find(popcorn_global.workshare_cache, &tmp);
  if(ret == HTAB_EMPTY_ENTRY) /* First time seeing the region */
  {
    ret = new_hash_value(ident);
    *htab_find_slot(&popcorn_global.workshare_cache, &tmp, INSERT) = ret;
    *new = true;
  }
  return ret;
}

/*********************** Work splitting helper APIs **************************/

void popcorn_get_page_faults(unsigned long long *sent,
                             unsigned long long *recv)
{
  unsigned long long i;
  FILE *fp;

  assert(sent && recv && "Invalid arguments to get_page_faults()");

  if((fp = fopen("/proc/popcorn_stat", "r")))
  {
    /* Skip header statistics & irrelevant message types */
    while(fgetc(fp) != '-');
    for(i = 0; i < 10; )
      if(fgetc(fp) == '\n') i++;
    fscanf(fp, " %llu %llu", sent, recv);
    fclose(fp);
  }
  else
  {
    *sent = 0;
    *recv = 0;
  }
}

static void init_statistics(int nid)
{
  unsigned long long sent, recv;
  popcorn_get_page_faults(&sent, &recv);
  popcorn_node[nid].page_faults = sent;
  popcorn_node[nid].workshare_time = 0;
}

static inline void loop_init(struct gomp_work_share *ws,
                             long start,
                             long end,
                             long incr,
                             enum gomp_schedule_type sched,
                             long chunk_size,
                             int nid)
{
  ws->sched = sched;
  ws->chunk_size = chunk_size * incr;
  /* Canonicalize loops that have zero iterations to ->next == ->end.  */
  ws->end = ((incr > 0 && start > end) || (incr < 0 && start < end))
            ? start : end;
  ws->incr = incr;
  ws->next = start;

#ifdef HAVE_SYNC_BUILTINS
  {
    /* For dynamic scheduling prepare things to make each iteration faster. */
    long nthreads = popcorn_global.threads_per_node[nid];
    if(__builtin_expect (incr > 0, 1))
    {
      /* Cheap overflow protection.  */
      if(__builtin_expect ((nthreads | ws->chunk_size) >=
                           1UL << (sizeof (long) * __CHAR_BIT__ / 2 - 1), 0))
        ws->mode = 0;
      else
        ws->mode = ws->end < (LONG_MAX - (nthreads + 1) * ws->chunk_size);
    }
    /* Cheap overflow protection.  */
    else if(__builtin_expect ((nthreads | -ws->chunk_size)
                              >= 1UL << (sizeof (long)
                                         * __CHAR_BIT__ / 2 - 1), 0))
      ws->mode = 0;
    else
      ws->mode = ws->end > (nthreads + 1) * -ws->chunk_size - LONG_MAX;
  }
#endif
}

static inline void loop_init_ull(struct gomp_work_share *ws,
                                 bool up,
                                 unsigned long long start,
                                 unsigned long long end,
                                 unsigned long long incr,
                                 enum gomp_schedule_type sched,
                                 unsigned long long chunk_size,
                                 int nid)
{
  ws->sched = sched;
  ws->chunk_size_ull = chunk_size * incr;
  /* Canonicalize loops that have zero iterations to ->next == ->end.  */
  ws->end_ull = ((up && start > end) || (!up && start < end))
                ? start : end;
  ws->incr_ull = incr;
  ws->next_ull = start;
  ws->mode = 0;

#if defined HAVE_SYNC_BUILTINS && defined __LP64__
  {
    /* For dynamic scheduling prepare things to make each iteration
       faster.  */
    long nthreads = popcorn_global.threads_per_node[nid];
    if(__builtin_expect (up, 1))
    {
      /* Cheap overflow protection.  */
      if(__builtin_expect ((nthreads | ws->chunk_size_ull)
                           < 1ULL << (sizeof (unsigned long long)
                                      * __CHAR_BIT__ / 2 - 1), 1))
        ws->mode = ws->end_ull < (__LONG_LONG_MAX__ * 2ULL + 1
                                  - (nthreads + 1) * ws->chunk_size_ull);
    }
    /* Cheap overflow protection.  */
    else if(__builtin_expect ((nthreads | -ws->chunk_size_ull)
                              < 1ULL << (sizeof (unsigned long long)
                                         * __CHAR_BIT__ / 2 - 1), 1))
      ws->mode = ws->end_ull > ((nthreads + 1) * -ws->chunk_size_ull
                                - (__LONG_LONG_MAX__ * 2ULL + 1));
  }
#endif
  if(!up)
    ws->mode |= 2;
}

/* Shorthands for rounding numbers */
#define ROUND( type, val, incr ) \
  { \
    type remainder = val % incr; \
    val = remainder ? (val + incr - remainder) : val; \
  }
#define ROUND_LONG( val, incr ) ROUND(long, val, incr)
#define ROUND_ULL( val, incr ) ROUND(unsigned long long, val, incr)

/* Calculate iteration splits between nodes for the remaining parallel work.
   This is done by the global leader, as having threads accurately calculating
   boundary conditions on loop iteration counts using floating point numbers is
   hard. For each node nid:

     popcorn_global.split[nid]   = node nid's starting iteration
     popcorn_global.split[nid+1] = node nid's ending iteration

   This function also updates the work share to be depleted. */
static int calculate_splits(workshare_csr_t *csr, struct gomp_work_share *ws)
{
  int i, max_node = 0;
  float split_range = 0.0, remaining;

  csr->remaining = ws->end - ws->next;
  remaining = (float)csr->remaining;
  popcorn_global.split[0] = ws->next;
  for(i = 1; i < MAX_POPCORN_NODES; i++)
  {
    if(popcorn_global.threads_per_node[i])
    {
      split_range += csr->core_speed_rating[i-1] *
                     popcorn_global.threads_per_node[i-1];
      popcorn_global.split[i] = ws->next +
        (split_range / csr->scaled_thread_range) * remaining;
      ROUND_LONG(popcorn_global.split[i], ws->incr);
      max_node = i;
    }
    else popcorn_global.split[i] = popcorn_global.split[i-1];
  }
  popcorn_global.split[max_node+1] = ws->end;
  ws->next = ws->end;

  return max_node;
}

static inline long
calc_chunk_from_ratio(int nid, long incr, workshare_csr_t *csr)
{
  long chunk;
  float one_thread_percent = csr->core_speed_rating[nid] /
                             csr->scaled_thread_range;
  /* Note: round up because it's better to slightly overestimate loop count
     distributions (which will get corrected when grabbing work) instead of
     underestimating and forcing another round of global work distribution */
  chunk = ceilf(one_thread_percent * (float)csr->remaining);
  ROUND_LONG(chunk, incr);
  return chunk;
}

static int
calculate_splits_ull(workshare_csr_t *csr, struct gomp_work_share *ws)
{
  int i, max_node = 0;
  float split_range = 0.0, remaining;

  csr->remaining_ull = ws->end_ull - ws->next_ull;
  remaining = (float)csr->remaining_ull;
  popcorn_global.split_ull[0] = ws->next_ull;
  for(i = 1; i < MAX_POPCORN_NODES; i++)
  {
    if(!popcorn_global.threads_per_node[i]) continue;
    split_range += csr->core_speed_rating[i-1] *
                   popcorn_global.threads_per_node[i-1];
    popcorn_global.split_ull[i] = ws->next_ull +
      (split_range / csr->scaled_thread_range) * remaining;
    ROUND_ULL(popcorn_global.split_ull[i], ws->incr_ull);
    max_node = i;
  }
  popcorn_global.split[max_node+1] = ws->end_ull;
  ws->next_ull = ws->end_ull;

  return max_node;
}

static inline unsigned long long
calc_chunk_from_ratio_ull(int nid,
                          unsigned long long incr,
                          workshare_csr_t *csr)
{
  unsigned long long chunk;
  float one_thread_percent = csr->core_speed_rating[nid] /
                             csr->scaled_thread_range;
  chunk = ceilf(one_thread_percent * (float)csr->remaining_ull);
  ROUND_ULL(chunk, incr);
  return chunk;
}

/* Signal core speed value indicating that a particular node shouldn't receive
   any iterations. */
#define NO_ITER FLT_MIN

static void init_workshare_from_splits(int nid,
                                       workshare_csr_t *csr,
                                       struct gomp_work_share *ws)
{
  if(csr->core_speed_rating[nid] == NO_ITER)
  {
    /* The scheduler decided not to give this node any iterations, set work
       share so threads on this node go to ending barrier. */
    ws->chunk_size = LONG_MAX;
    ws->next = ws->end = popcorn_global.ws.end;
  }
  else
  {
    ws->next = popcorn_global.split[nid];
    ws->end = popcorn_global.split[nid+1];
    ws->chunk_size = calc_chunk_from_ratio(nid, ws->incr, csr);
  }
  ws->sched = GFS_HIERARCHY_DYNAMIC;
}

static void init_workshare_from_splits_ull(int nid,
                                           workshare_csr_t *csr,
                                           struct gomp_work_share *ws)
{
  if(csr->core_speed_rating[nid] == NO_ITER)
  {
    ws->chunk_size_ull = ULLONG_MAX;
    ws->next_ull = ws->end_ull = popcorn_global.ws.end_ull;
  }
  else
  {
    ws->next_ull = popcorn_global.split_ull[nid];
    ws->end_ull = popcorn_global.split_ull[nid+1];
    ws->chunk_size_ull = calc_chunk_from_ratio_ull(nid, ws->incr_ull, csr);
  }
  ws->sched = GFS_HIERARCHY_DYNAMIC;
}

/* Whether or not to dump execution statistics like page faults &
   per-node execution times. */
bool popcorn_log_statistics;

// Note: the logging currently assumes only 1 thread is calling at a time!
// TODO printing floating point operations on aarch64 when starting on x86-64
// is currently not supported (due to software-emulated FP operations)
static char buf[2048];
#define REMAINING_BUF( buf, ptr ) (sizeof(buf) - ((ptr) - (buf)))
static void log_hetprobe_results(const char *ident, workshare_csr_t *csr)
{
  int i;
  char *cur = buf;

  // TODO un-do hardcoding of max 2
  const int max = 2;

  cur += snprintf(cur, sizeof(buf), "%s\nCSR:",
                  ident ? ident : "(no identifier)");
  for(i = 0; i < max; i++)
  {
    if(i && !(i % 8)) cur += snprintf(cur, REMAINING_BUF(buf, cur), "\n");
    cur += snprintf(cur, REMAINING_BUF(buf, cur), "\t%.3f",
                    csr->core_speed_rating[i]);
  }

  cur += snprintf(cur, REMAINING_BUF(buf, cur), "\nTimes:");
  for(i = 0; i < max; i++)
  {
    if(i && !(i % 8)) cur += snprintf(cur, REMAINING_BUF(buf, cur), "\n");
    cur += snprintf(cur, REMAINING_BUF(buf, cur), "\t%llu",
                    popcorn_global.workshare_time[i]);
  }

  cur += snprintf(cur, REMAINING_BUF(buf, cur), "\nFaults:");
  for(i = 0; i < max; i++)
  {
    if(i && !(i % 8)) cur += snprintf(cur, REMAINING_BUF(buf, cur), "\n");
    cur += snprintf(cur, REMAINING_BUF(buf, cur), "\t%llu",
                    popcorn_global.page_faults[i]);
  }

  snprintf(cur, REMAINING_BUF(buf, cur),
           "\nProbe: %ld / %llu probe iters/thread, "
           "%ld / %llu remaining, %.3f us/fault\n",
           csr->chunk_size, csr->chunk_size_ull,
           csr->remaining, csr->remaining_ull,
           csr->uspf);
  popcorn_log(buf);
}

void hierarchy_init_statistics(int nid)
{
  struct gomp_thread *thr = gomp_thread();
  bool leader;

  leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                     &popcorn_node[nid].bar,
                                     false, NULL);
  if(leader)
  {
    init_statistics(nid);
    hierarchy_leader_cleanup(&popcorn_node[nid].sync);
  }
  gomp_team_barrier_wait(&popcorn_node[nid].bar);
  clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
}

static workshare_csr_t dummy_csr;

void hierarchy_log_statistics(int nid, const void *ident)
{
  struct gomp_thread *thr = gomp_thread();
  struct timespec region_end;
  bool leader;
  unsigned long long sent, recv;

  clock_gettime(CLOCK_MONOTONIC, &region_end);
  __atomic_add_fetch(&popcorn_node[nid].workshare_time,
                     ELAPSED(thr->probe_start, region_end) / 1000,
                     MEMMODEL_ACQ_REL);
  leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                     &popcorn_node[nid].bar,
                                     false, NULL);
  if(leader)
  {
    popcorn_node[nid].workshare_time /= popcorn_global.threads_per_node[nid];
    popcorn_get_page_faults(&sent, &recv);
    popcorn_node[nid].page_faults = sent - popcorn_node[nid].page_faults;
    hierarchy_leader_cleanup(&popcorn_node[nid].sync);
  }
  gomp_team_barrier_wait(&popcorn_node[nid].bar);

  // TODO remove 2nd node hardcoded ID
  // TODO this races with threads starting a new work-sharing region
  if(thr->ts.team_id == 0 || thr->ts.team_id == 16)
    popcorn_log("%s / %d: %llu us, %llu faults\n", ident, nid,
                popcorn_node[nid].workshare_time,
                popcorn_node[nid].page_faults);
}

/*********************** Public Work splitting APIs **************************/

// TODO these don't support nested work sharing regions

void hierarchy_init_workshare_static(int nid,
                                     long long lb,
                                     long long ub,
                                     long long incr,
                                     long long chunk)
{
  struct gomp_thread *thr = gomp_thread();
  struct gomp_work_share *ws;

  ws = gomp_ptrlock_get(&popcorn_node[nid].ws_lock);
  if(ws == NULL)
  {
    ws = &popcorn_node[nid].ws;
    gomp_init_work_share(ws, false, popcorn_global.threads_per_node[nid]);
    loop_init(ws, lb, ub, incr, GFS_HIERARCHY_STATIC, chunk, nid);
    if(popcorn_log_statistics) init_statistics(nid);
    gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
  }
  if(popcorn_log_statistics) clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
  thr->ts.work_share = ws;
}

void hierarchy_init_workshare_static_ull(int nid,
                                         unsigned long long lb,
                                         unsigned long long ub,
                                         unsigned long long incr,
                                         unsigned long long chunk)
{
  struct gomp_thread *thr = gomp_thread();
  struct gomp_work_share *ws;

  ws = gomp_ptrlock_get(&popcorn_node[nid].ws_lock);
  if(ws == NULL)
  {
    ws = &popcorn_node[nid].ws;
    gomp_init_work_share(ws, false, popcorn_global.threads_per_node[nid]);
    loop_init_ull(ws, true, lb, ub, incr, GFS_HIERARCHY_STATIC, chunk, nid);
    if(popcorn_log_statistics) init_statistics(nid);
    gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
  }
  if(popcorn_log_statistics) clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
  thr->ts.work_share = ws;
}

void hierarchy_init_workshare_dynamic(int nid,
                                      long long lb,
                                      long long ub,
                                      long long incr,
                                      long long chunk)
{
  struct gomp_thread *thr = gomp_thread();
  struct gomp_team *team = thr->ts.team;
  int nthreads = team ? team->nthreads : 1;
  struct gomp_work_share *ws, *global;

  ws = gomp_ptrlock_get(&popcorn_node[nid].ws_lock);
  if(ws == NULL)
  {
    /* Note: initialize the local work-share to be finished so threads grab
       the next batch from the global pool immediately.  This is because we
       don't know where each node's pool of work starts/ends. */
    ws = &popcorn_node[nid].ws;
    gomp_init_work_share(ws, false, popcorn_global.threads_per_node[nid]);
    loop_init(ws, lb, lb, incr, GFS_HIERARCHY_DYNAMIC, chunk, nid);
    if(popcorn_log_statistics) init_statistics(nid);
    global = gomp_ptrlock_get(&popcorn_global.ws_lock);
    if(global == NULL)
    {
      global = &popcorn_global.ws;
      gomp_init_work_share(global, false, nthreads);
      loop_init(global, lb, ub, incr, GFS_HIERARCHY_DYNAMIC, chunk, nid);
      gomp_ptrlock_set(&popcorn_global.ws_lock, global);
    }
    gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
  }
  if(popcorn_log_statistics) clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
  thr->ts.work_share = ws;
}

void hierarchy_init_workshare_dynamic_ull(int nid,
                                          unsigned long long lb,
                                          unsigned long long ub,
                                          unsigned long long incr,
                                          unsigned long long chunk)
{
  struct gomp_thread *thr = gomp_thread();
  struct gomp_team *team = thr->ts.team;
  int nthreads = team ? team->nthreads : 1;
  struct gomp_work_share *ws, *global;

  ws = gomp_ptrlock_get(&popcorn_node[nid].ws_lock);
  if(ws == NULL)
  {
    ws = &popcorn_node[nid].ws;
    gomp_init_work_share(ws, false, popcorn_global.threads_per_node[nid]);
    loop_init_ull(ws, true, lb, lb, incr, GFS_HIERARCHY_DYNAMIC, chunk, nid);
    if(popcorn_log_statistics) init_statistics(nid);
    global = gomp_ptrlock_get(&popcorn_global.ws_lock);
    if(global == NULL)
    {
      global = &popcorn_global.ws;
      gomp_init_work_share(global, false, nthreads);
      loop_init_ull(global, true, lb, ub, incr, GFS_HIERARCHY_DYNAMIC,
                    chunk, nid);
      gomp_ptrlock_set(&popcorn_global.ws_lock, global);
    }
    gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
  }
  if(popcorn_log_statistics) clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
  thr->ts.work_share = ws;
}

void hierarchy_init_workshare_hetprobe(int nid,
                                       const void *ident,
                                       long long lb,
                                       long long ub,
                                       long long incr,
                                       long long chunk)
{
  struct gomp_thread *thr = gomp_thread();
  struct gomp_team *team = thr->ts.team;
  int nthreads = team ? team->nthreads : 1;
  struct gomp_work_share *ws, *global;
#ifdef _CACHE_HETPROBE
  bool new_ent;
  hash_entry_type ent = NULL;
#endif

  if(popcorn_global.popcorn_killswitch)
  {
    /* Somebody hit the distributed execution killswitch, only give work to the
       preferred node. */
    if(nid == popcorn_preferred_node)
      hierarchy_init_workshare_static(nid, lb, ub, incr, 1);
    else
      hierarchy_init_workshare_static(nid, ub + incr, ub, incr, 1);
    thr->ts.static_trip = 0;
    return;
  }

  ws = gomp_ptrlock_get(&popcorn_node[nid].ws_lock);
  if(ws == NULL)
  {
    ws = &popcorn_node[nid].ws;
    gomp_init_work_share(ws, false, popcorn_global.threads_per_node[nid]);
    loop_init(ws, lb, lb, incr, GFS_HETPROBE, chunk, nid);
    global = gomp_ptrlock_get(&popcorn_global.ws_lock);
    if(global == NULL)
    {
      global = &popcorn_global.ws;
      gomp_init_work_share(global, false, nthreads);
      loop_init(global, lb, ub, incr, GFS_HETPROBE, chunk, nid);
#ifdef _CACHE_HETPROBE
      ent = get_or_create_entry(ident, &new_ent);
      ent->chunk_size = chunk;
      if(!new_ent) /* Hey we've seen you before! */
      {
        if(ent->trips >= popcorn_max_probes)
        {
          calculate_splits(ent, global);
          global->sched = GFS_HIERARCHY_DYNAMIC;
        }
        else ent->trips++;
      }
#endif
      gomp_ptrlock_set(&popcorn_global.ws_lock, global);
    }
#ifdef _CACHE_HETPROBE
    if(global->sched == GFS_HIERARCHY_DYNAMIC)
    {
      /* We've seen the region enough times, no more probing */
      if(!ent) ent = get_entry(ident);
      assert(ent && "Missing cache entry");
      init_workshare_from_splits(nid, ent, ws);
      if(popcorn_log_statistics) init_statistics(nid);
    }
    else init_statistics(nid);
#else
    init_statistics(nid);
#endif
    gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
  }
  thr->ts.work_share = ws;
  thr->ts.static_trip = 0;
  clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
}

void hierarchy_init_workshare_hetprobe_ull(int nid,
                                           const void *ident,
                                           unsigned long long lb,
                                           unsigned long long ub,
                                           unsigned long long incr,
                                           unsigned long long chunk)
{
  struct gomp_thread *thr = gomp_thread();
  struct gomp_team *team = thr->ts.team;
  int nthreads = team ? team->nthreads : 1;
  struct gomp_work_share *ws, *global;
#ifdef _CACHE_HETPROBE
  bool new_ent;
  hash_entry_type ent = NULL;
#endif

  if(popcorn_global.popcorn_killswitch)
  {
    if(nid == popcorn_preferred_node)
      hierarchy_init_workshare_static_ull(nid, lb, ub, incr, chunk);
    else
      hierarchy_init_workshare_static_ull(nid, ub + incr, ub, incr, chunk);
    thr->ts.static_trip = 0;
    return;
  }

  ws = gomp_ptrlock_get(&popcorn_node[nid].ws_lock);
  if(ws == NULL)
  {
    ws = &popcorn_node[nid].ws;
    gomp_init_work_share(ws, false, popcorn_global.threads_per_node[nid]);
    loop_init_ull(ws, true, lb, lb, incr, GFS_HETPROBE, chunk, nid);
    global = gomp_ptrlock_get(&popcorn_global.ws_lock);
    if(global == NULL)
    {
      global = &popcorn_global.ws;
      gomp_init_work_share(global, false, nthreads);
      loop_init_ull(global, true, lb, ub, incr, GFS_HETPROBE, chunk, nid);
#ifdef _CACHE_HETPROBE
      ent = get_or_create_entry(ident, &new_ent);
      ent->chunk_size_ull = chunk;
      if(!new_ent) /* Hey we've seen you before! */
      {
        if(ent->trips >= popcorn_max_probes)
        {
          calculate_splits_ull(ent, global);
          global->sched = GFS_HIERARCHY_DYNAMIC;
        }
        else ent->trips++;
      }
#endif
      gomp_ptrlock_set(&popcorn_global.ws_lock, global);
    }
#ifdef _CACHE_HETPROBE
    if(global->sched == GFS_HIERARCHY_DYNAMIC)
    {
      if(!ent) ent = get_entry(ident);
      assert(ent && "Missing cache entry");
      init_workshare_from_splits_ull(nid, ent, ws);
      if(popcorn_log_statistics) init_statistics(nid);
    }
    else init_statistics(nid);
#else
    init_statistics(nid);
#endif
    gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
  }
  thr->ts.work_share = ws;
  thr->ts.static_trip = 0;
  clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
}

bool hierarchy_next_dynamic(int nid, long *start, long *end)
{
  bool ret;
  struct gomp_thread *thr = gomp_thread();
  struct gomp_work_share *ws = thr->ts.work_share;
  long chunk;

  /* *Must* use the locked versions to avoid racing against the leader when
     replenishing work from the global pool */
  gomp_mutex_lock(&ws->lock);
  ret = gomp_iter_dynamic_next_locked_ws(start, end, ws);
  if(!ret && !ws->threads_completed)
  {
    /* Local work share is out of work to distribute; replenish from global */
    chunk = ws->chunk_size * popcorn_global.threads_per_node[nid];
    ret = gomp_iter_dynamic_next_raw(&ws->next, &ws->end,
                                     &popcorn_global.ws, chunk);
    if(ret) ret = gomp_iter_dynamic_next_locked_ws(start, end, ws);
    else ws->threads_completed = true;
  }
  gomp_mutex_unlock(&ws->lock);

  return ret;
}

bool hierarchy_next_dynamic_ull(int nid,
                                unsigned long long *start,
                                unsigned long long *end)
{
  bool ret;
  struct gomp_thread *thr = gomp_thread();
  struct gomp_work_share *ws = thr->ts.work_share;
  unsigned long long chunk;

  gomp_mutex_lock(&ws->lock);
  ret = gomp_iter_ull_dynamic_next_locked_ws(start, end, ws);
  if(!ret && !ws->threads_completed)
  {
    chunk = ws->chunk_size_ull * popcorn_global.threads_per_node[nid];
    ret = gomp_iter_ull_dynamic_next_raw(&ws->next_ull, &ws->end_ull,
                                         &popcorn_global.ws, chunk);
    if(ret) ret = gomp_iter_ull_dynamic_next_locked_ws(start, end, ws);
    else ws->threads_completed = true;
  }
  gomp_mutex_unlock(&ws->lock);

  return ret;
}

static float calc_avg_us_per_pf()
{
  int i;
  unsigned long long cur_elapsed;
  float nthreads = gomp_thread()->ts.team->nthreads, uspf, avg_uspf = 0.0;

  /* Weight microseconds per fault (uspf) based on the number of threads */
  for(i = 0; i < MAX_POPCORN_NODES; i++)
  {
    cur_elapsed = popcorn_global.workshare_time[i];
    if(cur_elapsed)
    {
      uspf = (float)cur_elapsed / (float)popcorn_global.page_faults[i];
      avg_uspf +=
        uspf * ((float)popcorn_global.threads_per_node[i] / nthreads);
    }
  }

  return avg_uspf;
}

static inline float time_weighted_average(float cur, float prev, bool first)
{
  if(first) return cur;
  else return (0.75 * cur) + (0.25 * prev);
}

#define MAX( a, b ) ((a) > (b) ? (a) : (b))

// TODO this is ugly, refactor
static void calc_het_probe_workshare(int nid, bool ull, workshare_csr_t *csr)
{
  bool leader, calc_csr = true;
  size_t i, max_idx;
  unsigned long long cur_elapsed, min = UINT64_MAX, max = 0, sent, recv;
  float scale, cur_rating;

  /* Calculate this node's average time & page faults */
  popcorn_global.workshare_time[nid] =
    MAX(popcorn_node[nid].workshare_time /
        popcorn_global.threads_per_node[nid], 1);
  popcorn_get_page_faults(&sent, &recv);
  popcorn_global.page_faults[nid] = sent - popcorn_node[nid].page_faults;

  leader = select_leader_synchronous(&popcorn_global.sync,
                                     &popcorn_global.bar,
                                     false, NULL);
  if(leader)
  {
    csr->uspf =
      time_weighted_average(calc_avg_us_per_pf(), csr->uspf, csr->trips);

    /* If we've reached max probes, make a determination -- are we going to run
       across nodes or not? */
    if(csr->trips >= popcorn_max_probes && popcorn_prime_region &&
       strcmp(csr->ident, popcorn_prime_region) == 0)
    {
      if(csr->uspf <= 100.0)
      {
        /* It's not worth it -- use only the preferred node.  Note that we
           still need to set up the CSR for the remaining iterations in this
           work-sharing region.  Also set the global core speed rating, as
           the next hetprobe region will default to the static scheduler. */
        calc_csr = false;
        popcorn_global.popcorn_killswitch = true;
        popcorn_global.het_workshare = true;
        for(i = 0; i < MAX_POPCORN_NODES; i++)
        {
          if(i == popcorn_preferred_node)
          {
            popcorn_global.core_speed_rating[i] = 1;
            popcorn_global.scaled_thread_range =
              popcorn_global.threads_per_node[i];
            csr->core_speed_rating[i] = 1.0;
            csr->scaled_thread_range = popcorn_global.threads_per_node[i];
          }
          else
          {
            popcorn_global.core_speed_rating[i] = 0;
            csr->core_speed_rating[i] = 0.0;
          }
        }

        popcorn_log("%s: us per fault < 100, only executing on node %d\n",
                    csr->ident, popcorn_preferred_node);
      }
    }

    if(calc_csr)
    {
      /* Find the min & max values for scaling */
      for(i = 0; i < MAX_POPCORN_NODES; i++)
      {
        cur_elapsed = popcorn_global.workshare_time[i];
        if(cur_elapsed)
        {
          if(cur_elapsed < min) min = cur_elapsed;
          if(cur_elapsed > max)
          {
            max = cur_elapsed;
            max_idx = i;
          }
        }
      }

      /* Calculate core speed ratings based on ratio of each nodes' probe time
         to the minimum time. Also, accumulate page faults from all nodes. */
      csr->scaled_thread_range = 0.0;
      scale = 1.0 / ((float)min / (float)popcorn_global.workshare_time[max_idx]);
      for(i = 0; i < MAX_POPCORN_NODES; i++)
      {
        cur_elapsed = popcorn_global.workshare_time[i];
        if(cur_elapsed)
        {
          /* Update CSRs based on an exponentially-weighted moving average */
          cur_rating = (float)min / (float)cur_elapsed * scale;
          csr->core_speed_rating[i] =
            time_weighted_average(cur_rating,
                                  csr->core_speed_rating[i],
                                  csr->trips);
          csr->scaled_thread_range += csr->core_speed_rating[i] *
                                      popcorn_global.threads_per_node[i];
        }
      }
    }

    if(ull)
    {
      popcorn_global.ws.next_ull += popcorn_global.ws.chunk_size_ull *
                                    popcorn_global.ws.incr_ull *
                                    gomp_thread()->ts.team->nthreads;
      calculate_splits_ull(csr, &popcorn_global.ws);
    }
    else
    {
      popcorn_global.ws.next += popcorn_global.ws.chunk_size *
                                popcorn_global.ws.incr *
                                gomp_thread()->ts.team->nthreads;
      calculate_splits(csr, &popcorn_global.ws);
    }

    hierarchy_leader_cleanup(&popcorn_global.sync);
  }
  gomp_team_barrier_wait_nospin(&popcorn_global.bar);
}

bool hierarchy_next_hetprobe(int nid,
                             const void *ident,
                             long *start,
                             long *end)
{
  bool leader;
  struct gomp_thread *thr = gomp_thread();
  struct gomp_work_share *ws = thr->ts.work_share;
  struct timespec probe_end;
  workshare_csr_t *csr = NULL;

  switch(thr->ts.static_trip)
  {
  case 0: /* Probe period -- only probe once */
    thr->ts.static_trip = 1;
    *start = ws->next + (thr->ts.team_id * ws->chunk_size * ws->incr);
    *end = *start + (ws->chunk_size * ws->incr);
    return true;
  case 1: /* Finished probe, assign remaining iterations */
    thr->ts.static_trip = 2;

    /* Add this thread's elapsed time to the workshare */
    clock_gettime(CLOCK_MONOTONIC, &probe_end);
    __atomic_add_fetch(&popcorn_node[nid].workshare_time,
                       ELAPSED(thr->probe_start, probe_end) / 1000,
                       MEMMODEL_ACQ_REL);

    leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                       &popcorn_node[nid].bar,
                                       false, NULL);
    if(leader)
    {
#ifdef _CACHE_HETPROBE
      csr = get_entry(ident);
      assert(csr != HTAB_EMPTY_ENTRY && "Missing cache entry");
#else
      csr = &global_csr;
#endif
      calc_het_probe_workshare(nid, false, csr);
      init_workshare_from_splits(nid, csr, ws);
      hierarchy_leader_cleanup(&popcorn_node[nid].sync);
    }
    gomp_team_barrier_wait(&popcorn_node[nid].bar);

    /* Fall through */
  default: return hierarchy_next_dynamic(nid, start, end);
  }
}

bool hierarchy_next_hetprobe_ull(int nid,
                                 const void *ident,
                                 unsigned long long *start,
                                 unsigned long long *end)
{
  bool leader;
  struct gomp_thread *thr = gomp_thread();
  struct gomp_work_share *ws = thr->ts.work_share;
  struct timespec probe_end;
  workshare_csr_t *csr = NULL;

  switch(thr->ts.static_trip)
  {
  case 0: /* Probe period -- only probe once */
    thr->ts.static_trip = 1;
    *start =
      ws->next_ull + (thr->ts.team_id * ws->chunk_size_ull * ws->incr_ull);
    *end = *start + (ws->chunk_size_ull * ws->incr_ull);
    return true;
  case 1: /* Finished probe, assign remaining iterations */
    thr->ts.static_trip = 2;

    /* Add this thread's elapsed time to the workshare */
    clock_gettime(CLOCK_MONOTONIC, &probe_end);
    __atomic_add_fetch(&popcorn_node[nid].workshare_time,
                       ELAPSED(thr->probe_start, probe_end) / 1000,
                       MEMMODEL_ACQ_REL);

    leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                       &popcorn_node[nid].bar,
                                       false, NULL);
    if(leader)
    {
#ifdef _CACHE_HETPROBE
      csr = get_entry(ident);
      assert(csr != HTAB_EMPTY_ENTRY && "Missing cache entry");
#else
      csr = &global_csr;
#endif
      calc_het_probe_workshare(nid, true, csr);
      init_workshare_from_splits_ull(nid, csr, ws);
      hierarchy_leader_cleanup(&popcorn_node[nid].sync);
    }
    gomp_team_barrier_wait(&popcorn_node[nid].bar);

    /* Fall through */
  default: return hierarchy_next_dynamic_ull(nid, start, end);
  }
}

bool hierarchy_last(long end)
{
  return end >= popcorn_global.ws.end;
}

bool hierarchy_last_ull(unsigned long long end)
{
  return end >= popcorn_global.ws.end_ull;
}

void hierarchy_loop_end(int nid, const void *ident, bool global)
{
  struct gomp_thread *thr = gomp_thread();
  bool leader;
#ifdef _CACHE_HETPROBE
  struct timespec region_end;
  unsigned long long sent, recv;
  hash_entry_type ent;

  if(popcorn_log_statistics)
  {
    /* If it was originally the hetprobe scheduler we have an entry & region
       statistics will have been calculated.  If we don't have one, then we
       need to calculate the region statistics here. */
    ent = get_entry(ident);
    if(!ent)
    {
      clock_gettime(CLOCK_MONOTONIC, &region_end);
      __atomic_add_fetch(&popcorn_node[nid].workshare_time,
                         ELAPSED(thr->probe_start, region_end) / 1000,
                         MEMMODEL_ACQ_REL);
    }
  }
#endif

  leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                     &popcorn_node[nid].bar,
                                     false, NULL);
  if(leader)
  {
#ifdef _CACHE_HETPROBE
    if(popcorn_log_statistics && !ent)
    {
      popcorn_global.workshare_time[nid] =
        popcorn_node[nid].workshare_time /
        popcorn_global.threads_per_node[nid];
      popcorn_get_page_faults(&sent, &recv);
      popcorn_global.page_faults[nid] = sent - popcorn_node[nid].page_faults;
    }
#endif
    gomp_fini_work_share(&popcorn_node[nid].ws);
    gomp_ptrlock_destroy(&popcorn_node[nid].ws_lock);
    gomp_ptrlock_init(&popcorn_node[nid].ws_lock, NULL);
    if(global)
    {
      leader = select_leader_synchronous(&popcorn_global.sync,
                                         &popcorn_global.bar,
                                         false, NULL);
      if(leader)
      {
        gomp_fini_work_share(&popcorn_global.ws);
        gomp_ptrlock_destroy(&popcorn_global.ws_lock);
        gomp_ptrlock_init(&popcorn_global.ws_lock, NULL);
        hierarchy_leader_cleanup(&popcorn_global.sync);
      }
      gomp_team_barrier_wait_nospin(&popcorn_global.bar);
    }
    hierarchy_leader_cleanup(&popcorn_node[nid].sync);
  }
  gomp_team_barrier_wait(&popcorn_node[nid].bar);

#ifdef _CACHE_HETPROBE
  // TODO: when global == false, we can't guarantee that everybody has written
  // their statistics to popcorn_global
  if(thr->ts.team_id == 0 && popcorn_log_statistics)
  {
    if(!ent) log_hetprobe_results(ident, &dummy_csr);
    else log_hetprobe_results(ident, ent);
  }
#endif

  /* gomp_team_end() still expects the main thread to have a valid work share
     pointer */
  if(thr->ts.team_id == 0) thr->ts.work_share = &thr->ts.team->work_shares[0];
  else thr->ts.work_share = NULL;
}

