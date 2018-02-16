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

/* A linked list node. */
typedef struct node_t {
  struct node_t *prev, *next;
  memory_span_t mem;
} node_t;

/* A sorted linked list. */
typedef struct {
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
 */
void list_init(list_t *l, int nid);

/*
 * Return a list's size.
 * @param l a list
 * @return the number of nodes in the list
 */
size_t list_size(const list_t *l);

/*
 * Insert a memory region into the list, merging with other overlapping memory
 * regions as needed.
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
 * Print the contents of the list.
 *
 * @param l a list
 */
void list_print(list_t *l);

#endif

