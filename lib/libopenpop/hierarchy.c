/*
 * Provides hierarchy abstractions for threads executing in Popcorn Linux.
 *
 * Copyright Rob Lyerly, SSRG, VT, 2018
 */

#include "libgomp.h"
#include "hierarchy.h"

global_info_t ALIGN_PAGE popcorn_global;
node_info_t ALIGN_PAGE popcorn_node[MAX_POPCORN_NODES];

///////////////////////////////////////////////////////////////////////////////
// Leader selection
///////////////////////////////////////////////////////////////////////////////

void hierarchy_init_global(int nodes)
{
  popcorn_global.ninfo.num = nodes;
  popcorn_global.ninfo.remaining = nodes;
  /* Note: *must* use reinit_all, otherwise there's a race condition between
     leaders who have been released are reading generation in the barrier's
     do-while loop and the main thread resetting barrier's generation */
  gomp_barrier_reinit_all(&popcorn_global.bar, nodes);
}

void hierarchy_init_node(int nid)
{
  popcorn_node[nid].tinfo.remaining = popcorn_node[nid].tinfo.num;
  /* See note in hierarchy_init_global() above */
  gomp_barrier_reinit_all(&popcorn_node[nid].bar, popcorn_node[nid].tinfo.num);
}

int hierarchy_assign_node(unsigned tnum)
{
  unsigned cur = 0, thr_total = 0;
  for(cur = 0; cur < MAX_POPCORN_NODES; cur++)
  {
    thr_total += popcorn_global.threads_per_node[cur];
    if(tnum < thr_total)
    {
      popcorn_node[cur].tinfo.num++;
      return cur;
    }
  }

  /* If we've exhausted the specification default to origin */
  popcorn_node[0].tinfo.num++;
  return 0;
}

/****************************** Internal APIs *******************************/

static bool hierarchy_select_leader_optimistic(leader_select_t *l,
                                               size_t *ticket)
{
  size_t rem = __atomic_add_fetch(&l->remaining, -1, MEMMODEL_ACQ_REL);
  if(ticket) *ticket = rem;
  if((rem + 1) == l->num) return true;
  else return false;
}

static bool hierarchy_select_leader_synchronous(leader_select_t *l,
                                                size_t *ticket)
{
  size_t rem = __atomic_add_fetch(&l->remaining, -1, MEMMODEL_ACQ_REL);
  if(ticket) *ticket = rem;
  if(rem) return false;
  else return true;
}

static void hierarchy_leader_cleanup(leader_select_t *l)
{
  __atomic_store_n(&l->remaining, l->num, MEMMODEL_RELEASE);
}

///////////////////////////////////////////////////////////////////////////////
// Barriers
///////////////////////////////////////////////////////////////////////////////

void hierarchy_hybrid_barrier(int nid)
{
  bool leader = hierarchy_select_leader_synchronous(&popcorn_node[nid].tinfo,
                                                    NULL);
  if(leader)
  {
    gomp_team_barrier_wait_nospin(&popcorn_global.bar);
    hierarchy_leader_cleanup(&popcorn_node[nid].tinfo);
  }
  gomp_team_barrier_wait(&popcorn_node[nid].bar);
}

bool hierarchy_hybrid_cancel_barrier(int nid)
{
  bool ret = false;
  bool leader = hierarchy_select_leader_synchronous(&popcorn_node[nid].tinfo,
                                                    NULL);
  if(leader)
  {
    ret = gomp_team_barrier_wait_cancel_nospin(&popcorn_global.bar);
    hierarchy_leader_cleanup(&popcorn_node[nid].tinfo);
  }
  return ret || gomp_team_barrier_wait_cancel(&popcorn_node[nid].bar);
}

void hierarchy_hybrid_barrier_final(int nid)
{
  bool leader = hierarchy_select_leader_synchronous(&popcorn_node[nid].tinfo,
                                                    NULL);
  if(leader)
  {
    gomp_team_barrier_wait_final_nospin(&popcorn_global.bar);
    hierarchy_leader_cleanup(&popcorn_node[nid].tinfo);
  }
  gomp_team_barrier_wait_final(&popcorn_node[nid].bar);
}

///////////////////////////////////////////////////////////////////////////////
// Reductions
///////////////////////////////////////////////////////////////////////////////

static inline void
hierarchy_reduce_leader(int nid,
                        void *reduce_data,
                        void (*reduce_func)(void *lhs, void *rhs))
{
  bool global_leader;
  size_t i, reduced = 1, nthreads = popcorn_node[nid].tinfo.num, max_entry;
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

  /* Now, select a global leader & do the same thing on the global queue. */
  global_leader = hierarchy_select_leader_optimistic(&popcorn_global.ninfo,
                                                     NULL);
  if(global_leader)
  {
    reduced = 1;
    while(reduced < popcorn_global.ninfo.num)
    {
      for(i = 0; i < MAX_POPCORN_NODES; i++)
      {
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
    hierarchy_leader_cleanup(&popcorn_global.ninfo);
  }
  else /* Each node should get its own reduction entry, no need to loop */
    __atomic_store_n(&popcorn_global.reductions[nid].p, reduce_data,
                     MEMMODEL_RELEASE);
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
                         MEMMODEL_RELAXED) != NULL) break;
      else __asm volatile("" : : : "memory");
    }
  }
}

void hierarchy_reduce(int nid,
                      void *reduce_data,
                      void (*reduce_func)(void *lhs, void *rhs))
{
  bool leader;
  size_t ticket;

  leader = hierarchy_select_leader_optimistic(&popcorn_node[nid].tinfo,
                                              &ticket);
  if(leader)
  {
    hierarchy_reduce_leader(nid, reduce_data, reduce_func);
    hierarchy_leader_cleanup(&popcorn_node[nid].tinfo);
  }
  else hierarchy_reduce_local(nid, ticket, reduce_data);
}

