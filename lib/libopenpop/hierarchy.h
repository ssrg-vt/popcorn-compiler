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

typedef struct {
  /* Popcorn thread placement */
  bool ALIGN_PAGE *nodes;
  unsigned long num_nodes;
  unsigned long threads_per_node;

  /* Enable/disable Popcorn-specific optimizations */
  bool hybrid_barrier;

  /* Global barrier for use in hierarchical barrier */
  gomp_barrier_t ALIGN_CACHE bar;
} ALIGN_PAGE global_info_t;

/* Per-node hierarchy information */
typedef struct {
  /* Number of threads executing on the node */
  size_t ALIGN_CACHE threads;

  /* Number of threads that have not entered the leader selection process for 
     synchronous leader selection, i.e., number of threads remaining before a
     leader can be selected */
  size_t ALIGN_CACHE remaining;

  /* Whether or not a leader has been selected */
  bool ALIGN_CACHE has_leader;

  /* Local barrier for use in hierarchical barrier */
  gomp_barrier_t ALIGN_CACHE bar;
} ALIGN_PAGE node_info_t;

extern global_info_t popcorn_global;
extern node_info_t popcorn_node[MAX_POPCORN_NODES];

/*
 * Initialize all per-node data structures.
 * @param nid the node ID
 */
void hierarchy_init_node(int nid);

/*
 * Enter the leader selection process for node NID.  Return whether the thread
 * was selected as leader. The optimistic implementation selects the first
 * thread to enter the selection process as the leader.
 *
 * @param nid the node on which to select a leader
 * @return true if selected as leader for node nid or false otherwise
 */
bool hierarchy_select_leader_optimistic(int nid);

/*
 * Enter the leader selection process for node NID.  Return whether the thread
 * was selected as leader. The synchronous implementation selects the last
 * thread to enter the selection process as the leader.
 *
 * @param nid the node on which to select a leader
 * @return true if selected as leader for node nid or false otherwise
 */
bool hierarchy_select_leader_synchronous(int nid);

/*
 * After leader has finished its work, clean up per-node data for next leader
 * selection process.
 *
 * @param nid the node on which to clean up leader selection data
 */
void hierarchy_leader_cleanup(int nid);

#endif /* _HIERARCHY_H */

