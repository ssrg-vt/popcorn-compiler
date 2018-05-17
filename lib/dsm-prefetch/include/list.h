/*
 * List APIs & definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu
 * Date: February 13th, 2018
 */

#ifndef _LIST_H
#define _LIST_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "definitions.h"

/* An opaque node type. */
typedef struct node_t node_t;

/* An opaque cache entry type. */
typedef struct node_cache_t node_cache_t;

/* A sorted linked list. */
typedef struct {
  node_cache_t *cache;
  node_t *head, *tail;
  size_t size;
  int nid;
  pthread_mutex_t lock;
} list_t;

/*
 * Initialize a list.
 *
 * Note: *not* thread safe!
 *
 * @param l a list to be initialized
 * @param nid a node ID, passed to node-aware memory allocator
 */
void list_init(list_t *l, int nid);

/*
 * Return a list's size.
 * @param l a list
 * @return the number of nodes in the list
 */
size_t list_size(const list_t *l);

/*
 * Insert a memory region into the list, merging with other spans as needed.
 *
 * @param l a list
 * @param mem a contiguous memory region to be inserted into the list
 */
void list_insert(list_t *l, const memory_span_t *mem);

/*
 * Return true if the list has a memory region that overlaps a span, or false
 * otherwise.
 *
 * @param l a list
 * @param mem a contiguous memory region
 */
bool list_overlaps(list_t *l, const memory_span_t *mem);

/*
 * Remove any memory regions from the list that overlap with a span.
 *
 * @param l a list
 * @param mem a contiguous memory region to be removed from the list
 */
void list_remove(list_t *l, const memory_span_t *mem);

/*
 * Clear the list, freeing all nodes.
 *
 * @param l a list
 */
void list_clear(list_t *l);

/*
 * Access the list in an atomic fashion in order to let a thread batch together
 * multiple operations.
 *
 * @param l a list
 */
void list_atomic_start(list_t *l);
void list_atomic_end(list_t *l);

/*
 * List iterators.  Must be used in conjunction with list_atomic_* to ensure
 * thread safety.
 *
 * Note: *not* thread safe!
 *
 * @param l a list
 * @param n an opaque node iterator
 * @return an opaque node iterator
 */
const node_t *list_begin(list_t *l);
const node_t *list_next(const node_t *n);
const node_t *list_end(list_t *l);

/*
 * Retrieve the memory region contained in a node.
 *
 * @param n an opaque node iterator
 * @return a memory region pointer
 */
const memory_span_t *list_get_span(const node_t *n);

/*
 * Print the contents of the list.
 *
 * @param l a list
 */
void list_print(list_t *l);

#endif

