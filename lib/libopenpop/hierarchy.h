/*
 * Provides hierarchy abstractions for threads executing in Popcorn Linux.
 *
 * Copyright Rob Lyerly, SSRG, VT, 2018
 */

#ifndef _HIERARCHY_H
#define _HIERARCHY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "libgomp.h"
#include "platform.h"

///////////////////////////////////////////////////////////////////////////////
// Type definitions & declarations
///////////////////////////////////////////////////////////////////////////////

#define ALIGN_PAGE __attribute__((aligned(PAGESZ)))
#define ALIGN_CACHE __attribute__((aligned(64)))

typedef struct htab *htab_t;

/* Hierarchical reduction configuration */
#define REDUCTION_ENTRIES 48UL
typedef union {
  void *p;
  char padding[64];
} ALIGN_CACHE aligned_void_ptr;

/* Leader selection information */
typedef struct {
  /* Number of participants in the leader selection process */
  size_t ALIGN_CACHE num;

  /* Number of participants that have not entered the leader selection
     process. */
  size_t ALIGN_CACHE remaining;
} leader_select_t;

/* Global Popcorn execution information.  The read-only/read-mostly data (flags
   & thread placement locations) are placed on the first page, whereas data
   that is meant to be shared across nodes is on subsequent pages. */
typedef struct {
  /* Execution flags */
  bool distributed; /* Are we running distributed? */
  bool finished; /* Are we doing end-of-application cleanup? */

  /* Enable/disable functionality */
  bool hybrid_barrier;
  bool hybrid_reduce;
  bool het_workshare;

  /* Popcorn nodes available & thread placement across nodes */
  unsigned long nodes;
  unsigned long threads_per_node[MAX_POPCORN_NODES];

  /* The compute power "rating" of cores on each node.  For example, an
     individual core on a node with a rating of 2 is considered to be twice as
     fast as a core a node with a rating of 1, and will get twice as much work.
     The static scheduler will adjust how work is distributed according to the
     node's rating. */
  struct {
    unsigned long core_speed_rating[MAX_POPCORN_NODES];
    unsigned long scaled_thread_range;
  };

  /* Cache of computed core speeds from the probing scheduler */
  htab_t workshare_cache;

  /* Global node leader selection */
  leader_select_t ALIGN_PAGE sync;
  leader_select_t ALIGN_CACHE opt;

  /* Global barrier for use in hierarchical barrier */
  gomp_barrier_t ALIGN_PAGE bar;

  /* Global reduction space */
  aligned_void_ptr ALIGN_PAGE reductions[MAX_POPCORN_NODES];

  /* Global work share */
  union ALIGN_PAGE {
    gomp_ptrlock_t ws_lock;
    struct gomp_work_share *ws;
  };

  /* Global timing information for the heterogeneous probing scheduler */
  unsigned long long workshare_time[MAX_POPCORN_NODES];

  /* Global page faults during the probing period */
  unsigned long long page_faults;

  union {
    long split[MAX_POPCORN_NODES+1];
    unsigned long split_ull[MAX_POPCORN_NODES+1];
  };
} global_info_t;

/* Per-node hierarchy information.  This should all be accessed locally
   per-node, meaning nothing needs to be separated onto multiple pages. */
typedef struct {
  /* Per-node thread information */
  leader_select_t ALIGN_CACHE sync, opt;

  /* Per-node barrier for use in hierarchical barrier */
  gomp_barrier_t ALIGN_CACHE bar;

  /* Per-node reduction space */
  aligned_void_ptr reductions[REDUCTION_ENTRIES];

  /* Per-node work shares.  Maintains a local view of the work-sharing region
     which will be replenished dynamically from the global work distribution
     queue. */
  union {
    gomp_ptrlock_t ws_lock;
    struct gomp_work_share *ws;
  };

  /* Per-node timing information for the heterogeneous probing scheduler */
  unsigned long long workshare_time;

  /* Per-node page fault counts read from '/proc/popcorn_stat'.  Counts are not
     kept consistent between nodes so when calculating page faults during probe
     period we *must* use the difference in fault counts from the same node. */
  unsigned long long page_faults;

  char padding[PAGESZ - (2 * sizeof(leader_select_t))
                      - sizeof(gomp_barrier_t)
                      - (sizeof(aligned_void_ptr) * REDUCTION_ENTRIES)
                      - sizeof(gomp_ptrlock_t)
                      - sizeof(unsigned long long)
                      - sizeof(unsigned long long)];
} node_info_t;

extern global_info_t popcorn_global;
extern node_info_t popcorn_node[MAX_POPCORN_NODES];

///////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////

/*
 * Initialize global synchronization data structures.
 * @param nodes the number of nodes participating
 */
void hierarchy_init_global(int nodes);

/*
 * Initialize per-node synchronization data structures.
 * @param nid the node ID
 */
void hierarchy_init_node(int nid);

/*
 * Return the node on which a thread should execute given the user's places
 * specification.  Updates internal counters to reflect the placement.
 *
 * @param tid the number of the thread being released as part of a team
 * @return the node on which the thread should execute
 */
int hierarchy_assign_node(unsigned tnum);

///////////////////////////////////////////////////////////////////////////////
// Barriers
///////////////////////////////////////////////////////////////////////////////

/*
 * Execute a hybrid barrier.
 * @param nid the node in which to participate.
 * @param desc an optional description of the barrier location
 */
void hierarchy_hybrid_barrier(int nid, const char *desc);

/*
 * Execute a cancellable hybrid barrier.
 * @param nid the node in which to participate.
 * @param desc an optional description of the barrier location
 * @return true if cancelled or false otherwise
 */
bool hierarchy_hybrid_cancel_barrier(int nid, const char *desc);

/*
 * Execute the end-of-parallel-region hybrid barrier.
 * @param nid the node in which to participate.
 * @param desc an optional description of the barrier location
 */
void hierarchy_hybrid_barrier_final(int nid, const char *desc);

///////////////////////////////////////////////////////////////////////////////
// Reductions
///////////////////////////////////////////////////////////////////////////////

/*
 * Execute a tree reduction; use an optimistic leader selection process, reduce
 * locally then globally.
 *
 * @param nid the node in which to execute reductions
 * @param reduce_data the leader's payload
 * @param reduce_func function which executes the reduction
 * @return true if one final reduction is needed or false otherwise
 */
bool hierarchy_reduce(int nid,
                      void *reduce_data,
                      void (*reduce_func)(void *lhs, void *rhs));

///////////////////////////////////////////////////////////////////////////////
// Work sharing
///////////////////////////////////////////////////////////////////////////////

/*
 * Initialize work-sharing construct for static scheduling.
 *
 * @param nid the node for which to initialize a work-sharing construct
 * @param lb the lower bound
 * @param ub the upper bound
 * @param incr the increment
 * @param chunk the chunk size
 */
void hierarchy_init_workshare_static(int nid,
                                     long long lb,
                                     long long ub,
                                     long long incr,
                                     long long chunk);

/* Same as above but with unsigned long long types */
void hierarchy_init_workshare_static_ull(int nid,
                                         unsigned long long lb,
                                         unsigned long long ub,
                                         unsigned long long incr,
                                         unsigned long long chunk);

/*
 * Initialize work-sharing construct using the hierarchical dynamic scheduler
 * for the node.
 *
 * @param nid the node for which to initialize a work-sharing construct
 * @param lb the lower bound
 * @param ub the upper bound
 * @param incr the increment
 * @param chunk the chunk size
 */
void hierarchy_init_workshare_dynamic(int nid,
                                      long long lb,
                                      long long ub,
                                      long long incr,
                                      long long chunk);

/* Same as above but with unsigned long long types */
void hierarchy_init_workshare_dynamic_ull(int nid,
                                          unsigned long long lb,
                                          unsigned long long ub,
                                          unsigned long long incr,
                                          unsigned long long chunk);

/*
 * Initialize work-sharing construct using the heterogeneous probing scheduler
 * for the node.
 *
 * @param nid the node for which to initialize a work-sharing construct
 * @param ident a pointer uniquely identifying the work-sharing region
 * @param lb the loop iteration range's lower bound
 * @param ub the loop iteration range's upper bound
 * @param st the stride
 * @param chunk the chunk size
 */
void hierarchy_init_workshare_hetprobe(int nid,
                                       const void *ident,
                                       long long lb,
                                       long long ub,
                                       long long incr,
                                       long long chunk);

/* Same as above but with unsigned long long types */
void hierarchy_init_workshare_hetprobe_ull(int nid,
                                           const void *ident,
                                           unsigned long long lb,
                                           unsigned long long ub,
                                           unsigned long long incr,
                                           unsigned long long chunk);

/*
 * Grab the next batch of iterations from the local work share.  Replenish from
 * the global work share if necessary.
 *
 * Note: should be called for the first iteration by the GFS_HETPROBE scheduler
 * algorithm, after which hierarchy_next_hetprobe() should be called
 *
 * @param nid the node for which to grab more work
 * @param start pointer to variable to be set to the start of range
 * @param end pointer to variable to be set to the end of range
 * @return true if there's work remaining to be performed
 */
bool hierarchy_next_dynamic(int nid, long *start, long *end);

/* Same as above but with unsigned long long types */
bool hierarchy_next_dynamic_ull(int nid,
                                unsigned long long *start,
                                unsigned long long *end);

/*
 * Called *after* the probing period for the heterogeneous probing scheduler.
 * Divides remaining global iterations between nodes according to timing
 * information, then equally divides per-node iterations among all threads on
 * the node.
 *
 * @param nid the node for which to grab more work
 * @param ident a pointer uniquely identifying the work-sharing region
 * @param start pointer to variable to be set to the start of range
 * @param end pointer to variable to be set to the end of range
 * @return true if there's work remaining to be performed
 */
bool hierarchy_next_hetprobe(int nid,
                             const void *ident,
                             long *start,
                             long *end);

/* Same as above but with unsigned long long types */
bool hierarchy_next_hetprobe_ull(int nid,
                                 const void *ident,
                                 unsigned long long *start,
                                 unsigned long long *end);

/*
 * Return whether the specified iteration is the last iteration for the work
 * share.
 *
 * @param the iteration to check
 * @return true if it's the last iteration, false otherwise
 */
bool hierarchy_last(long end);

/* Same as above but with unsigned long long types */
bool hierarchy_last_ull(unsigned long long end);

/*
 * Clean up after a work-sharing region, freeing all resources.
 *
 * @param nid the node for which to (potentially) clean up resources
 * @param global whether the global workshare was used (dynamic, hetprobe
 *               scheduler) or not (static scheduler)
 */
void hierarchy_loop_end(int nid, bool global);

#endif /* _HIERARCHY_H */

