/*
 * Stack transformation runtime configuration.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 10/29/2015
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include "het_bin.h"

/* Enable verbose debugging output, including information & warnings. */
//#define _DEBUG 1

/*
 * Log stack transformation output to file rather than stdout/stderr (requires
 * debugging be enabled).
 */
//#define _LOG 1
#define LOG_FILE "stack-transform.log"

/* Enable sanity checks, increases transformation overhead. */
//#define _CHECKS 1

/*
 * Enable timing of operations to determine hotspots.  Fine-grained timing
 * provides further details about individual operations (with more overhead).
 */
// Note: many functions use print statements in debugging, so in order to get
// more accurate timing information disable debugging information.
//#define _TIMING 1
//#define _FINE_GRAINED_TIMING 1

/*
 * Select the function used to measure time.  This may cause performance
 * differences depending on if the function uses a syscall or vDSO.
 */
#define CLOCK_GETTIME 0
#define GETTIMEOFDAY 1
#define _TIMER_SRC CLOCK_GETTIME // musl-libc has vDSO versions for both archs

/*
 * Select the type of metadata used to locate live values in stack frames.
 */
#define DWARF_LIVE_VALS 0
#define STACKMAP_LIVE_VALS 1
#define _LIVE_VALS STACKMAP_LIVE_VALS

/*
 * Enable function argument/variable querying optimization.  Does a targeted
 * search through the function's children for DW_TAG_formal_argument and
 * DW_TAG_variable DIEs rather than doing multiple generic searches.
 */
// Note: this only applies when using DWARF debugging information to locate
// live values, i.e. _LIVE_VALS == DWARF_LIVE_VALS
#define _FUNC_QUERY_OPT 1

/*
 * Select TLS implementation.  Popcorn compiler support for TLS is a little
 * iffy, so fall back to the pthreads implementation if necessary.
 */
#define COMPILER_TLS 0
#define PTHREAD_TLS 1
#define _TLS_IMPL PTHREAD_TLS

/*
 * Maximum number of frames that can be rewritten.
 */
#define MAX_FRAMES 512

/*
 * Maximum size of the DWARF expression evaluation stack.  Consistent w/ GCC.
 */
#define EXPR_STACK_SIZE 64

/*
 * Default character buffer size.
 */
#define BUF_SIZE 512

/*
 * Files containing definitions of starting functions for both the main and
 * forked threads.
 */
// Note: containing files must be specified because musl has differing
// declaration and definition prototypes for some functions, e.g.
// __libc_start_main().
#define START_MAIN_CU "src/env/__libc_start_main.c"
#define START_THREAD_CU "src/thread/pthread_create.c"

/*
 * Names of ELF sections containing stack transformation call site meta-data.
 */
#define SECTION_ST_ID SECTION_PREFIX "." SECTION_ID
#define SECTION_ST_ADDR SECTION_PREFIX "." SECTION_ADDR
#define SECTION_ST_LIVE SECTION_PREFIX "." SECTION_LIVE

///////////////////////////////////////////////////////////////////////////////
// Userspace rewriting configuration
///////////////////////////////////////////////////////////////////////////////

/*
 * Environment variables specifying AArch64 & x86-64 binary names.
 */
#define ENV_AARCH64_BIN "ST_AARCH64_BIN"
#define ENV_X86_64_BIN "ST_X86_64_BIN"

/*
 * Stack limits.
 */
#define MAX_STACK_SIZE (8 * 1024 * 1024)
#define B_STACK_OFFSET (4 * 1024 * 1024)

#endif /* _CONFIG_H */

///////////////////////////////////////////////////////////////////////////////
// Sane configuration checks
///////////////////////////////////////////////////////////////////////////////

#if defined(_LOG) && !defined(_DEBUG)
# error Must define _DEBUG to enable logging (_LOG)!
#endif

#if defined(_FINE_GRAINED_TIMING) && !defined(_TIMING)
# error Must define _TIMING to enable fine-grained timing (_FINE_GRAINED_TIMING)!
#endif

