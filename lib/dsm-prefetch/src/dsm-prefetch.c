/*
 * Batch together & send prefetching hints to the DSM protocol
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <migrate.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>
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
  sem_t work;
  char padding[PAGESZ - sizeof(size_t) - sizeof(sem_t)];
} __attribute__((aligned (PAGESZ))) thread_arg_t;

/* Statically-allocated lists. */
static node_requests_t requests[MAX_POPCORN_NODES];

/* Statistics about prefetching */
typedef struct {
  size_t num; // Number of prefetch requests
  size_t pages; // Number of pages prefetched
  size_t time; // Time to prefetch, in nanoseconds
} stats_t;

#ifdef _MAPREFETCH
/* Threads for asynchronous manual prefetching */
static pthread_t prefetch_threads[MAX_POPCORN_NODES];
static thread_arg_t prefetch_params[MAX_POPCORN_NODES];
static void *prefetch_thread_main(void *arg);
#elif defined _STATISTICS
stats_t total_stats = { .num = 0, .pages = 0, .time = 0 };

static void accumulate_global_stats(stats_t *stats) {
  __atomic_fetch_add(&total_stats.num, stats->num, __ATOMIC_RELAXED);
  __atomic_fetch_add(&total_stats.pages, stats->pages, __ATOMIC_RELAXED);
  __atomic_fetch_add(&total_stats.time, stats->time, __ATOMIC_RELAXED);
}
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
    if(!node_available(i))
    {
      warn("Node %lu not available for prefetching\n", i);
      continue;
    }

    prefetch_params[i].nid = i;
    prefetch_params[i].exit = false;
    failed = sem_init(&prefetch_params[i].work, 0, 0);
    failed |= pthread_create(&prefetch_threads[i], NULL, prefetch_thread_main,
                             &prefetch_params[i]);
    if(failed) warn("Could not initialize prefetching thread %d\n", i);
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
    if(!node_available(i)) continue;

    prefetch_params[i].exit = true;
    sem_post(&prefetch_params[i].work);
    pthread_join(prefetch_threads[i], NULL);
    sem_destroy(&prefetch_params[i].work);
  }
}
#elif defined _STATISTICS
static void __attribute__((destructor)) print_stats() {
  const char *fn = NULL;
  FILE *out = stderr;

  if((fn = getenv(ENV_STAT_LOG_FN))) out = fopen(fn, "w");

  if(out)
    fprintf(out, "Executed %lu prefetch requests\n"
                 "Prefetched %lu pages\n"
                 "Prefetching took %lu nanoseconds\n",
            total_stats.num, total_stats.pages, total_stats.time);

  if(fn && out) fclose(out);
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
  memory_span_t span = { .low = (uint64_t)low, .high = (uint64_t)high };

  // Ensure prefetch request is for a valid node.
  if(nid < 0 || nid >= MAX_POPCORN_NODES)
  {
    warn("Invalid node ID %d\n", nid);
    return;
  }

  // Ensure prefetch request is for a valid memory span.  This can arise when
  // expressions used to calculate bounds may produce zero or high < low bounds
  // based on an application's runtime values.
  if(low >= high)
  {
    warn("Invalid bounds %p - %p: %s\n", low, high,
          low == high ? "zero-sized span" : "inverted bounds");
    return;
  }

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
    warn("Invalid node ID %d", nid);
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

/*
 * Manually read/write pages (without affecting semantics) in order to force
 * the DSM to bring the pages to the current node with appropriate permissions.
 *
 * Note: optnone is required otherwise the compiler optimizes out accesses
 */
static inline void __attribute__((optnone, unused))
prefetch_span_manual(access_type_t type, const memory_span_t *span)
{
  volatile char *mem;
  char c = 0;
  for(mem = (char *)span->low; mem < (char *)span->high; mem += PAGESZ)
  {
    switch(type)
    {
    case READ: c += __atomic_load_n(mem, __ATOMIC_RELAXED); break;
    case WRITE: c += __atomic_fetch_or(mem, 0, __ATOMIC_RELAXED); break;
    default: assert(false && "Unknown access type"); break;
    }
  }

  // Make sure we touch the last page
  mem = (char *)PAGE_ROUND_DOWN((uint64_t)mem);
  if(mem > (char *)span->low && mem < (char *)span->high)
  {
    switch(type)
    {
    case READ: c += __atomic_load_n(mem, __ATOMIC_RELAXED); break;
    case WRITE: c += __atomic_fetch_or(mem, 0, __ATOMIC_RELAXED); break;
    default: assert(false && "Unknown access type"); break;
    }
  }
}

/* Prefetch a given span */
static void prefetch_span(access_type_t type, const memory_span_t *span)
{
#ifdef _MANUAL_PREFETCH
  // Note: no manual analog to releasing ownership
  if(type != RELEASE) prefetch_span_manual(type, span);
  else
  {
    memory_span_t align = {
      .low = PAGE_ROUND_DOWN((uint64_t)span->low),
      .high = PAGE_ROUND_UP((uint64_t)span->high)
    };
    madvise((void *)align.low, SPAN_SIZE(align), MADV_RELEASE);
  }
#else
  memory_span_t align = {
    .low = PAGE_ROUND_DOWN((uint64_t)span->low),
    .high = PAGE_ROUND_UP((uint64_t)span->high)
  };

  switch(type)
  {
  case READ: madvise((void *)align.low, SPAN_SIZE(align), MADV_READ); break;
  case WRITE: madvise((void *)align.low, SPAN_SIZE(align), MADV_WRITE); break;
  case RELEASE:
    madvise((void *)align.low, SPAN_SIZE(align), MADV_RELEASE);
    break;
  default: assert(false && "Unknown access type"); break;
  }
#endif /* _MANUAL_PREFETCH */
}

/*
 * Core prefetching logic, used both in manual & OS-based prefetching.  By
 * default, only records the number of spans prefetched.  If _STATISTICS is
 * defined, records the number of pages and time to prefetch as well.
 */
static void popcorn_prefetch_execute_internal(int nid, stats_t *stats)
{
  const node_t *n, *end;
  const memory_span_t *span;
#ifdef _STATISTICS
  struct timespec start_time, end_time;
#endif
  stats->num = 0;
  stats->pages = 0;
  stats->time = 0;

  assert(0 <= nid && nid < MAX_POPCORN_NODES && "Invalid node ID");

  // We can't prefetch to another node, so warn & clear out lists to prevent
  // them from growing forever due to failed prefetch executions.
  if(current_nid() != nid) {
    warn("Cannot prefetch to node on which we're not running (%d vs. %d)\n",
         current_nid(), nid);
    list_clear(&requests[nid].write);
    list_clear(&requests[nid].read);
    list_clear(&requests[nid].release);
    return;
  }

  // Acquire locks to prevent other threads from trying to add new requests
  // while we're processing the lists.
  list_atomic_start(&requests[nid].read);
  list_atomic_start(&requests[nid].write);
  list_atomic_start(&requests[nid].release);

  // Send write requests
  n = list_begin(&requests[nid].write);
  end = list_end(&requests[nid].write);
  while(n != end)
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

#ifdef _STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif
    prefetch_span(WRITE, span);
#ifdef _STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    stats->pages += SPAN_NUM_PAGES(*span);
    stats->time += NS(end_time) - NS(start_time);
#endif
    stats->num++;

    n = list_next(n);
  }
  list_clear(&requests[nid].write);
  list_atomic_end(&requests[nid].write);

  // Send read requests
  n = list_begin(&requests[nid].read);
  end = list_end(&requests[nid].read);
  while(n != end)
  {
    span = list_get_span(n);

    // If we're prefetching a region, it doesn't make sense to release
    // ownership.  Remove any prefetched regions from the release list.
    list_remove(&requests[nid].release, span);

    debug("Node %d: executing prefetch of 0x%lx -> 0x%lx for reading\n",
          nid, span->low, span->high);

#ifdef _STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif
    prefetch_span(READ, span);
#ifdef _STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    stats->pages += SPAN_NUM_PAGES(*span);
    stats->time += NS(end_time) - NS(start_time);
#endif
    stats->num++;

    n = list_next(n);
  }
  list_clear(&requests[nid].read);
  list_atomic_end(&requests[nid].read);

  // Send release requests
  n = list_begin(&requests[nid].release);
  end = list_end(&requests[nid].release);
  while(n != end)
  {
    span = list_get_span(n);

    debug("Node %d: executing release of 0x%lx -> 0x%lx\n",
          nid, span->low, span->high);

#ifdef _STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif
    prefetch_span(RELEASE, span);
#ifdef _STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    stats->pages += SPAN_NUM_PAGES(*span);
    stats->time += NS(end_time) - NS(start_time);
#endif
    stats->num++;

    n = list_next(n);
  }
  list_clear(&requests[nid].release);
  list_atomic_end(&requests[nid].release);
}

size_t popcorn_prefetch_execute()
{
  return popcorn_prefetch_execute_node(current_nid());
}

size_t popcorn_prefetch_execute_node(int nid)
{
  stats_t stats;

  // Ensure prefetch request is for a valid node.
  if(nid < 0 || nid >= MAX_POPCORN_NODES)
  {
    warn("Invalid node ID %d", nid);
    return 0;
  }

#ifdef _MAPREFETCH
  stats.num = list_size(&requests[nid].write) +
              list_size(&requests[nid].read) +
              list_size(&requests[nid].release);
  sem_post(&prefetch_params[nid].work);
#else
  int current = current_nid();
  if(current != nid) migrate(nid, NULL, NULL);
  popcorn_prefetch_execute_internal(nid, &stats);
  if(current != nid) migrate(current, NULL, NULL);
#ifdef _STATISTICS
  accumulate_global_stats(&stats);
#endif
#endif

  return stats.num;
}

/* Prefetching thread main loop. */
static void * __attribute__((unused))
prefetch_thread_main(void *arg)
{
  stats_t stats = { .num = 0, .pages = 0, .time = 0 }, cur;
  thread_arg_t *param = (thread_arg_t *)arg;

  debug("PID %d: servicing prefetch requests for node %d\n",
        gettid(), param->nid);

  migrate(param->nid, NULL, NULL);
  if(current_nid() != param->nid) warn("PID %d: still on origin\n", gettid());

  sem_wait(&param->work);
  while(!param->exit)
  {
    debug("PID %d: prefetching for node %d\n", gettid(), param->nid);
    popcorn_prefetch_execute_internal(param->nid, &cur);
    stats.num += cur.num;
#ifdef _STATISTICS
    stats.pages += cur.pages;
    stats.time += cur.time;
#endif
    sem_wait(&param->work);
  }

  migrate(0, NULL, NULL);

#ifndef _STATISTICS
  debug("PID %d: executed %lu requests\n", gettid(), stats.num);
#else
  debug("PID %d: executed %lu requests, touched %lu pages, took %lu ns\n",
        gettid(), stats.num, stats.pages, stats.time);
#endif

  return NULL;
}

