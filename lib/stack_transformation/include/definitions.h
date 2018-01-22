/*
 * Library-internal definitions & includes.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/25/2016
 */

#ifndef _DEFINITIONS_H
#define _DEFINITIONS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

//#include <libelf/libelf.h>
#include <my_libelf.h>
#include <ELF.h>

#include "config.h"
#include "retvals.h"
#include "bitmap.h"
#include "list.h"
#include "timer.h"
#include "regs.h"
#include "properties.h"
#include "call_site.h"

///////////////////////////////////////////////////////////////////////////////
// Checking, debugging & information macros
///////////////////////////////////////////////////////////////////////////////

#define STRINGIFY( x ) #x
#define STR( x ) STRINGIFY( x )

#ifdef _PER_LOG_OPEN
/*
 * Open & close files for each print call to work around limitations in
 * Popcorn's file I/O.
 */
#define OPEN_LOG_FILE	__log = fopen(LOG_FILE, "a")
#define CLOSE_LOG_FILE	fclose(__log)
#define CLOSE_GLOBAL_LOG_FILE
#else
#define OPEN_LOG_FILE
#define CLOSE_LOG_FILE
#define CLOSE_GLOBAL_LOG_FILE fclose(__log)
#endif

#ifdef _LOG
# define ST_ERR( code, str, ... ) \
  { \
    OPEN_LOG_FILE; \
    fprintf(stderr, "[" __FILE__ ":" STR(__LINE__) "] ERROR: " str, ##__VA_ARGS__); \
    fprintf(__log, "[" __FILE__ ":" STR(__LINE__) "] ERROR: " str, ##__VA_ARGS__); \
    CLOSE_LOG_FILE; \
    CLOSE_GLOBAL_LOG_FILE; \
    exit(code); \
  }
#else
# define ST_ERR( code, str, ... ) \
  { \
    fprintf(stderr, "[" __FILE__ ":" STR(__LINE__) "] ERROR: " str, ##__VA_ARGS__); \
    exit(code); \
  }
#endif

#ifdef _DEBUG

# ifdef _LOG

/* Log file descriptor.  Defined in src/init.c. */
extern FILE* __log;

#  define ST_RAW_INFO( str, ... ) \
  { \
    OPEN_LOG_FILE; \
    fprintf(__log, str, ##__VA_ARGS__); fflush(__log); \
    CLOSE_LOG_FILE; \
  }
#  define ST_INFO( str, ... ) \
  { \
    OPEN_LOG_FILE; \
    fprintf(__log, "[" __FILE__ ":" STR(__LINE__) "] " str, ##__VA_ARGS__); fflush(__log); \
    CLOSE_LOG_FILE; \
  }
#  define ST_WARN( str, ... ) \
  { \
    OPEN_LOG_FILE; \
    fprintf(__log, "[" __FILE__ ":" STR(__LINE__) "] WARNING: " str, ##__VA_ARGS__); fflush(__log); \
    CLOSE_LOG_FILE; \
  }

# else

#  define ST_RAW_INFO( str, ... ) printf(str, ##__VA_ARGS__)
#  define ST_INFO( str, ... ) \
      printf("[" __FILE__ ":" STR(__LINE__) "] " str, ##__VA_ARGS__)
#  define ST_WARN( str, ... ) \
      fprintf(stderr, "[" __FILE__ ":" STR(__LINE__) "] WARNING: " str, ##__VA_ARGS__)

# endif /* _LOG */

#else

# define ST_RAW_INFO( ... ) {}
# define ST_INFO( ... ) {}
# define ST_WARN( ... ) {}

#endif /* _DEBUG */

#ifdef _CHECKS

/* Assert that an expression is true, or print message & throw an error. */
# define ASSERT( expr, msg, ... ) if(!(expr)) ST_ERR(1, msg, ##__VA_ARGS__);

#else

# define ASSERT( expr, msg, ... ) {}

#endif /* _CHECKS */

///////////////////////////////////////////////////////////////////////////////
// Data access structures
///////////////////////////////////////////////////////////////////////////////

/* Get a value's size. */
#define VAL_SIZE( val ) (val->is_alloca ? val->alloca_size : val->size)

/*
 * A fixup record for reifying pointers to the stack when pointed-to data is
 * found.
 */
typedef struct fixup {
  void* src_addr; // pointed-to address on the source stack
  int act; // in which activation we must apply the fixup
  const live_value* dest_loc; // pointer to reify on destination stack
} fixup;

/* List of fixup records. */
define_list_type(fixup);

///////////////////////////////////////////////////////////////////////////////
// Rewriting metadata
///////////////////////////////////////////////////////////////////////////////

/* A call frame activation and unwinding information. */
typedef struct activation
{
  call_site site; /* call site information */
  void* cfa; /* canonical frame address */
  regset_t regs; /* register values */
  bitmap callee_saved; /* callee-saved registers stored in prologue */
} activation;

/*
 * Stack transformation handle, holds information required to do transform.
 * Instantiated once for each binary.
 */
struct _st_handle
{
  /////////////////////////////////////////////////////////////////////////////
  // Descriptors
  /////////////////////////////////////////////////////////////////////////////

  int fd; /* OS file descriptor */
  Elf* elf; /* libELF descriptor */

  /////////////////////////////////////////////////////////////////////////////
  // Binary & architecture information
  /////////////////////////////////////////////////////////////////////////////

  const char* fn; /* ELF file name */
  uint16_t arch; /* target architecture for the binary */
  uint16_t ptr_size; /* size of pointers on the architecture */

  regops_t regops; /* architecture-specific register access operations */
  properties_t props; /* architecture-specific stack properties */

  /////////////////////////////////////////////////////////////////////////////
  // Code/data/stack metadata
  /////////////////////////////////////////////////////////////////////////////

  /* Per-function unwinding record metadata */
  uint64_t unwind_addr_count;
  const unwind_addr* unwind_addrs;

  /* Register unwinding records */
  uint64_t unwind_count;
  const unwind_loc* unwind_locs;

  /* Call site records */
  uint64_t sites_count;
  const call_site* sites_id; /* sorted by ID */
  const call_site* sites_addr; /* sorted by return address */

  /* Call site live value records */
  uint64_t live_vals_count;
  const live_value* live_vals;

  /* Architecture-specific call site live value records */
  uint64_t arch_live_vals_count;
  const arch_live_value* arch_live_vals;
};

typedef struct _st_handle* st_handle;

/*
 * Stack rewriting context.  Used to hold current stack information for
 * rewriting.  Instantiated twice for each thread inside of rewriting functions
 * (one for each of source and destination stack.
 */
struct rewrite_context
{
  /* Binary/architecture-specific information */
  st_handle handle;

  /* Stack & register information, will contain transformation results. */
  void* stack_base; /* highest stack address */
  void* stack; /* top of stack (lowest stack address) */
  void* regs; /* register set, for copying in & out */

  /* Meta-data for stack activations. */
  int num_acts; /* number of activations */
  int act; /* current activation */
  activation acts[MAX_FRAMES]; /* all activations currently processed */
  list_t(fixup) stack_pointers; /* pointers to the stack, to be resolved */

  /* Pools for constant-time allocation of per-frame/runtime-dependent data */
  void* regset_pool; /* Register sets */
  STORAGE_TYPE* callee_saved_pool; /* Callee-saved registers (bitmaps) */
};

typedef struct rewrite_context* rewrite_context;

/* Macros to access activation information. */
#define ACT( ctx ) ctx->acts[ctx->act]
#define PREV_ACT( ctx ) ctx->acts[ctx->act - 1]
#define NEXT_ACT( ctx ) ctx->acts[ctx->act + 1]

/* Macros to access register set functions & properties. */
#define REGOPS( ctx ) ctx->handle->regops
#define PROPS( ctx ) ctx->handle->props

#endif /* _DEFINITIONS_H */

