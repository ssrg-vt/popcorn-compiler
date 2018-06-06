/*
 * Provides hierarchy abstractions for threads executing in Popcorn Linux.
 *
 * Copyright Rob Lyerly, SSRG, VT, 2018
 */

#ifndef _HIERARCHY_H
#define _HIERARCHY_H

#include <stddef.h>
#include <stdbool.h>
#include "bar.h"
#include "platform.h"

#define ALIGN_PAGE __attribute__((aligned(PAGESZ)))
#define ALIGN_CACHE __attribute__((aligned(64)))

/* Hierarchical reduction configuration & syntactic sugar */
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

  /* Enable/disable optimizations */
  bool hybrid_barrier;
  bool hybrid_reduce;

  /* Popcorn nodes available & thread placement across nodes */
  unsigned long nodes;
  unsigned long threads_per_node[MAX_POPCORN_NODES];

  /* Global node leader selection */
  leader_select_t ALIGN_PAGE sync;
  leader_select_t ALIGN_CACHE opt;

  /* Global barrier for use in hierarchical barrier */
  gomp_barrier_t ALIGN_PAGE bar;

  /* Global reduction space */
  aligned_void_ptr ALIGN_PAGE reductions[MAX_POPCORN_NODES];
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

  char padding[PAGESZ - sizeof(leader_select_t) - sizeof(gomp_barrier_t)
                      - (sizeof(aligned_void_ptr) * REDUCTION_ENTRIES)];
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
 */
void hierarchy_hybrid_barrier(int nid);

/*
 * Execute a cancellable hybrid barrier.
 * @param nid the node in which to participate.
 * @return true if cancelled or false otherwise
 */
bool hierarchy_hybrid_cancel_barrier(int nid);

/*
 * Execute the end-of-parallel-region hybrid barrier.
 * @param nid the node in which to participate.
 */
void hierarchy_hybrid_barrier_final(int nid);

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

#endif /* _HIERARCHY_H */

