/*
 * Provide prefetching hints to the DSM protocol.  In order to reduce the
 * amount of information passed to the DSM protocol, all threads on the same
 * node should batch together prefetch requests.  After all requests have been
 * accumulated, a single thread should execute the prefetch requests on behalf
 * of all threads on a node.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#ifndef _DSM_PREFETCH_H
#define _DSM_PREFETCH_H

#include <stddef.h>

/* How a thread will access memory. */
typedef enum {
  READ,
  WRITE,
  EXECUTE
} access_type_t;

/*
 * Request prefetching for a contiguous span of memory for the node on which
 * the thread is currently executing.  Prefetch up to but excluding the highest
 * address.  Note this API does not prefetch anything, but only queues a
 * request to be sent by prefetch_execute().
 *
 * @param type how the thread will be accessing the memory
 * @param low the lowest address of the memory span
 * @param high the highest address of the memory span
 */
void prefetch(access_type_t type, void *low, void *high);

/*
 * Request prefetching for a contiguous span of memory on a node.  Prefetch up
 * to but excluding the highest address.  Note this API does not prefetch
 * anything, but only queues a request to be sent by prefetch_execute().
 *
 * @param nid the node on which the thread will be accessing the memory
 * @param type how the thread will be accessing the memory
 * @param low the lowest address of the memory span
 * @param high the highest address of the memory span
 */
void prefetch_node(int nid, access_type_t type, void *low, void *high);

/*
 * Return the number of prefetch requests currently batched for a given
 * node & access type.
 *
 * @param nid the node for the prefetch requests
 * @param type access type
 * @return the number of prefetch requests batched
 */
size_t prefetch_num_requests(int nid, access_type_t type);

/*
 * Inform the DSM of all outstanding prefetch requests for the specified node.
 * Thread safe, but really only needs to be called by one thread per node.
 *
 * @param nid the node for which to prefetch data
 */
void prefetch_execute(int nid);

#endif

