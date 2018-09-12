/*
 * Provides hierarchy abstractions for threads executing in Popcorn Linux.
 *
 * Copyright Rob Lyerly, SSRG, VT, 2018
 */

#include <assert.h>
#include "libgomp.h"
#include "hierarchy.h"

/* Time barriers to detect load imbalance among nodes */
//#define _TIME_BARRIER 1

#ifdef _TIME_BARRIER
# include <time.h>
# define NS( ts ) ((ts.tv_sec * 1000000000LL) + ts.tv_nsec)
# define ELAPSED( start, end ) (NS(end) - NS(start))
#endif

global_info_t ALIGN_PAGE popcorn_global;
node_info_t ALIGN_PAGE popcorn_node[MAX_POPCORN_NODES];

///////////////////////////////////////////////////////////////////////////////
// Leader selection
///////////////////////////////////////////////////////////////////////////////

void hierarchy_init_global(int nodes)
{
  popcorn_global.sync.remaining = popcorn_global.sync.num = nodes;
  popcorn_global.opt.remaining = popcorn_global.opt.num = nodes;
  /* Note: *must* use reinit_all, otherwise there's a race condition between
     leaders who have been released are reading generation in the barrier's
     do-while loop and the main thread resetting barrier's generation */
  gomp_barrier_reinit_all(&popcorn_global.bar, nodes);
}

void hierarchy_init_node(int nid)
{
  size_t num;

  assert(popcorn_node[nid].sync.num == popcorn_node[nid].opt.num &&
         "Corrupt node configuration");

  num = popcorn_node[nid].sync.num;
  popcorn_node[nid].sync.remaining = popcorn_node[nid].opt.remaining = num;
  /* See note in hierarchy_init_global() above */
  gomp_barrier_reinit_all(&popcorn_node[nid].bar, num);
}

int hierarchy_assign_node(unsigned tnum)
{
  unsigned cur = 0, thr_total = 0;
  for(cur = 0; cur < MAX_POPCORN_NODES; cur++)
  {
    thr_total += popcorn_global.threads_per_node[cur];
    if(tnum < thr_total)
    {
      popcorn_node[cur].sync.num++;
      popcorn_node[cur].opt.num++;
      return cur;
    }
  }

  /* If we've exhausted the specification default to origin */
  popcorn_node[0].sync.num++;
  popcorn_node[0].opt.num++;
  return 0;
}

/****************************** Internal APIs *******************************/

static bool hierarchy_select_leader_optimistic(leader_select_t *l,
                                               size_t *ticket)
{
  size_t rem = __atomic_fetch_add(&l->remaining, -1, MEMMODEL_ACQ_REL);
  if(ticket) *ticket = rem - 1;
  return rem == l->num;
}

static bool hierarchy_select_leader_synchronous(leader_select_t *l,
                                                size_t *ticket)
{
  size_t rem = __atomic_add_fetch(&l->remaining, -1, MEMMODEL_ACQ_REL);
  if(ticket) *ticket = rem;
  return !rem;
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
#ifdef _TIME_BARRIER
  struct timespec start, end;
#endif

  leader = hierarchy_select_leader_synchronous(&popcorn_node[nid].sync, NULL);
  if(leader)
  {
#ifdef _TIME_BARRIER
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    gomp_team_barrier_wait_nospin(&popcorn_global.bar);
#ifdef _TIME_BARRIER
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("%s barrier wait: %lld ns\n",
           (desc ? desc : "Anonymous"),
           ELAPSED(start, end));
#endif
    hierarchy_leader_cleanup(&popcorn_node[nid].sync);
  }
  gomp_team_barrier_wait(&popcorn_node[nid].bar);
}

bool hierarchy_hybrid_cancel_barrier(int nid, const char *desc)
{
  bool ret = false, leader;
#ifdef _TIME_BARRIER
  struct timespec start, end;
#endif

  leader = hierarchy_select_leader_synchronous(&popcorn_node[nid].sync, NULL);
  if(leader)
  {
#ifdef _TIME_BARRIER
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    ret = gomp_team_barrier_wait_cancel_nospin(&popcorn_global.bar);
    // TODO if cancelled at global level need to cancel local barrier
#ifdef _TIME_BARRIER
    if(!ret)
    {
      clock_gettime(CLOCK_MONOTONIC, &end);
      printf("%s cancel barrier wait: %lld ns\n",
             (desc ? desc : "Anonymous"),
             ELAPSED(start, end));
    }
#endif
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
  unsigned awaited_final;
#ifdef _TIME_BARRIER
  struct timespec start, end;
#endif

  leader = hierarchy_select_leader_synchronous(&popcorn_node[nid].sync, NULL);
  if(leader)
  {
#ifdef _TIME_BARRIER
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    // Wait for non-leader threads to enter per-node barrier
    do awaited_final = __atomic_load_n(&popcorn_node[nid].bar.awaited_final,
                                       MEMMODEL_ACQUIRE);
    while(awaited_final != 1);
    gomp_team_barrier_wait_final_nospin(&popcorn_global.bar);
#ifdef _TIME_BARRIER
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("%s final team barrier wait: %lld ns\n",
           (desc ? desc : "Anonymous"),
           ELAPSED(start, end));
#endif
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
  global_leader = hierarchy_select_leader_optimistic(&popcorn_global.opt,
                                                     NULL);
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

  leader = hierarchy_select_leader_optimistic(&popcorn_node[nid].opt,
                                              &ticket);
  if(leader)
  {
    leader = hierarchy_reduce_leader(nid, reduce_data, reduce_func);
    hierarchy_leader_cleanup(&popcorn_node[nid].opt);
  }
  else hierarchy_reduce_local(nid, ticket, reduce_data);
  return leader;
}

