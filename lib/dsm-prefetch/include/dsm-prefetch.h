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

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* How a thread will access memory. */
typedef enum {
  READ,    /* Read/replicated permissions */
  WRITE,   /* Write/exclusive permissions */
  EXECUTE, /* Execution permissions -- currently unimplemented! */
  RELEASE  /* Release current permissions */
} access_type_t;

/*
 * Request prefetching for a contiguous span of memory for the node on which
 * the thread is currently executing.  Prefetch the pages containing up to but
 * excluding the highest address.  Note this API does not prefetch anything,
 * but only queues a request to be sent by popcorn_prefetch_execute().
 *
 * @param type how the thread will be accessing the memory
 * @param low the lowest address of the memory span
 * @param high the highest address of the memory span
 */
void popcorn_prefetch(access_type_t type, const void *low, const void *high);

/*
 * Request prefetching for a contiguous span of memory on a node.  Prefetch the
 * pages containing up to but excluding the highest address.  Note this API
 * does not prefetch anything, but only queues a request to be sent by
 * popcorn_prefetch_execute().
 *
 * @param nid the node on which the thread will be accessing the memory
 * @param type how the thread will be accessing the memory
 * @param low the lowest address of the memory span
 * @param high the highest address of the memory span
 */
void popcorn_prefetch_node(int nid,
                           access_type_t type,
                           const void *low,
                           const void *high);

/*
 * Return the number of prefetch requests currently batched for a given node &
 * access type.
 *
 * @param nid the node for the prefetch requests
 * @param type access type
 * @return the number of prefetch requests batched
 */
size_t popcorn_prefetch_num_requests(int nid, access_type_t type);

/*
 * Inform the DSM of all outstanding prefetch requests for the node on which
 * the thread is currently executing and clear the queued requests.  Only needs
 * to be called once per node.
 *
 * Note: if manual asynchronous prefetching is enabled, the return value of
 * prefetch requests executed is approximate -- rather than synchronizing with
 * the thread to get an exact answer, return potentially-stale information.
 *
 * @return the number of prefetch requests executed
 */
size_t popcorn_prefetch_execute();

/*
 * Inform the DSM of all outstanding prefetch requests for the specified node
 * and clear the queued requests.  Only needs to be called once per node.
 *
 * Note: if manual asynchronous prefetching is enabled, the return value of
 * prefetch requests executed is approximate -- rather than synchronizing with
 * the thread to get an exact answer, return potentially-stale information.
 *
 * @param nid the node for which to prefetch data
 * @return the number of prefetch requests executed
 */
size_t popcorn_prefetch_execute_node(int nid);

#ifdef __cplusplus
}
#endif

#endif

