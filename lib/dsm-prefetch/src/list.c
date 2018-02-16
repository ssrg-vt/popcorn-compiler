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
#include "list.h"

///////////////////////////////////////////////////////////////////////////////
// Node API
///////////////////////////////////////////////////////////////////////////////

/* Allocate & initialize a new linked list node. */
static node_t *node_create(const memory_span_t *mem, int nid)
{
  assert(mem && "Invalid memory span pointer");
  // TODO use node-aware memory allocator
  node_t *n = (node_t *)malloc(sizeof(node_t));
  assert(n && "Invalid node pointer");
  n->mem = *mem;
#ifdef _DEBUG
  n->prev = n->next = NULL;
#endif
  return n;
}

/* Free a linked list node. */
static void node_free(node_t *n)
{
  assert(n && "Invalid node pointer");
#ifdef _DEBUG
  n->prev = n->next = NULL;
  n->mem.low = n->mem.high = 0;
#endif
  free(n);
}

///////////////////////////////////////////////////////////////////////////////
// List API
///////////////////////////////////////////////////////////////////////////////

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

/*
 * Return whether two nodes should be merged, i.e., they contain adjacent or
 * overlapping memory regions.
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
  assert(a && b && a->low <= b->low && "Invalid arguments to merge checking");
  return (a->low == b->low) || (a->high >= b->low);
}

/*
 * Return whether two nodes overlap.  This is slightly different from merge
 * checking -- two spans overlap only if the high edge of the earlier span (in
 * a sorted ordering) crosses the low edge of the later span, and do not
 * overlap if they are only equal.  This is because memory spans denote memory
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
  assert(a && b && a->low <= b->low && "Invalid arguments to overlap checking");
  return (a->low == b->low) || (a->high > b->low);
}

/*
 * Merge two nodes.  Merges b in to a and returns the merged node.
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

#ifdef _DEBUG
  printf("Merging 0x%lx - 0x%lx and 0x%lx - 0x%lx to 0x%lx - 0x%lx\n",
         a->mem.low, a->mem.high, b->mem.low, b->mem.high,
         MIN(a->mem.low, b->mem.low), MAX(a->mem.high, b->mem.high));
#endif

  a->mem.high = MAX(a->mem.high, b->mem.high);
  a->next = b->next;
  if(a->next) a->next->prev = a;
  else l->tail = a; // b was the tail
  node_free(b);
  l->size--;
  return a;
}

void list_insert(list_t *l, const memory_span_t *mem)
{
  node_t *cur, *prev, *next, *n = node_create(mem, l->nid);

  assert(l && mem && "Invalid arguments to list_insert()");
  assert(n && "Invalid pointer returned by node_create()");
  assert(mem->low < mem->high && "Invalid memory span");

  pthread_mutex_lock(&l->lock);
  if(!l->head)
  {
    // Nothing in the list
    l->head = n;
    l->tail = n;
    l->size = 1;
    n->prev = n->next = NULL;
  }
  else
  {
    // Find the node's place in the list
    cur = l->head;
    while(cur && cur->mem.low < n->mem.low) cur = cur->next;

    // Insert the node into the list
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
    else if(cur->mem.low == n->mem.low) // Overlapping, merge cur & n
      n = list_merge(l, cur, n);
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
    if(list_check_merge(&n->prev->mem, &n->mem)) n = list_merge(l, prev, n);

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

  pthread_mutex_lock(&l->lock);
  if(l->head)
  {
    // Seek to the location in the list where it could potentially overlap
    next = l->head;
    while(next && next->mem.low < mem->low) next = next->next;

    if(!next) overlaps = list_check_overlap(&l->tail->mem, mem);
    else
    {
      overlaps = list_check_overlap(&next->prev->mem, mem) ||
                 list_check_overlap(mem, &next->mem);
    }
  }
  pthread_mutex_unlock(&l->lock);

  return overlaps;
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

void list_atomic_start(list_t *l) { pthread_mutex_lock(&l->lock); }

void list_atomic_end(list_t *l) { pthread_mutex_unlock(&l->lock); }

void list_print(list_t *l)
{
  node_t *cur;

  assert(l && "Invalid arguments to list_print()");

  pthread_mutex_lock(&l->lock);
  printf("List for node %d (%p) contains %lu spans\n", l->nid, l, l->size);
  cur = l->head;
  while(cur)
  {
    printf("  0x%lu - 0x%lu\n", cur->mem.low, cur->mem.high);
    cur = cur->next;
  }
  pthread_mutex_unlock(&l->lock);
}

