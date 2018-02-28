/*
 * Batch together & send prefetching hints to the DSM protocol
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#include <assert.h>
#include <migrate.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>

#include "platform.h"
#include "definitions.h"
#include "list.h"
#include "dsm-prefetch.h"


///////////////////////////////////////////////////////////////////////////////
// Definitions, declarations & utilities
///////////////////////////////////////////////////////////////////////////////

/* Per-node lists containing read, write & release prefetch requests. */
typedef struct {
  list_t read, write, release;
  char padding[PAGESZ - (3 * sizeof(list_t))];
} __attribute__((aligned (PAGESZ))) node_requests_t;

/* Parameters for threads performing asynchronous manual prefetching. */
typedef struct {
  int nid;
  volatile bool exit;
  volatile size_t executed;
  sem_t work;
} thread_arg_t;

/* Statically-allocated lists. */
static node_requests_t requests[MAX_POPCORN_NODES];

#ifdef _MAPREFETCH
/* Threads for asynchronous manual prefetching */
static pthread_t prefetch_threads[MAX_POPCORN_NODES];
static thread_arg_t prefetch_params[MAX_POPCORN_NODES];
static void *prefetch_thread_main(void *arg);
#endif

/* Get a human-readable string for the access type. */
static inline const char * __attribute__((unused))
access_type_str(access_type_t type)
{
  switch(type)
  {
  case READ: return "reading";
  case WRITE: return "writing";
  case RELEASE: return "release";
  default: return "(unknown)";
  }
}

///////////////////////////////////////////////////////////////////////////////
// Initialization & cleanup
///////////////////////////////////////////////////////////////////////////////

/*
 * Initialize all lists & prefetching threads (if configured) at application
 * startup.
 */
static void __attribute__((constructor)) prefetch_initialize()
{
  size_t i;
  for(i = 0; i < MAX_POPCORN_NODES; i++)
  {
    list_init(&requests[i].read, i);
    list_init(&requests[i].write, i);
    list_init(&requests[i].release, i);
  }

#ifdef _MAPREFETCH
  int failed;
  for(i = 0; i < MAX_POPCORN_NODES; i++)
  {
    prefetch_params[i].nid = i;
    prefetch_params[i].exit = false;
    prefetch_params[i].executed = 0;
    failed = sem_init(&prefetch_params[i].work, 0, 0);
    failed |= pthread_create(&prefetch_threads[i], NULL, prefetch_thread_main,
                             &prefetch_params[i]);
    assert(!failed && "Could not initialize prefetching threads");
  }
#endif
}

#ifdef _MAPREFETCH
/* Join all prefetching threads. */
static void __attribute__((destructor)) prefetch_end()
{
  size_t i;
  for(i = 0; i < MAX_POPCORN_NODES; i++)
  {
    prefetch_params[i].exit = true;
    sem_post(&prefetch_params[i].work);
    pthread_join(prefetch_threads[i], NULL);
    sem_destroy(&prefetch_params[i].work);
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Prefetch request batching
///////////////////////////////////////////////////////////////////////////////

void popcorn_prefetch(access_type_t type, const void *low, const void *high)
{
  popcorn_prefetch_node(current_nid(), type, low, high);
}

void popcorn_prefetch_node(int nid,
                           access_type_t type,
                           const void *low,
                           const void *high)
{
  memory_span_t span;

  // Ensure prefetch request is for a valid node.
  if(nid < 0 || nid >= MAX_POPCORN_NODES)
  {
    debug("WARNING: invalid node ID %d", nid);
    return;
  }

  // Ensure prefetch request is for a valid memory span.  This can arise when
  // expressions used to calculate bounds may produce zero or high < low bounds
  // based on an application's runtime values.
  if(low >= high)
  {
    debug("WARNING: invalid bounds %p - %p: %s", low, high,
          low == high ? "zero-sized span" : "inverted bounds");
    return;
  }

  span.low = PAGE_ROUND_DOWN((uint64_t)low);
  span.high = PAGE_ROUND_UP((uint64_t)high);

  debug("Node %d: queueing span 0x%lx -> 0x%lx for %s\n",
        nid, span.low, span.high, access_type_str(type));

  switch(type)
  {
  case READ: list_insert(&requests[nid].read, &span); break;
  case WRITE: list_insert(&requests[nid].write, &span); break;
  case RELEASE: list_insert(&requests[nid].release, &span); break;
  default: assert(false && "Unknown access type"); break;
  }
}

size_t popcorn_prefetch_num_requests(int nid, access_type_t type)
{
  // Ensure prefetch request is for a valid node.
  if(nid < 0 || nid >= MAX_POPCORN_NODES)
  {
    debug("WARNING: invalid node ID %d", nid);
    return 0;
  }

  switch(type)
  {
  case READ: return list_size(&requests[nid].read);
  case WRITE: return list_size(&requests[nid].write);
  case RELEASE: return list_size(&requests[nid].release);
  default: assert(false && "Unknown access type"); break;
  }

  return UINT64_MAX;
}

///////////////////////////////////////////////////////////////////////////////
// Execute prefetch requests
///////////////////////////////////////////////////////////////////////////////

static inline void __attribute__((optnone, unused))
prefetch_span_manual(access_type_t type, const memory_span_t *span)
{
  volatile char *mem;
  char c = 0, c2;
  for(mem = (char *)span->low; mem < (char *)span->high; mem += PAGESZ)
  {
    switch(type)
    {
    case READ: c += *(char *)mem; break;
    case WRITE:
      c2 = *(char *)mem;
      c += c2;
      *(char *)mem = c2;
      break;
    case RELEASE:/* no manual analog */ break;
    default: assert(false && "Unknown access type"); break;
    }
  }

  // Make sure we touch the last page
  mem = (char *)((uint64_t)mem & ~0xfffUL);
  if(mem > (char *)span->low && mem < (char *)span->high)
  {
    switch(type)
    {
    case READ: c += *(char *)mem; break;
    case WRITE:
      c2 = *(char *)mem;
      c += c2;
      *(char *)mem = c2;
    case RELEASE:/* no manual analog */ break;
    default: assert(false && "Unknown access type"); break;
    }
  }
}

static void prefetch_span(access_type_t type, const memory_span_t *span)
{
#ifdef _MANUAL_PREFETCH
  prefetch_span_manual(type, span);
#else
  switch(type)
  {
  case READ: madvise((void *)span->low, SPAN_SIZE(*span), MADV_READ); break;
  case WRITE: madvise((void *)span->low, SPAN_SIZE(*span), MADV_WRITE); break;
  case RELEASE:
    madvise((void *)span->low, SPAN_SIZE(*span), MADV_RELEASE); break;
  default: assert(false && "Unknown access type"); break;
  }
#endif /* _MANUAL_PREFETCH */
}

/* Core prefetching logic, used both in manual & OS-based prefetching */
static size_t popcorn_prefetch_execute_internal(int nid)
{
  size_t executed = 0;
  const node_t *n;
  const memory_span_t *span;

  assert(nid > -1 && nid < MAX_POPCORN_NODES && "Invalid node ID");
  assert(current_nid() == nid && "Cannot prefetch to another node");

  list_atomic_start(&requests[nid].read);
  list_atomic_start(&requests[nid].write);
  list_atomic_start(&requests[nid].release);

  // Send write requests
  n = list_begin(&requests[nid].write);
  while(n != list_end(&requests[nid].write))
  {
    span = list_get_span(n);

    // Rather than prefetching the same region for both reading and writing,
    // delete regions requested for writing from the read list
    list_remove(&requests[nid].read, span);

    // If we're prefetching a region, it doesn't make sense to release
    // ownership.  Remove any prefetched regions from the release list.
    list_remove(&requests[nid].release, span);

    debug("Node %d: executing prefetch of 0x%lx -> 0x%lx for writing\n",
          nid, span->low, span->high);

    prefetch_span(WRITE, span);
    executed++;
    n = list_next(n);
  }
  list_clear(&requests[nid].write);
  list_atomic_end(&requests[nid].write);

  // Send read requests
  n = list_begin(&requests[nid].read);
  while(n != list_end(&requests[nid].read))
  {
    span = list_get_span(n);

    // If we're prefetching a region, it doesn't make sense to release
    // ownership.  Remove any prefetched regions from the release list.
    list_remove(&requests[nid].release, span);

    debug("Node %d: executing prefetch of 0x%lx -> 0x%lx for reading\n",
          nid, span->low, span->high);

    prefetch_span(READ, span);
    executed++;
    n = list_next(n);
  }
  list_clear(&requests[nid].read);
  list_atomic_end(&requests[nid].read);

  // Send release requests
  n = list_begin(&requests[nid].release);
  while(n != list_end(&requests[nid].release))
  {
    span = list_get_span(n);

    debug("Node %d: executing release of 0x%lx -> 0x%lx\n",
          nid, span->low, span->high);

    prefetch_span(RELEASE, span);
    executed++;
    n = list_next(n);
  }
  list_clear(&requests[nid].release);
  list_atomic_end(&requests[nid].release);

  return executed;
}

size_t popcorn_prefetch_execute()
{
  return popcorn_prefetch_execute_node(current_nid());
}

size_t popcorn_prefetch_execute_node(int nid)
{
  size_t executed;

  // Ensure prefetch request is for a valid node.
  if(nid < 0 || nid >= MAX_POPCORN_NODES)
  {
    debug("WARNING: invalid node ID %d", nid);
    return 0;
  }

#ifdef _MAPREFETCH
  executed = list_size(&requests[nid].write) +
             list_size(&requests[nid].read) +
             list_size(&requests[nid].release);
  sem_post(&prefetch_params[nid].work);
#else
  int current = current_nid();
  if(current != nid) migrate(nid, NULL, NULL);
  executed = popcorn_prefetch_execute_internal(nid);
  if(current != nid) migrate(current, NULL, NULL);
#endif

  return executed;
}

/* Prefetching thread main loop. */
static void * __attribute__((unused))
prefetch_thread_main(void *arg)
{
  thread_arg_t *param = (thread_arg_t *)arg;

  migrate(param->nid, NULL, NULL);

  sem_wait(&param->work);
  while(!param->exit)
  {
    debug("PID %d: prefetching for node %d\n", gettid(), param->nid);
    param->executed += popcorn_prefetch_execute_internal(param->nid);
    sem_wait(&param->work);
  }

  migrate(0, NULL, NULL);

  return NULL;
}

