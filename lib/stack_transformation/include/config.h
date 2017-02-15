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
 * Default character buffer size.
 */
#define BUF_SIZE 512

/*
 * Names of ELF sections containing stack transformation unwind & call site
 * meta-data.
 */
#define SECTION_ST_UNWIND_ADDR SECTION_PREFIX "." SECTION_UNWIND_ADDR
#define SECTION_ST_UNWIND SECTION_PREFIX "." SECTION_UNWIND
#define SECTION_ST_ID SECTION_PREFIX "." SECTION_ID
#define SECTION_ST_ADDR SECTION_PREFIX "." SECTION_ADDR
#define SECTION_ST_LIVE SECTION_PREFIX "." SECTION_LIVE
#define SECTION_ST_ARCH_LIVE SECTION_PREFIX "." SECTION_ARCH

///////////////////////////////////////////////////////////////////////////////
// Userspace rewriting configuration
///////////////////////////////////////////////////////////////////////////////

/*
 * Environment variables specifying AArch64 & x86-64 binary names.
 */
#define ENV_AARCH64_BIN "ST_AARCH64_BIN"
#define ENV_X86_64_BIN "ST_X86_64_BIN"

/*
 * Stack limits -- Linux defaults to 8MB.
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

