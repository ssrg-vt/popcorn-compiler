/*
 * Provides hierarchy abstractions for threads executing in Popcorn Linux.
 *
 * Copyright Rob Lyerly, SSRG, VT, 2018
 *
 * Additions: 
 * Irregular case, Carlos Bilbao, 2021.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <float.h>
#include "hierarchy.h"

global_info_t ALIGN_PAGE popcorn_global;
node_info_t ALIGN_PAGE popcorn_node[MAX_POPCORN_NODES];

#define DEBUG_IRREGULAR
#ifdef DEBUG_IRREGULAR
#define IRR_DEBUG( ...) fprintf(stderr, __VA_ARGS__)
#endif

#define DEBUG_CSR
#ifdef DEBUG_CSR
#define CSR_DEBUG( ...) fprintf(stderr, __VA_ARGS__)
#endif

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
// Initialization
///////////////////////////////////////////////////////////////////////////////

int hierarchy_node_first_thread(int nid)
{
  int i;
  unsigned cur = 0;
  assert(nid >= 0 && nid < MAX_POPCORN_NODES && "Invalid node ID");
  for(i = 0; i < nid; i++) cur += popcorn_global.node_places[i];
  return cur;
}

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

void hierarchy_init_node_team_state(int nid,
                                    struct gomp_team *team,
                                    struct gomp_work_share *ws,
                                    struct gomp_work_share *last_ws,
                                    unsigned start_team_id,
                                    unsigned level,
                                    unsigned active_level,
                                    unsigned place_partition_off,
                                    unsigned place_partition_len,
#ifdef HAVE_SYNC_BUILTINS
                                    unsigned long single_count,
#endif
                                    unsigned long static_trip,
                                    struct gomp_task *task,
                                    struct gomp_task_icv *icv,
                                    void (*fn)(void *),
                                    void *data)
{
  // TODO how can threads be shuffled between nodes in this situation?
  popcorn_node[nid].ns.ts.team = team;
  popcorn_node[nid].ns.ts.work_share = ws;
  popcorn_node[nid].ns.ts.last_work_share = last_ws;
  popcorn_node[nid].ns.ts.team_id = start_team_id;
  popcorn_node[nid].ns.ts.level = level;
  popcorn_node[nid].ns.ts.active_level = active_level;
  popcorn_node[nid].ns.ts.place_partition_off = place_partition_off;
  popcorn_node[nid].ns.ts.place_partition_len = place_partition_len;
#ifdef HAVE_SYNC_BUILTINS
  popcorn_node[nid].ns.ts.single_count = single_count;
#endif
  popcorn_node[nid].ns.ts.static_trip = static_trip;
  popcorn_node[nid].ns.task = task;
  popcorn_node[nid].ns.icv = icv;
  popcorn_node[nid].ns.fn = fn;
  popcorn_node[nid].ns.data = data;
}

void hierarchy_clear_node_team_state(int nid)
{
  // Threads check the function passed by gomp_team_start() to
  // gomp_thread_start() to determine whether to participate.
  popcorn_node[nid].ns.fn = NULL;
}

/* Note: the main thread should already have initialized this node's
   synchronization data structures! */
void hierarchy_init_thread(int nid)
{
  size_t i, start = popcorn_node[nid].ns.ts.team_id;
  bool leader;
  struct gomp_thread *me = gomp_thread(), *nthr;
  struct gomp_team *team = popcorn_node[nid].ns.ts.team;
  struct gomp_task *task = popcorn_node[nid].ns.task;
  struct gomp_task_icv *icv = popcorn_node[nid].ns.icv;
  void (*fn)(void *) = popcorn_node[nid].ns.fn;
  void *data = popcorn_node[nid].ns.data;

  /* If the main thread didn't set this node's function then we aren't
     participating in the parallel region. */
  if(!fn) return;

  // TODO if we fell back to single node execution, reassign node IDs

  leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                     &popcorn_node[nid].bar,
                                     false, NULL);
  if(leader)
  {
    for(i = 0; i < popcorn_global.threads_per_node[nid]; i++)
    {
      nthr = me->thread_pool->threads[i+start];
      memcpy(&nthr->ts, &popcorn_node[nid].ns.ts, sizeof(nthr->ts));
      nthr->ts.team_id = i + start;
      nthr->task = &team->implicit_task[i+start];
      nthr->place = 0;
      gomp_init_task(nthr->task, task, icv);
      nthr->fn = fn;
      nthr->data = data;
    }
    popcorn_node[nid].ns.fn = NULL;
    hierarchy_leader_cleanup(&popcorn_node[nid].sync);
  }
  gomp_team_barrier_wait(&popcorn_node[nid].bar);
}

///////////////////////////////////////////////////////////////////////////////
// Barriers
///////////////////////////////////////////////////////////////////////////////

void hierarchy_hybrid_barrier(int nid)
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

bool hierarchy_hybrid_cancel_barrier(int nid)
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
void hierarchy_hybrid_barrier_final(int nid)
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

#define _HETPROBE_IRREGULAR

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

#if !defined(_CACHE_HETPROBE) || defined(_HETPROBE_IRREGULAR)
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
  struct gomp_thread *thr = gomp_thread();
  unsigned long long i, read;
  char *buf, *cur, *end;
  FILE *fp;
  static const size_t bufsz = 7768;

  assert(sent && recv && "Invalid arguments to get_page_faults()");

  if((fp = fopen("/proc/popcorn_stat", "r")))
  {
    cur = buf = (char *)popcorn_malloc(sizeof(char) * bufsz, thr->popcorn_nid);
    read = fread(buf, bufsz, 1, fp);
    while(*cur != '-') cur++;
    for(i = 0; i < 10; cur++)
      if(*cur == '\n') i++;
    while(*cur == ' ') cur++;
    *sent = strtoul(cur, &end, 10);
    cur = end;
    while(*cur == ' ') cur++;
    *recv = strtoul(cur, NULL, 10);
    popcorn_free(buf);
  }
  else
  {
    *sent = 0;
    *recv = 0;
  }
}

static void init_statistics(int nid)
{
  unsigned long long sent = 0, recv = 0;
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
   This function also updates the work share to be depleted. 
*/
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
      split_range += csr->core_speed_rating[i-1] *  popcorn_global.threads_per_node[i-1];
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
    split_range += csr->core_speed_rating[i-1] * popcorn_global.threads_per_node[i-1];
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

static void 
init_workshare_from_splits(int nid, workshare_csr_t *csr, struct gomp_work_share *ws)
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

static inline void
get_next_work_fraction(int nid,struct gomp_work_share *ws, struct gomp_thread *thr)
{
	int pctg, iters, id = gomp_thread()->ts.team_id;

	pctg = gomp_global_icv.irr_percentage;
	iters = thr->ts.real_ws_i;

	if (iters == 1){
		ws->next = ws->real_next;
	} 
	else {
		ws->next = ws->end;
	}

	ws->end = ws->next + ((pctg * (ws->real_chunk_size))/100) * iters; 
	ws->chunk_size = (ws->end - ws->next) + 1;
	thr->ts.real_ws_i++;

  IRR_DEBUG("T.%d [Node %d]> Got next=%d/%d end=%d/%d chunk=%d (%s)\n",
        id,nid,ws->next,ws->real_next,ws->end,ws->real_end,ws->chunk_size,__func__);
}

static inline void
get_next_work_fraction_ull(int nid,struct gomp_work_share *ws, struct gomp_thread *thr)
{
	// TODO Use the ull version of these variables.
}

/* For the irregular hetprobe, we may need to wake the other threads for re-probing. */
static bool init_workshare_from_splits_irreg(int nid, 
   workshare_csr_t *csr, struct gomp_work_share *ws, struct gomp_thread *thr)
{
   bool ret = false;
   int id = thr->ts.team_id;

  if(csr->core_speed_rating[nid] == NO_ITER)
  {
    /*  The threads this leader work for might have to start working again for the 
	      re-probing. We can afford to spin because the node will be empty anyway.
    */
    IRR_DEBUG("T.%d [Node %d]> Will sleep. (%s) \n",id,nid, __func__);
    gomp_team_barrier_wait(&popcorn_global.bar_irregular);
    ret = true;
    IRR_DEBUG("T.%d [Node %d]> Was awaken. (%s) \n",id,nid, __func__);
  }
  else
  {
   /*    To trigger re-probing, we need the threads to go back for more work regularly, 
	 even if the CSR obtained from the probing is big. If no reprobing, case 3 will 
	 give them more of what they should run (between the splits). 
   */
    ws->real_next = popcorn_global.split[nid];
    ws->real_end = popcorn_global.split[nid+1];
    ws->real_chunk_size = calc_chunk_from_ratio(nid, ws->incr, csr);
   
    /* Give a fraction relative to the re-probing percentage  */ 
    get_next_work_fraction(nid,ws,thr); 
  }

  return ret;
}

/* For the irregular hetprobe, we may need to wake the other threads for re-probing. */
static bool init_workshare_from_splits_irreg_ull(int nid, 
    workshare_csr_t *csr, struct gomp_work_share *ws, struct gomp_thread *thr)
{
	// TODO
	// Use ws->real_end_ull and so on.
	return true;
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

  cur += snprintf(cur, sizeof(buf), "%s\nCSR:", ident ? ident : "(no identifier)");

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

void hierarchy_log_statistics(int nid, const char *ident)
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

void hierarchy_init_workshare_hetprobe_irregular(int nid,
                                       const void *ident,
                                       long long lb,
                                       long long ub,
                                       long long incr,
                                       long long chunk)
{
  /* TODO So far I do not see anything that should be changed right away, 
     other than to keep an eye on the probe caching, removed for now... -Carlos.*/

  struct gomp_thread *thr = gomp_thread();
  struct gomp_team *team = thr->ts.team;
  int nthreads = team ? team->nthreads : 1;
  struct gomp_work_share *ws, *global;

  IRR_DEBUG("T.%d [Node %d]> Initializes (%s) \n",thr->ts.team_id,nid, __func__);

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
    loop_init(ws, lb, lb, incr, GFS_HETPROBE_IRREGULAR, chunk, nid);
    global = gomp_ptrlock_get(&popcorn_global.ws_lock);
    popcorn_global.init_chunk = chunk;

    if(global == NULL)
    {
      global = &popcorn_global.ws;
      gomp_init_work_share(global, false, nthreads);
      loop_init(global, lb, ub, incr, GFS_HETPROBE_IRREGULAR, chunk, nid);
      gomp_ptrlock_set(&popcorn_global.ws_lock, global);
    }
    init_statistics(nid);
   gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
  }

  thr->ts.work_share = ws;
  thr->ts.static_trip = 0;
  clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
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
    /* Somebody hit the distributed execuon killswitch, only give work to the
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

void hierarchy_init_workshare_hetprobe_irregular_ull(int nid,
                                           const void *ident,
                                           unsigned long long lb,
                                           unsigned long long ub,
                                           unsigned long long incr,
                                           unsigned long long chunk)
{
   /* TODO So far I do not see anything that should be changed right away, 
     other than to keep an eye on the probe caching, removed for now... -Carlos.*/

 struct gomp_thread *thr = gomp_thread();
  struct gomp_team *team = thr->ts.team;
  int nthreads = team ? team->nthreads : 1;
  struct gomp_work_share *ws, *global;

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
      gomp_ptrlock_set(&popcorn_global.ws_lock, global);
    }
    init_statistics(nid);
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
    ret = gomp_iter_dynamic_next_raw(&ws->next, &ws->end,&popcorn_global.ws, chunk);
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
      avg_uspf += uspf * ((float)popcorn_global.threads_per_node[i] / nthreads);
   }
  }

  return avg_uspf;
}

inline float time_weighted_average(float cur, float prev, bool first)
{
  if(first) return cur;
  else return (0.75 * cur) + (0.25 * prev);
}

#define MAX( a, b ) ((a) > (b) ? (a) : (b))

static void calc_het_probe_workshare(int nid, bool ull, workshare_csr_t *csr, int het_irregular)
{
  bool leader, calc_csr = true;
  size_t i, max_idx;
  unsigned long long cur_elapsed, min = UINT64_MAX, max = 0, sent, recv;
  long ws_threads;
  float scale, cur_rating;
  int id = gomp_thread()->ts.team_id;

  /* Calculate this node's average time & page faults */
  ws_threads = popcorn_node[nid].workshare_time / popcorn_global.threads_per_node[nid];
  popcorn_global.workshare_time[nid] = MAX(ws_threads, 1);
  popcorn_get_page_faults(&sent, &recv);
  popcorn_global.page_faults[nid] = sent - popcorn_node[nid].page_faults;

  if (!het_irregular){
    leader = select_leader_synchronous(&popcorn_global.sync,&popcorn_global.bar,false, NULL);
  }
  else {
    leader = get_global_leader(nid,gomp_thread());
  }

  struct gomp_thread *thr = gomp_thread();

  if(leader)
  {
    assert("csr in calc_het_probe_workshare is NULL" && csr);
    assert("csr->trips don't make sense" && !(csr->trips && !csr->uspf));
    csr->uspf = time_weighted_average(calc_avg_us_per_pf(), csr->uspf, csr->trips);

    IRR_DEBUG("T.%d [Node %d]> Page_fs %llu, workshare time %llu (%s)\n",
     id,nid,popcorn_global.page_faults[nid],popcorn_global.workshare_time[nid], __func__);
    /* If we've reached max probes, make a determination -- are we going to run
       across nodes or not? */
    if(csr->trips >= popcorn_max_probes && popcorn_prime_region &&
       strcmp(csr->ident, popcorn_prime_region) == 0)
    {
      if(csr->uspf <= 100.0)
      {
   	    IRR_DEBUG("T.%d [Node %d]> Reached max probes (uspf %f) (%s)\n",
		    id,nid,csr->uspf, __func__);
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
            popcorn_global.scaled_thread_range = popcorn_global.threads_per_node[i];
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
      scale = 1.0 / ((float)min / (float)max);
   
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
                                  csr->trips == 0);
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

  /* Printing the CSR might give us some insights on the degree of irregularity */
  
  CSR_DEBUG("T.%d [Node %d]> CSR is now %d:%d (%s)\n",
     id,nid,csr->core_speed_rating[0],csr->core_speed_rating[1], __func__);
}

typedef enum {
	PROBING_IRR=0,
	PART_IRR_PROBING,
	PART_IRR_DYNAMIC,
	PROBED_IRR,
	NEXT_IRR
} cases_irregular_next_t;

void fix_jumps(int nid,long *start, long *end, struct gomp_thread *thr, bool probing)
{
  int i = 0, init, end_j, aux, max;
  max = popcorn_global.num_irr_jumps;

  thr->ts.static_trip = (probing)? PROBED_IRR:NEXT_IRR;

  /* Is there any jump between start and end? */
  for (;i < max; ++i)
  {
    init   = popcorn_global.het_irregular_jumps[i].init;
    end_j  = popcorn_global.het_irregular_jumps[i].end;

    if (init >= *start && end_j <= *end)
    {
      aux = *end;
      /* End where the jump begins. */
      *end = init;
      thr->ts.static_trip = (probing)? PART_IRR_PROBING:PART_IRR_DYNAMIC;
      thr->reprobe_init = end_j;
      thr->reprobe_end = aux;
      IRR_DEBUG("T.%d [Node %d]> Jump solved, assigned  [%lu-%lu]\n",
        thr->ts.team_id,nid,*start,*end);
      break;
    }
  }

  popcorn_global.total_irr_done += *end - *start;
}

static inline bool assign_probing_work(int nid,
                                       struct gomp_thread *thr,
                                       struct gomp_work_share *ws,
                                       long *start,
                                       long *end,
                                       bool reprobed)
{
	if (!reprobed){
		*start = ws->next + (thr->ts.team_id * ws->chunk_size * ws->incr);
		*end = *start + (ws->chunk_size * ws->incr);
	}
	else {
		*start = thr->reprobe_init;
		*end   = thr->reprobe_end;
	}

	fix_jumps(nid,start,end,thr, true);

	return true;
}

void regenerate_local_work(int nid,
          struct gomp_thread *thr,
          struct gomp_work_share *old_ws)
{
	struct gomp_work_share *ws;
	long long lb = popcorn_global.other_next, incr=1,chunk=popcorn_global.init_chunk;

	ws = gomp_ptrlock_get(&popcorn_node[nid].ws_lock);
	ws = &popcorn_node[nid].ws;
	gomp_init_work_share(ws, false, popcorn_global.threads_per_node[nid]);
	loop_init(ws, lb, lb, incr, GFS_HETPROBE_IRREGULAR, chunk, nid);
	gomp_ptrlock_set(&popcorn_node[nid].ws_lock, ws);
	old_ws = ws;
}

int get_local_leader(int nid, struct gomp_thread *thr)
{
    int leader, id = gomp_thread()->ts.team_id;
    static volatile int leader_id = -1;

    if (leader_id == -1)
    {
      leader = select_leader_synchronous(&popcorn_node[nid].sync,
                                         &popcorn_node[nid].bar,false, NULL);  
      if (leader){
        leader_id = id;
      }
    }
    else return (leader_id == id);

    return leader;
}

int get_global_leader(int nid, struct gomp_thread *thr)
{
    int leader, id = gomp_thread()->ts.team_id;
    static volatile int leader_id = -1;

    if (leader_id == -1)
    {
      leader = select_leader_synchronous(&popcorn_global.sync,&popcorn_global.bar,
                                        false, NULL);
 
      if (leader){
        leader_id = id;
      }
    }
    else return (leader_id == id);

    return leader;
}

/* We will need to assign work, but if we for example had before a  distribution of 
   N0=[10,20) and N1=[20,25] and both did two iterations, we will need to work from 
   now on with a workshare [12-19,22-25] i.e. [real_next-real_end] for both nodes. 
*/
void regenerate_global_work(int nid,
			    struct gomp_thread *thr,
			    struct gomp_work_share *old_ws)
{
	struct gomp_work_share *global; 
	long next_1, end_1,next_2,end_2,aux;
	long long incr = 1, chunk = popcorn_global.init_chunk;
	struct gomp_team *team = thr->ts.team;
	int nthreads = team ? team->nthreads : 1, jump;
	int id = thr->ts.team_id;

	next_1 = old_ws->next;
	end_1  = old_ws->real_end;
	next_2 = popcorn_global.other_next;
	end_2  = popcorn_global.other_end;

	if (next_1 > next_2){
		aux    = next_2;
		next_2 = next_1;
		next_1 = aux;
		aux    = end_2;
		end_2  = end_1;
		end_1  = aux;
	}

	IRR_DEBUG("T.%d [Node %d]> Joining [%lu-%lu],[%lu-%lu](%s)\n",
	 id,nid,next_1,end_1,next_2,end_2,__func__);

	global = gomp_ptrlock_get(&popcorn_global.ws_lock);
	global = &popcorn_global.ws;
	gomp_init_work_share(global, false, nthreads);
	loop_init(global, next_1, end_2, incr, GFS_HETPROBE_IRREGULAR, chunk, nid);
	gomp_ptrlock_set(&popcorn_global.ws_lock, global);

	/* Is there fragmentation? */
	if (end_1 < next_2){

		jump = popcorn_global.num_irr_jumps;
		assert("MAX_IRR_JUMPS is too small here!" && jump < MAX_IRR_JUMPS);
		/* This could be optmized in 1. Memory (using dynamic array) and 2. In
		   complexity using workshares made of ints and jump labels  (But that
		   would be a considerably bigger refactoring work).*/
		popcorn_global.het_irregular_jumps[jump].init = end_1;
		popcorn_global.het_irregular_jumps[jump].end  = next_2;
		popcorn_global.num_irr_jumps++;
    		IRR_DEBUG("T.%d [Node %d]> Jump [%lu-%lu] registered\n",
			  id,nid,end_1,next_2);
	}

	if (popcorn_global.other_next > next_1){
		popcorn_global.other_next = next_1;
	}
}

/* The re-probing is a way a rollback of work splitting decisions and statistics,
   and it depends on the state of threads on the other node and if a heterogeneous
   set up was selected by Hetprobe.
   Returns 1 if the thread was the last leader to arrive.
*/
bool sync_reprobing(int nid, struct gomp_thread *thr,struct gomp_work_share *old_ws)
{
  int id = gomp_thread()->ts.team_id;
  bool leader;

  leader = select_leader_synchronous(&popcorn_global.sync,&popcorn_global.bar,false, NULL);

  if (!leader)
    IRR_DEBUG("T.%d [Node %d]> Re-probing sync waiting for leader (%s)\n",id,nid,__func__);

  if (leader){   
    regenerate_global_work(nid,thr,old_ws);
    hierarchy_leader_cleanup(&popcorn_global.sync);
  }
  else {
  	popcorn_global.other_next = old_ws->next;
  	popcorn_global.other_end  = old_ws->real_end;
  }
  gomp_team_barrier_wait_nospin(&popcorn_global.bar);

  /* Regenerate local work */
  regenerate_local_work(nid,thr,old_ws);

  /* Restart splitting between real and next work */
  thr->ts.real_ws_i = 1;  

  return leader;
}

void msg_static_trip(int id,int nid,int trip)
{
	switch(trip){
	case PROBING_IRR:
  		IRR_DEBUG("T.%d [Node %d]> Starts probing period\n",id,nid);
	break;
	case PART_IRR_PROBING:
  		IRR_DEBUG("T.%d [Node %d]> Asks for more probing work\n",id,nid);
	break;
	case PART_IRR_DYNAMIC:
  		IRR_DEBUG("T.%d [Node %d]> Asks for more dynamic work\n",id,nid);
	break;
	case PROBED_IRR:
  		IRR_DEBUG("T.%d [Node %d]> Finished probing\n",id,nid);
	break;
	default:
  		IRR_DEBUG("T.%d [Node %d]> Asks for more work\n",id,nid);
	break;
	}
}

inline void init_statistics_het(int nid)
{
	init_statistics(nid);
  popcorn_node[nid].page_faults = 0;
}

/*
  //TODO Remove Find fast via vim: zzz
*/
bool hierarchy_next_hetprobe_irregular(int nid,
                             const void *ident,
                             long *start,
                             long *end)
{
  bool leader, ret, func_ret, waited, probe_again = false;
  struct gomp_thread *thr = gomp_thread(), *nthr;
  struct gomp_work_share *ws = thr->ts.work_share;
  struct timespec probe_end;
  workshare_csr_t *csr = NULL;
  int used_percentage, id = thr->ts.team_id, i;
  size_t start_id = popcorn_node[nid].ns.ts.team_id;

  msg_static_trip(id,nid,thr->ts.static_trip);

  switch(thr->ts.static_trip)
  {
  case PROBING_IRR: /* Probe period */
    
    func_ret = assign_probing_work(nid,thr,ws,start,end,false);
    break;

  case PART_IRR_PROBING:

    /* If had to jump on a re-probing work assignment, give the next bunch */
    func_ret = assign_probing_work(nid,thr,ws,start,end,true);
    break;

  case PART_IRR_DYNAMIC:

    /* If had to jump on a dynamic work assignment, give the next bunch */
    func_ret = hierarchy_next_dynamic(nid, start, end);
    fix_jumps(nid,start,end,thr,false);
    break;

  case PROBED_IRR: /* Finished probe, assign iterations */

    thr->ts.static_trip = NEXT_IRR;

    /* Add this thread's elapsed time to the workshare */
    clock_gettime(CLOCK_MONOTONIC, &probe_end);

    __atomic_add_fetch(&popcorn_node[nid].workshare_time,
                       ELAPSED(thr->probe_start, probe_end) / 1000,
                       MEMMODEL_ACQ_REL);

    leader = get_local_leader(nid,thr);

    if(leader)
    {
      IRR_DEBUG("T.%d [Node %d]> It's leading. (%s)\n",id,nid, __func__);
      csr = &global_csr;
      calc_het_probe_workshare(nid, false, csr,1);
      waited = init_workshare_from_splits_irreg(nid, csr, ws,thr); 

      if (waited)
      {
        init_statistics_het(nid);
        /* Regenerate local work */
        regenerate_local_work(nid,thr,ws);
        /* We need to restart the clock of each thread in the node and assign them
           probing work.
        */
        for(i = 0; i < popcorn_global.threads_per_node[nid]; i++)
        {
          nthr = thr->thread_pool->threads[i+start_id];
          nthr->ts.probe_again = true;
        }
      }
      hierarchy_leader_cleanup(&popcorn_node[nid].sync);
    }
    else
    {
      IRR_DEBUG("T.%d [Node %d]> It's not leading. (%s)\n",id,nid, __func__);
    }
    gomp_team_barrier_wait(&popcorn_node[nid].bar);

    /* Check if the leader was stopped because there was no work in this node. */
    if (thr->ts.probe_again){
        IRR_DEBUG("T.%d [Node %d]> Gets probing work. (%s)\n",id,nid, __func__);
        func_ret = assign_probing_work(nid,thr,ws,start,end,false);
        clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
        thr->ts.probe_again = false;
    }
    else {
      func_ret = hierarchy_next_dynamic(nid, start, end);
      fix_jumps(nid,start,end,thr,false);
    }

    break;

  /* In the hetprobe irregular we keep calling this function, because stopped threads
     should be restarted when we reach a re-probing point.
   */
  default:

try_again:

    /* Compute the percentage if we use that periodic profiling mode */
    if (gomp_global_icv.use_pctg_hetprobe) {

      used_percentage = (popcorn_global.total_irr_done * 100)/popcorn_global.total_irr;
 
      IRR_DEBUG("T.%d [Node %d]> Iters=%d/%d (%d%%) last probe was on %d%%\n",
    		   id,nid,popcorn_global.total_irr_done,popcorn_global.total_irr,
    		   used_percentage,popcorn_global.last_probe);

      /* Have we reached or even passed a re-probing period? */
      if ((used_percentage-popcorn_global.last_probe)>=gomp_global_icv.irr_percentage && 
          used_percentage < 100)
     	{
    		  probe_again = true;
    	}
    } 

    leader = get_local_leader(nid,thr);

    /* Should we go back to a probing period? */ 
    /* TODO Future work: Set this value when user-defined or heuristic triggers. 
    */
    if (probe_again) {

      if (leader)
      {
        IRR_DEBUG("T.%d [Node %d]> It's leading in probe_again. (%s)\n",id,nid, __func__);

       /*  Three cases: 
        0. The threads are stopped at the other side (This node was the absolute favorite)
        and so we must restart their leader first, and provide them with probing work on 
        the second static trip (and send them to the first static trip). 

        The alternatives are two variations, in both the other node has work too, and hence
        it will come here for more until empty.
        1. If empty (but not done), we must stop it before leaving and repeat case 0. 
	      Future work.
        2. If not empty by now, the leader will have to wait for it to refill.  

        In any case we must restart the statistics of the other threads, and unless the
        other node had no work, this leader will have to wait for the other.
        */

        ret = gomp_team_barrier_wait_cancel(&popcorn_global.bar_irregular);

        if (!ret){ 
        	/* CASE 1/2. */
       		if (sync_reprobing(nid,thr,ws)){
               		popcorn_global.last_probe = used_percentage;
       		}
        }
        else { /* CASE 0. The leader at the other node was sleeping. */ 
  		      regenerate_global_work(nid,thr,ws);
          	popcorn_global.last_probe = used_percentage;
        }
        /* Restart metrics */ 
        init_statistics_het(nid);
        hierarchy_leader_cleanup(&popcorn_node[nid].sync);
      }
      else {
        IRR_DEBUG("T.%d [Node %d]> It's not leading in probe_again. (%s)\n",id,nid, __func__);
      }
      gomp_team_barrier_wait(&popcorn_node[nid].bar);
      func_ret = assign_probing_work(nid,thr,ws,start,end,false);
      clock_gettime(CLOCK_MONOTONIC, &thr->probe_start);
  }
  else {

      /* They should only receive a portion of what they should be given so they come
         for more (Enabling re-probing).
      */
    	if (leader) {
          get_next_work_fraction(nid,ws,thr);
          hierarchy_leader_cleanup(&popcorn_node[nid].sync);
    	}
    	gomp_team_barrier_wait(&popcorn_node[nid].bar);
	    func_ret = hierarchy_next_dynamic(nid, start, end);

	   /* Is this thread done too soon? */
	   // Edge cases - TODO Future work: One done too soon.

    	fix_jumps(nid,start,end,thr,false);
    } 
  }

    return func_ret;
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
      calc_het_probe_workshare(nid, false, csr,0);
      init_workshare_from_splits(nid, csr, ws);
      hierarchy_leader_cleanup(&popcorn_node[nid].sync);
    }
    gomp_team_barrier_wait(&popcorn_node[nid].bar);

    /* Fall through */
  default: return hierarchy_next_dynamic(nid, start, end);
  }
}

bool hierarchy_next_hetprobe_irregular_ull(int nid,
                                 const void *ident,
                                 unsigned long long *start,
                                 unsigned long long *end)
{
	// TODO Copy
	return 0;
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
      calc_het_probe_workshare(nid, true, csr,0);
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
