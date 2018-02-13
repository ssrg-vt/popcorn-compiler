/*
 * List implementation.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

#include <stdlib.h>
#ifdef _DEBUG
#include <stdio.h>
#endif
#include <assert.h>
#include <stdbool.h>
#include "list.h"

///////////////////////////////////////////////////////////////////////////////
// Node API
///////////////////////////////////////////////////////////////////////////////

/* Allocate & initialize a new linked list node. */
static node_t *node_create(memory_span_t *mem)
{
  assert(mem && "Invalid memory span pointer");
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

void list_init(list_t *list)
{
  pthread_mutexattr_t attr;
  assert(list && "Invalid list pointer");
  list->head = list->tail = NULL;
  list->size = 0;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&list->lock, &attr);
  pthread_mutexattr_destroy(&attr);
}

size_t list_size(list_t *l)
{
  assert(l && "Invalid list pointer");
  return l->size;
}

/*
 * Return whether two nodes should be merged, i.e., they contain overlapping
 * memory regions.  Note: a *must* be directly before b in sorted ordering in
 * the list.
 *
 * @param a a linked list node
 * @param b another linked list node
 * @return whether a & b overlap and should thus be merged
 */
static bool list_check_merge(node_t *a, node_t *b)
{
  assert(a && b &&
         a->mem.low < b->mem.low &&
         a->next == b && b->prev == a &&
         "Invalid arguments to merge checking");
  return a->mem.high >= b->mem.low;
}

/*
 * Merge two nodes.  Merges b in to a and returns the merged node.  Note: a
 * *must* be directly before b in sorted ordering in the list.
 *
 * @param a a linked list node
 * @param b another linked list node
 * @return the new merged node
 */
static node_t *list_merge(list_t *l, node_t *a, node_t *b)
{
  assert(l && a && b &&
         a->mem.low < b->mem.low &&
         a->next == b && b->prev == a &&
         "Invalid arguments to merge checking");

#ifdef _DEBUG
  printf("Merging 0x%lx - 0x%lx and 0x%lx - 0x%lx to 0x%lx - 0x%lx\n",
         a->mem.low, a->mem.high, b->mem.low, b->mem.high,
         MIN(a->mem.low, b->mem.low), MAX(a->mem.high, b->mem.high));
#endif

  a->mem.low = MIN(a->mem.low, b->mem.low);
  a->mem.high = MAX(a->mem.high, b->mem.high);
  a->next = b->next;
  if(a->next) a->next->prev = a;
  else l->tail = a; // b was the tail
  node_free(b);
  l->size--;
  return a;
}

void list_insert(list_t *l, memory_span_t *mem)
{
  node_t *cur, *prev, *next, *n = node_create(mem);

  assert(l && mem && "Invalid arguments to list_insert()");
  assert(n && "Invalid pointer returned by node_create()");

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
      l->size++;
    }
    else if(!cur) // Last element in the list
    {
      cur = l->tail;
      cur->next = n;
      n->prev = cur;
      n->next = NULL;
      l->tail = n;
      l->size++;
    }
    else if(cur->mem.low == n->mem.low) // Overlapping, merge cur & n
    {
      cur->mem.high = MAX(cur->mem.high, n->mem.high);
      node_free(n);
      n = cur;
    }
    else // In the middle of the list
    {
      next = cur;
      cur = cur->prev;
      cur->next = n;
      n->prev = cur;
      next->prev = n;
      n->next = next;
      l->size++;
    }

    // Merge with neighbors
    prev = n->prev;
    while(prev && list_check_merge(prev, n))
    {
      n = list_merge(l, prev, n);
      prev = n->prev;
    }

    next = n->next;
    while(next && list_check_merge(n, next))
    {
      n = list_merge(l, n, next);
      next = n->next;
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

void list_atomic_start(list_t *l) { pthread_mutex_lock(&l->lock); }

void list_atomic_end(list_t *l) { pthread_mutex_unlock(&l->lock); }

