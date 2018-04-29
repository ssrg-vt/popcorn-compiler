/*
 * List implementation.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#include "definitions.h"
#include "list.h"
#include "platform.h"

///////////////////////////////////////////////////////////////////////////////
// Node API
///////////////////////////////////////////////////////////////////////////////

/* A linked list node. */
typedef struct node_t {
  struct node_t *prev, *next;
  memory_span_t mem;
} node_t;

#ifndef _NOCACHE
/* Pre-allocated linked list nodes */
static node_t __attribute__((aligned(4096)))
node_cache[MAX_POPCORN_NODES][NODE_CACHE_SIZE];

/* "In use" flag for each node in the cache */
static bool __attribute__((aligned(4096)))
node_cache_used[MAX_POPCORN_NODES][PAGE_ROUND_UP(NODE_CACHE_SIZE)];
#endif

/* Allocate & initialize a new linked list node.  Note: not thread safe! */
static node_t *node_create(const memory_span_t *mem, int nid)
{
  int ret;
  node_t *n;

  assert(mem && "Invalid memory span pointer");
  assert(0 <= nid && nid < MAX_POPCORN_NODES && "Invalid node ID");

#ifndef _NOCACHE
  // Try to allocate from the node cache before dropping to malloc
  size_t i;
  for(i = 0; i < NODE_CACHE_SIZE; i++) {
    if(!node_cache_used[nid][i]) {
      node_cache_used[nid][i] = true;
      node_cache[nid][i].mem = *mem;
#ifdef _CHECKS
      node_cache[nid][i].prev = node_cache[nid][i].next = NULL;
#endif
      return &node_cache[nid][i];
    }
  }
#endif
  // TODO extreme internal fragmentation, use node-aware memory allocator
  ret = posix_memalign((void **)&n, PAGESZ, sizeof(node_t));
  assert(n && "Invalid node pointer");
  n->mem = *mem;
#ifdef _CHECKS
  n->prev = n->next = NULL;
#endif
  return n;
}

/* Free a linked list node.  Note: note thread safe! */
static void node_free(node_t *n)
{
  assert(n && "Invalid node pointer");
#ifdef _CHECKS
  n->prev = n->next = NULL;
  n->mem.low = n->mem.high = 0;
#endif
#ifndef _NOCACHE
  // Determine if the node was allocated from the cache or heap
  size_t node, entry;
  void *cast = (void *)n, *cache_start = (void *)node_cache,
       *cache_end = (void *)&node_cache[MAX_POPCORN_NODES];
  if(cache_start <= cast && cast < cache_end) {
    size_t offset = cast - cache_start,
           per_node_size = sizeof(node_t) * NODE_CACHE_SIZE;
    node = offset / per_node_size;
    entry = (offset % per_node_size) / sizeof(node_t);

    assert(node < MAX_POPCORN_NODES && entry < NODE_CACHE_SIZE &&
           "Invalid node cache entry calculation");
    assert(node_cache_used[node][entry] == true && "Invalid cache metadata");

    node_cache_used[node][entry] = false;
    return;
  }
#endif
  free(n);
}

///////////////////////////////////////////////////////////////////////////////
// List API
///////////////////////////////////////////////////////////////////////////////

/* Internal APIs */

/*
 * Return whether two memory spans should be merged, i.e., they contain
 * adjacent or overlapping memory regions.
 *
 * Note: a *must* be before b in sorted ordering.
 *
 * @param a a memory span
 * @param b a memory span
 * @return whether a & b overlap/are adjacent and should be merged
 */
static inline bool
list_check_merge(const memory_span_t *a, const memory_span_t *b)
{
  assert(a && b && a->low <= b->low &&
         "Invalid arguments to list_check_merge()");
  return (a->low == b->low) || (a->high >= b->low);
}

/*
 * Return whether two memory spans overlap.  This is slightly different from
 * merge checking -- two spans overlap *only if* the high edge of the earlier
 * span (in a sorted ordering) crosses the low edge of the later span.  They do
 * not overlap if they are only equal because memory spans denote memory
 * regions up to but *not including* the high address.
 *
 * Note: a *must* be before b in sorted ordering.
 *
 * @param a a memory span
 * @param b a memory span
 * @return whether a & b overlap
 */
static inline bool
list_check_overlap(const memory_span_t *a, const memory_span_t *b)
{
  assert(a && b && a->low <= b->low &&
         "Invalid arguments to list_check_overlap()");
  return (a->low == b->low) || (a->high > b->low);
}

/*
 * Return whether one memory span entirely contains another memory span, i.e.,
 * the memory in the second span is a subset or equal to the memory contained
 * in the first span.
 *
 * Note: a *must* be before b in sorted ordering.
 *
 * @param a a memory span
 * @param b a memory span
 * @return whether a contains b
 */
static inline bool
list_check_contained(const memory_span_t *a, const memory_span_t *b)
{
  assert(a && b && a->low <= b->low &&
         "Invalid arguments to list_check_contained()");
  return (a->low <= b->low) && (a->high >= b->high);
}

/*
 * Seek to the location in the list where the memory span would be inserted.
 * Return the successor node, i.e., the node directly after where the span
 * would be inserted.  For example, in the following list:
 *
 *           ----------     ----------
 *   ... <-> | 0x1000 | <-> | 0x3000 | <-> ...
 *           ----------     ----------
 *
 * calling list_seek() for a memory span starting at 0x2000 would return a
 * pointer to the node containing 0x3000.  Note that if the start address
 * already exists in a node in the list, list_seek() returns that node.  For
 * example, calling list_seek() for another memory span starting at 0x3000
 * would again return a pointer to the node containing 0x3000.
 *
 * @param l a list
 * @param mem a memory span
 * @return a node, or NULL if the span should be added as the tail
 */
static inline node_t *list_seek(list_t *l, const memory_span_t *mem)
{
  assert(l && mem && "Invalid arguments to list_seek()");
  if(l->head)
  {
    node_t *cur = l->head;
    while(cur && cur->mem.low < mem->low) cur = cur->next;
    return cur;
  }
  else return NULL;
}

/*
 * Merge two nodes.  Merges b into a and returns the merged node.
 *
 * Note: a *must* be directly before b in sorted ordering in the list.
 *
 * @param a a linked list node
 * @param b a linked list node
 * @return the new merged node
 */
static node_t *list_merge(list_t *l, node_t *a, node_t *b)
{
  assert(l && a && b &&
         a->mem.low <= b->mem.low &&
         a->next == b && b->prev == a &&
         "Invalid arguments to node merge");

  debug("Merging 0x%lx - 0x%lx and 0x%lx - 0x%lx to 0x%lx - 0x%lx\n",
        a->mem.low, a->mem.high, b->mem.low, b->mem.high,
        MIN(a->mem.low, b->mem.low), MAX(a->mem.high, b->mem.high));

  a->mem.high = MAX(a->mem.high, b->mem.high);
  a->next = b->next;
  if(a->next) a->next->prev = a;
  else l->tail = a; // b was the tail
  node_free(b);
  l->size--;
  return a;
}

/*
 * Delete a node from the list & return its successor.
 *
 * @param l a list
 * @param n a node
 * @return the node's successor in the list, or NULL if the node was the tail
 */
static node_t *list_delete(list_t *l, node_t *n)
{
  node_t *prev, *next = NULL;

  assert(l && n && "Invalid arguments to list_delete()");

  debug("Deleting 0x%lx - 0x%lx\n", n->mem.low, n->mem.high);

  if(l->size == 1)
  {
    l->head = l->tail = NULL;
    l->size = 0;
  }
  else
  {
    if(n == l->head)
    {
      l->head = n->next;
      l->head->prev = NULL;
      next = l->head;
    }
    else if(n == l->tail)
    {
      l->tail = n->prev;
      l->tail->next = NULL;
    }
    else
    {
      prev = n->prev;
      next = n->next;
      prev->next = next;
      next->prev = prev;
    }
    l->size--;
  }
  node_free(n);
  return next;
}

/* User-facing APIs */

void list_init(list_t *l, int nid)
{
  pthread_mutexattr_t attr;
  assert(l && "Invalid list pointer");
  l->head = l->tail = NULL;
  l->size = 0;
  l->nid = nid;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&l->lock, &attr);
  pthread_mutexattr_destroy(&attr);
}

size_t list_size(const list_t *l)
{
  assert(l && "Invalid list pointer");
  return l->size;
}

void list_insert(list_t *l, const memory_span_t *mem)
{
  node_t *cur, *prev, *next, *n;

  assert(l && mem && "Invalid arguments to list_insert()");
  assert(mem->low < mem->high && "Invalid memory span");

  pthread_mutex_lock(&l->lock);

  n = node_create(mem, l->nid);

  assert(n && "Invalid pointer returned by node_create()");

  if(!l->head)
  {
    // First node in the list
    l->head = l->tail = n;
    l->size = 1;
    n->prev = n->next = NULL;
  }
  else
  {
    // Insert the node into the list
    cur = list_seek(l, mem);
    if(cur == l->head) // First element in the list
    {
      cur->prev = n;
      n->next = cur;
      n->prev = NULL;
      l->head = n;
    }
    else if(!cur) // Last element in the list
    {
      cur = l->tail;
      cur->next = n;
      n->prev = cur;
      n->next = NULL;
      l->tail = n;
    }
    else // In the middle of the list
    {
      next = cur;
      cur = cur->prev;
      cur->next = n;
      n->prev = cur;
      next->prev = n;
      n->next = next;
    }
    l->size++;

    // Merge with predecessor span; can merge at most once.
    prev = n->prev;
    if(prev && list_check_merge(&prev->mem, &n->mem))
      n = list_merge(l, prev, n);

    // Merge with successor spans; can merge an arbitrary number of times.
    next = n->next;
    while(next && list_check_merge(&n->mem, &next->mem))
    {
      n = list_merge(l, n, next);
      next = n->next;
    }
  }
  pthread_mutex_unlock(&l->lock);
}

bool list_overlaps(list_t *l, const memory_span_t *mem)
{
  bool overlaps = false;
  node_t *next;

  assert(l && mem && "Invalid arguments to list_overlaps()");
  assert(mem->low < mem->high && "Invalid memory span");

  pthread_mutex_lock(&l->lock);
  if(l->head)
  {
    next = list_seek(l, mem);
    if(!next) overlaps = list_check_overlap(&l->tail->mem, mem);
    else overlaps = list_check_overlap(&next->prev->mem, mem) ||
                    list_check_overlap(mem, &next->mem);
  }
  pthread_mutex_unlock(&l->lock);

  return overlaps;
}

void list_remove(list_t *l, const memory_span_t *mem)
{
  node_t *cur, *prev;
  memory_span_t cur_span, new_span;

  assert(l && mem && "Invalid arguments to list_remove()");
  assert(mem->low < mem->high && "Invalid memory span");

  pthread_mutex_lock(&l->lock);
  if(l->head)
  {
    cur = list_seek(l, mem);

    // Remove overlapping region from predecessor; can split at most once.
    // Note that by definition the predecessor will not be a subset of mem
    // since it's lower bound *must* be before mem's lower bound.
    if(cur) prev = cur->prev;
    else prev = l->tail;
    if(prev && list_check_overlap(&prev->mem, mem))
    {
      if(prev->mem.high <= mem->high)
      {
        debug("Resizing 0x%lx - 0x%lx to 0x%lx - 0x%lx\n",
              prev->mem.low, prev->mem.high, prev->mem.low, mem->low);

        prev->mem.high = mem->low;
      }
      else
      {
        // The memory region being removed is a strict subset of prev -- split
        // prev into two new nodes with mem removed.
        debug("Replacing 0x%lx - 0x%lx with 0x%lx - 0x%lx & 0x%lx - 0x%lx\n",
              prev->mem.low, prev->mem.high, prev->mem.low, mem->low,
              mem->high, prev->mem.high);

        cur_span = prev->mem;
        list_delete(l, prev);

        new_span.low = cur_span.low;
        new_span.high = mem->low;
        list_insert(l, &new_span);

        new_span.low = mem->high;
        new_span.high = cur_span.high;
        list_insert(l, &new_span);
      }
    }

    // Remove overlapping regions from successors; can split an arbitrary
    // number of times.  Note that by definition mem will not be a strict
    // subset of the successor since its lower bound *must* be less than or
    // equal to the successor's lower bound.
    while(cur && list_check_overlap(mem, &cur->mem))
    {
      if(list_check_contained(mem, &cur->mem)) cur = list_delete(l, cur);
      else
      {
        debug("Resizing 0x%lx - 0x%lx to 0x%lx - 0x%lx\n",
              cur->mem.low, cur->mem.high, mem->high, cur->mem.high);

        cur->mem.low = mem->high;
      }
    }
  }
  pthread_mutex_unlock(&l->lock);
}

void list_clear(list_t *l)
{
  node_t *cur, *next;

  pthread_mutex_lock(&l->lock);
  cur = l->head;
  while(cur)
  {
    next = cur->next;
    node_free(cur);
    cur = next;
  }
  l->head = NULL;
  l->tail = NULL;
  l->size = 0;
  pthread_mutex_unlock(&l->lock);
}

void list_atomic_start(list_t *l) {
  assert(l && "Invalid arguments to list_atomic_start()");
  pthread_mutex_lock(&l->lock);
}

void list_atomic_end(list_t *l) {
  assert(l && "Invalid arguments to list_atomic_end()");
  pthread_mutex_unlock(&l->lock);
}

const node_t *list_begin(list_t *l) {
  assert(l && "Invalid arguments to list_begin()");
  return l->head;
}

const node_t *list_next(const node_t *n) {
  if(n) return n->next;
  else return NULL;
}

const node_t *list_end(list_t *l) { return NULL; }

const memory_span_t *list_get_span(const node_t *n) {
  assert(n && "Invalid arguments to list_get_span()");
  return &n->mem;
}

void list_print(list_t *l)
{
  node_t *cur;

  assert(l && "Invalid arguments to list_print()");

  pthread_mutex_lock(&l->lock);
  printf("List for node %d (%p) contains %lu span(s)\n", l->nid, l, l->size);
  cur = l->head;
  while(cur)
  {
    printf("  0x%lu - 0x%lu\n", cur->mem.low, cur->mem.high);
    cur = cur->next;
  }
  pthread_mutex_unlock(&l->lock);
}

