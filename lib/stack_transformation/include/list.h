/*
 * A linked list for storing to-be-fixed pointers to stack variables.  The list
 * needs to only support forward traversal and doesn't require sorting.  The
 * list's implementation is generated via macros so that it can encapsulate
 * various types of data -- a C version of polymorphism.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/2/2016
 */
// Note: the type must be one word, e.g. 'int' and not 'struct my_struct'
// (typedef structs to avoid this issue).

#ifndef _LIST_H
#define _LIST_H

///////////////////////////////////////////////////////////////////////////////
// List definitions & declarations
///////////////////////////////////////////////////////////////////////////////

/* List types. */
#define node_t( type ) struct node_##type
#define list_t( type ) struct list_##type

/* A list node. */
#define _LIST_NODE( type ) \
node_t(type) { \
  node_t(type)* prev; \
  node_t(type)* next; \
  type data; \
};

/* A list object. */
#define _LIST( type ) \
list_t(type) { \
  size_t size; \
  node_t(type)* head; \
  node_t(type)* tail; \
};

///////////////////////////////////////////////////////////////////////////////
// List operations
///////////////////////////////////////////////////////////////////////////////

/*
 * Initialize a list.
 *
 * @param list a list object
 */
#define list_init( type, list ) list_init_##type(list)
#define _LIST_INIT( type ) \
static inline void list_init_##type(list_t(type)* list) \
{ \
  ASSERT(list, "invalid argument to list_init()\n"); \
  list->size = 0; \
  list->head = NULL; \
  list->tail = NULL; \
}

/*
 * Get the size of the list.
 *
 * @param list a list object
 * @return the number of nodes in the list
 */
#define list_size( type, list ) list_size_##type(list)
#define _LIST_SIZE( type ) \
static inline size_t list_size_##type(const list_t(type)* list) \
{ \
  ASSERT(list, "invalid argument to list_size()\n"); \
  return list->size; \
} \

/*
 * Get the first node in the list.
 *
 * @param list a list object
 * @param the first node in the list
 */
#define list_begin( type, list ) list_begin_##type(list)
#define _LIST_BEGIN( type ) \
static inline node_t(type)* list_begin_##type(list_t(type)* list) \
{ \
  ASSERT(list, "invalid argument to list_begin()\n"); \
  return list->head; \
}

/*
 * Get the last node in the list.
 *
 * @param list a list object
 * @return the last node in the list
 */
#define list_end( type, list ) list_end_##type(list)
#define _LIST_END( type ) \
static inline node_t(type)* list_end_##type(list_t(type)* list) \
{ \
  ASSERT(list, "invalid argument to list_end()\n"); \
  return list->tail; \
}

/*
 * Return the next node in the list.
 *
 * @param node a node in a list
 * @return the next node in the list, or NULL if this node is the last
 */
#define list_next( type, node ) list_next_##type(node)
#define _LIST_NEXT( type ) \
static inline node_t(type)* list_next_##type(const node_t(type)* node) \
{ \
  ASSERT(node, "invalid argument to list_next()\n"); \
  return node->next; \
}

/*
 * Return the previous node in the list.
 *
 * @param node a node in a list
 * @return the previous node in the list, or NULL if this node is the first
 */
#define list_prev( type, node ) list_prev_##type(node)
#define _LIST_PREV( type ) \
static inline node_t(type)* list_prev_##type(const node_t(type)* node) \
{ \
  ASSERT(node, "invalid argument to list_prev()\n"); \
  return node->prev; \
}

/*
 * Append a new node to the end of the list with the specified values.
 *
 * @param list a list object
 * @param data the data to be added to the list
 * @return node the newly created node
 */
#define list_add( type, list, data ) list_add_##type(list, data)
#define _LIST_ADD( type ) \
static inline node_t(type)* list_add_##type(list_t(type)* list, type data) \
{ \
  node_t(type)* node; \
  ASSERT(list, "invalid arguments to list_add()\n"); \
  node = (node_t(type)*)malloc(sizeof(node_t(type))); \
  node->data = data; \
  node->next = NULL; \
\
  if(list->head == NULL) /* List is empty */ \
  { \
    ASSERT(list->tail == NULL, "corrupted linked list\n"); \
    list->head = node; \
    list->tail = node; \
    node->prev = NULL; \
  } \
  else /* Append to end of list */ \
  { \
    ASSERT(list->tail, "corrupted linked list\n"); \
    list->tail->next = node; \
    node->prev = list->tail; \
    list->tail = node; \
  } \
\
  list->size++; \
  return node; \
}

/*
 * Remove a node from the list & return pointer to next node.
 *
 * @param list a list object
 * @param node a node within the list
 * @return the next node in the list, or NULL if this node is the last
 */
#define list_remove( type, list, node ) list_remove_##type(list, node)
#define _LIST_REMOVE( type ) \
static inline node_t(type)* list_remove_##type(list_t(type)* list, \
                                               node_t(type)* node) \
{ \
  node_t(type)* ret; \
  ASSERT(list && node, "invalid arguments to list_remove()\n"); \
  ASSERT(list->size > 0, "attempting to remove from empty list\n"); \
  if(list->size == 1) \
  { \
    ASSERT(node == list->head && node == list->tail, "corrupted linked list\n"); \
    list->head = NULL; \
    list->tail = NULL; \
  } \
  else if(node == list->head) \
  { \
    ASSERT(node->next, "corrupted linked list\n"); \
    list->head = node->next; \
    node->next->prev = NULL; \
  } \
  else if(node == list->tail) \
  { \
    ASSERT(node->prev, "corrupted linked list\n"); \
    list->tail = node->prev; \
    node->prev->next = NULL; \
  } \
  else /* In the middle of the list */ \
  { \
    ASSERT(node->prev && node->next, "corrupted linked list\n"); \
    node->prev->next = node->next; \
    node->next->prev = node->prev; \
  } \
  ret = node->next; \
  free(node); \
  list->size--; \
  return ret; \
}

/*
 * Remove all nodes from a list.
 *
 * @param list a list object
 */
#define list_clear( type, list ) list_clear_##type(list)
#define _LIST_CLEAR(type) \
static inline void list_clear_##type(list_t(type)* list) \
{ \
  ASSERT(list, "invalid argument to list_clear()\n"); \
  while(list->head) list_remove(type, list, list->head); \
}

///////////////////////////////////////////////////////////////////////////////
// List implementations
///////////////////////////////////////////////////////////////////////////////

/* Create a list definition containing data of type TYPE. */
#define define_list_type( type ) \
_LIST_NODE(type) \
_LIST(type) \
_LIST_INIT(type) \
_LIST_SIZE(type) \
_LIST_BEGIN(type) \
_LIST_END(type) \
_LIST_NEXT(type) \
_LIST_PREV(type) \
_LIST_ADD(type) \
_LIST_REMOVE(type) \
_LIST_CLEAR(type)

/* A fixup record. */
typedef struct fixup {
  void* src_addr;
  value dest_loc;
} fixup;

/* Variable location & values. */
typedef struct varval {
  const call_site_value* var;
  value val_src;
  value val_dest;
} varval;

/* Define list types. */
define_list_type(fixup);
define_list_type(varval);

#endif /*_LIST_H */

