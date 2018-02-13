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
    list_init(&requests[i].read);
    list_init(&requests[i].write);
  }
}

void prefetch(access_type_t type, void *low, void *high)
{
  int nid = current_nid();
  prefetch_node(nid, type, low, high);
}

void prefetch_node(int nid, access_type_t type, void *low, void *high)
{
  memory_span_t span;

  assert(nid > -1 && nid < MAX_POPCORN_NODES && "Invalid node ID");
  assert(low < high && "Invalid memory span");

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

size_t prefetch_num_requests(int nid, access_type_t type)
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

static inline void
prefetch_span(int nid, access_type_t type, memory_span_t *span)
{
  int current = current_nid();
  if(current != nid) migrate(nid, NULL, NULL);
#ifdef _MANUAL_PREFETCH
  // TODO manually prefetch that crap
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

void prefetch_execute(int nid)
{
  node_t *n;

  assert(nid > -1 && nid < MAX_POPCORN_NODES && "Invalid node ID");

  /* Send read prefetch requests. */
  list_atomic_start(&requests[nid].read);
  n = requests[nid].read.head;
  while(n)
  {
#ifdef _DEBUG
    printf("Node %d: executing prefetch of 0x%lx -> 0x%lx for reading\n",
           nid, n->mem.low, n->mem.high);
#endif
    prefetch_span(nid, READ, &n->mem);
    n = n->next;
  }
  list_clear(&requests[nid].read);
  list_atomic_end(&requests[nid].read);

  /* Send write prefetch requests. */
  list_atomic_start(&requests[nid].write);
  n = requests[nid].write.head;
  while(n)
  {
#ifdef _DEBUG
    printf("Node %d: executing prefetch of 0x%lx -> 0x%lx for writing\n",
           nid, n->mem.low, n->mem.high);
#endif
    prefetch_span(nid, WRITE, &n->mem);
    n = n->next;
  }
  list_clear(&requests[nid].write);
  list_atomic_end(&requests[nid].write);
}

