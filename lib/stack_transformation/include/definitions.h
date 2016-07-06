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

#include <libelf/libelf.h>
#include <dwarf.h>
#include <libdwarf.h>

#include "config.h"
#include "retvals.h"
#include "bitmap.h"
#include "timer.h"
#include "regs.h"
#include "properties.h"
#include "call_site.h"

///////////////////////////////////////////////////////////////////////////////
// Checking, debugging & information macros
///////////////////////////////////////////////////////////////////////////////

#define STRINGIFY( x ) #x
#define STR( x ) STRINGIFY( x )

#ifdef _LOG
# define ST_ERR( code, str, ... ) \
  { \
    fprintf(stderr, "[" __FILE__ ":" STR(__LINE__) "] ERROR: " str, ##__VA_ARGS__); \
    fprintf(__log, "[" __FILE__ ":" STR(__LINE__) "] ERROR: " str, ##__VA_ARGS__); \
    fflush(__log); \
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

#  define ST_RAW_INFO( str, ... ) fprintf(__log, str, ##__VA_ARGS__)
#  define ST_INFO( str, ... ) \
      fprintf(__log, "[" __FILE__ ":" STR(__LINE__) "] " str, ##__VA_ARGS__)
#  define ST_WARN( str, ... ) \
      fprintf(__log, "[" __FILE__ ":" STR(__LINE__) "] WARNING: " str, ##__VA_ARGS__)

# else

#  define ST_RAW_INFO( str, ... ) printf(str, ##__VA_ARGS__)
#  define ST_INFO( str, ... ) \
      printf("[" __FILE__ ":" STR(__LINE__) "] " str, ##__VA_ARGS__)
#  define ST_WARN( str, ... ) \
      fprintf(stderr, "[" __FILE__ ":" STR(__LINE__) "] WARNING: " str, ##__VA_ARGS__)

# endif /* _LOG */

#else

# define ST_RAW_INFO( ... )
# define ST_INFO( ... )
# define ST_WARN( ... )

#endif /* _DEBUG */

#ifdef _CHECKS

/* Assert that an expression is true, or print message & throw an error. */
# define ASSERT( expr, msg, ... ) if(!(expr)) ST_ERR(1, msg, ##__VA_ARGS__);

/*
 * Check for DWARF errors.  Does *not* check for no entry return values, as
 * those are used to communicate information back to the library.  The user
 * can get the return value from the function:
 *
 *    int ret = DWARF_CHK(dwarf_func(...), "dwarf_func");
 */
# define DWARF_CHK( func, msg ) \
({ \
  int _ret = func; \
  if(_ret == DW_DLV_ERROR) \
    ST_ERR(DW_DLV_ERROR, "DWARF error in %s: %s\n", msg, dwarf_errmsg(err)); \
  _ret; \
})

/*
 * Check for DWARF errors.  Unlike DWARF_CHK, DWARF_OK ensures that not only
 * did the function succeed, but that it found an entry.  In other words, this
 * macro checks to ensure that the function returned DW_DLV_OK.
 */
# define DWARF_OK( func, msg ) \
({ \
  int _ret = func; \
  if(_ret != DW_DLV_OK) \
    ST_ERR(DW_DLV_ERROR, "DWARF error in %s: %s\n", msg, dwarf_errmsg(err)); \
  _ret; \
})

#else

# define ASSERT( expr, msg, ... ) {}
# define DWARF_CHK( func, msg ) func
# define DWARF_OK( func, msg ) func

#endif /* _CHECKS */

///////////////////////////////////////////////////////////////////////////////
// Data access structures
///////////////////////////////////////////////////////////////////////////////

#if _LIVE_VALS == DWARF_LIVE_VALS

/* Dwarf information for locating a variable within a stack frame. */
typedef struct variable
{
  Dwarf_Die die;
#ifdef _DEBUG
  char* name;
#endif
  Dwarf_Unsigned size;
  bool is_ptr;
  Dwarf_Signed num_locs;
  Dwarf_Locdesc** locs;
} variable;

#else /* STACKMAP_LIVE_VALS */

/* Live value location records are equivalent to variable information. */
typedef call_site_value variable;

#endif

/* Type of location where value is stored */
enum loc_type {
  ADDRESS = 0, // In memory
  REGISTER, // In register
  CONSTANT // Constant value
};

/* Where a value is located -- can be in memory, a register or is constant. */
typedef struct value_loc
{
  bool is_valid;
  uint8_t num_bytes; // TODO this should be a per-byte mask
  enum loc_type type;
  union
  {
    Dwarf_Unsigned addr;
    dwarf_reg reg;
    uint64_t val;
  };
} value_loc;

/*
 * A live value read from a context.  Contains either the contents of a
 * register or the memory location of the value.
 */
typedef struct value
{
  bool is_valid;
  bool is_addr;
  union
  {
    uint64_t val;
    void* addr;
  };
} value;

/* Function information, defined in func.c */
typedef struct func_info* func_info;

// TODO Here because list needs struct variable/struct value definitions
#include "list.h"

///////////////////////////////////////////////////////////////////////////////
// Rewriting metadata
///////////////////////////////////////////////////////////////////////////////

/* A call frame activation and unwinding information. */
typedef struct activation
{
  func_info function; /* current function information */
  void* cfa; /* canonical frame address */
  regset_t regs; /* register values */
  call_site site; /* call site information */
  bitmap callee_saved; /* callee-saved registers stored in prologue */
  Dwarf_Regtable3 rules; /* uwninding rules */
} activation;

/*
 * Stack transformation handle, holds information required to do transform.
 * Instantiated once for each binary.
 */
struct _st_handle
{
  /////////////////////////////////////////////////////////////////////////////
  // Descriptors and locks
  /////////////////////////////////////////////////////////////////////////////

  int fd; /* OS file descriptor */
  Elf* elf; /* libELF descriptor */
  Dwarf_Debug dbg; /* libDWARF descriptor */

  /////////////////////////////////////////////////////////////////////////////
  // Binary & architecture information
  /////////////////////////////////////////////////////////////////////////////

  const char* fn; /* file name */
  uint16_t arch; /* target architecture for the binary */
  uint16_t ptr_size; /* size of pointers on the architecture */
  Dwarf_Small version; /* DWARF frame information version */
  Dwarf_Unsigned code_align; /* DWARF frame code alignment */
  Dwarf_Signed data_align; /* DWARF frame data alignment */

  /////////////////////////////////////////////////////////////////////////////
  // Code/data/stack information
  /////////////////////////////////////////////////////////////////////////////

  /* Register & stack information */
  regset_t regops;
  properties_t props;

  /* Start functions */
  func_info start_main;
  func_info start_thread;

  /* Compilation unit & function lookup */
  Dwarf_Unsigned arange_count;
  Dwarf_Arange* aranges;

  /* Frame unwinding descriptions */
  Dwarf_Signed cie_count;
  Dwarf_Cie* cies;
  Dwarf_Signed fde_count;
  Dwarf_Fde* fdes;

  /* Frame unwinding descriptions from .eh_frame */
  Dwarf_Signed cie_count_eh;
  Dwarf_Cie* cies_eh;
  Dwarf_Signed fde_count_eh;
  Dwarf_Fde* fdes_eh;

  /* Call site lookup */
  uint64_t sites_count;
  const call_site* sites_id; /* sorted by ID */
  const call_site* sites_addr; /* sorted by return address */
#if _LIVE_VALS == STACKMAP_LIVE_VALS
  uint64_t live_vals_count;
  const call_site_value* live_vals; /* live value locations */
#endif
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
  void* stack; /* top of stack */
  void* regs;

  /* Meta-data for stack activations. */
  int num_acts; /* number of activations */
  int act; /* current activation */
  activation acts[MAX_FRAMES]; /* all activations currently processed */
  list_t(fixup) stack_pointers; /* stack pointers to be resolved */

  /* Pools for constant-time allocation of per-frame/runtime-dependent data */
  Dwarf_Regtable_Entry3* regtable_pool;
  STORAGE_TYPE* callee_saved_pool;
};

typedef struct rewrite_context* rewrite_context;

/* Macros to access activation information. */
#define ACT( ctx ) ctx->acts[ctx->act]
#define PREV_ACT( ctx ) ctx->acts[ctx->act - 1]
#define NEXT_ACT( ctx ) ctx->acts[ctx->act + 1]

#endif /* _DEFINITIONS_H */

