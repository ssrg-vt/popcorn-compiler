/*
 * Batch together & send prefetching hints to the DSM protocol
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#ifdef _DEBUG
#include <stdio.h>
#endif
#include <stdbool.h>
#include <assert.h>
#include <migrate.h>
#include "platform.h"
#include "definitions.h"
#include "list.h"
#include "dsm-prefetch.h"
#include "sys/mman.h"

/* A set of per-node lists containing read & write prefetch requests. */
typedef struct {
  list_t read, write;
  char padding[PAGESZ - (2 * sizeof(list_t))];
} __attribute__((aligned (PAGESZ))) node_requests_t;

/* Statically-allocated lists. */
static node_requests_t requests[MAX_POPCORN_NODES];

/* Initialize all lists at application startup. */
static void __attribute__((constructor)) prefetch_initialize_lists()
{
  size_t i;
  for(i = 0; i < MAX_POPCORN_NODES; i++)
  {
    list_init(&requests[i].read, i);
    list_init(&requests[i].write, i);
  }
}

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

  assert(nid > -1 && nid < MAX_POPCORN_NODES && "Invalid node ID");

  // Rather than using an assert, print warning (if type=debug) and return.
  // This is because expressions used to calculate bounds may produce zero or
  // high < low bounds based on an application's runtime values.
  if(low >= high)
  {
#ifdef _DEBUG
    printf("WARNING: invalid bounds %p - %p: %s", low, high,
           low == high ? "zero-sized span" : "inverted bounds");
#endif
    return;
  }

  span.low = (uint64_t)low;
  span.high = (uint64_t)high;

#ifdef _DEBUG
  printf("Node %d: queueing prefetch of %p -> %p for %s\n", nid, low, high,
         type == READ ? "reading" : "writing");
#endif

  switch(type)
  {
  case READ: list_insert(&requests[nid].read, &span); break;
  case WRITE: list_insert(&requests[nid].write, &span); break;
  default: assert(false && "Unknown access type"); break;
  }
}

size_t popcorn_prefetch_num_requests(int nid, access_type_t type)
{
  assert(nid > -1 && nid < MAX_POPCORN_NODES && "Invalid node ID");

  switch(type)
  {
  case READ: return list_size(&requests[nid].read);
  case WRITE: return list_size(&requests[nid].write);
  default: assert(false && "Unknown access type"); break;
  }

  return UINT64_MAX;
}

static void
prefetch_span(int nid, access_type_t type, const memory_span_t *span)
{
  int current = current_nid();
  if(current != nid) migrate(nid, NULL, NULL);
#ifdef _MANUAL_PREFETCH
  volatile char *mem
  char c = 0, c2;
  for(mem = span->low; mem < span->high; mem += PAGESZ)
  {
    switch(type)
    {
    case READ: c += *(char *)mem; break;
    case WRITE:
      c2 = *(char *)mem;
      c += c2;
      *(char *)mem = c2;
      break;
    default: assert(false && "Unknown access type"); break;
    }
  }

  // Make sure we touch the last page
  mem &= ~0xfffUL;
  if(mem > span->low && mem < span->high)
  {
    switch(type)
    {
    case READ: c += *(char *)mem; break;
    case WRITE:
      c2 = *(char *)mem;
      c += c2;
      *(char *)mem = c2;
    default: assert(false && "Unknown access type"); break;
    }
  }
#else
  switch(type)
  {
  case READ: madvise((void *)span->low, SPAN_SIZE(*span), MADV_READ); break;
  case WRITE: madvise((void *)span->low, SPAN_SIZE(*span), MADV_WRITE); break;
  default: assert(false && "Unknown access type"); break;
  }
#endif
  if(current != nid) migrate(current, NULL, NULL);
}

size_t popcorn_prefetch_execute(int nid)
{
  size_t executed = 0;
  const node_t *n;
  const memory_span_t *span;

  assert(nid > -1 && nid < MAX_POPCORN_NODES && "Invalid node ID");

  list_atomic_start(&requests[nid].read);
  list_atomic_start(&requests[nid].write);

  // Send write requests
  n = list_begin(&requests[nid].write);
  while(n != list_end(&requests[nid].write))
  {
    span = list_get_span(n);

    // Rather than prefetching the same region for both reading and writing,
    // delete regions requested for writing from the read list
    list_remove(&requests[nid].read, span);
#ifdef _DEBUG
    printf("Node %d: executing prefetch of 0x%lx -> 0x%lx for writing\n",
           nid, span->low, span->high);
#endif
    prefetch_span(nid, WRITE, span);
    executed++;
    n = list_next(n);
  }
  list_clear(&requests[nid].write);

  // Send read requests
  n = requests[nid].read.head;
  while(n)
  {
    span = list_get_span(n);
#ifdef _DEBUG
    printf("Node %d: executing prefetch of 0x%lx -> 0x%lx for reading\n",
           nid, span->low, span->high);
#endif
    prefetch_span(nid, READ, span);
    executed++;
    n = list_next(n);
  }
  list_clear(&requests[nid].read);

  list_atomic_end(&requests[nid].write);
  list_atomic_end(&requests[nid].read);

  return executed;
}

