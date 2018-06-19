/*
 * Popcorn-specific platform information.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#ifndef _PLATFORM_H
#define _PLATFORM_H

/* The size of a page in the system. */
#define PAGESZ 4096UL

/*
 * Round down & up to the nearest pages, respectively.  Arguments must be of
 * unsigned long/uint64_t type.
 */
#define PAGE_ROUND_DOWN( x ) ((x) & ~(PAGESZ - 1))
#define PAGE_ROUND_UP( x ) PAGE_ROUND_DOWN((x) + PAGESZ - 1)

/* The maximum number of nodes supported by the system. */
#define MAX_POPCORN_NODES 32

/* Status of thread within Popcorn's single system image */
struct popcorn_thread_status {
  int current_nid;  /* The thread's current node */
  int proposed_nid; /* Destination node if somebody proposed migration */
  int peer_nid;     /* Node ID of peer thread in SSI */
  int peer_pid;     /* PID of peer thread in SSI */
};

/*
 * Return the node ID on which the current thread is executing.
 * @return the current node ID or -1 otherwise
 */
int popcorn_getnid();

/*
 * Query thread status information.  Populates the thread status struct with
 * the current thread's status.
 *
 * @param status thread status struct
 * @return 0 if completed successfully or non-zero otherwise
 */
int popcorn_getthreadinfo(struct popcorn_thread_status *status);

/* Status of nodes in Popcorn's single system image */
struct popcorn_node_status {
  unsigned int status; /* 1 if online, 0 if not */
  int arch;            /* Architecture of node -- see arch.h */
  int distance;        /* Hop distance between current and other node */
};

/*
 * Query node status information.  Populates the integer passed via pointer
 * with the ID of the origin node and populates the array of nodes status
 * structs with their current status.
 *
 * @param origin pointer to integer to be set with the origin ID
 * @param status array of node status structs, must have MAX_POPCORN_NODES
 *               elements
 * @return 0 if completed successfully or non-zero otherwise
 */
int popcorn_getnodeinfo(int *origin,
                        struct popcorn_node_status status[MAX_POPCORN_NODES]);

#endif /* _PLATFORM_H */

