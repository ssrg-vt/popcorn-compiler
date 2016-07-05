/**
 * Configuration for heterogeneous binaries.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2016
 */

#ifndef _HET_BIN_H
#define _HET_BIN_H

/* Section name prefix for all call site metadata. */
#define SECTION_PREFIX ".stack_transform"

/*
 * Section name postfix for call site sections -- sorted by ID & address,
 * respectively.
 */
#define SECTION_ID "id"
#define SECTION_ADDR "addr"

/* Live variable location record section postfix. */
#define SECTION_LIVE "live"

/* Names of starting functions for the main thread & forked threads. */
// Note: these are specific to musl-libc
#define START_MAIN "__libc_start_main"
#define START_THREAD "start"

#endif /* _HET_BIN_H */

