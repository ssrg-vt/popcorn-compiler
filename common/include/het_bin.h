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

/* Section name postfix for function metadata information. */
#define SECTION_FUNCTIONS "functions"

/* Section name postfix for stack slot metadata. */
#define SECTION_STACK_SLOTS "stack_slots"

/* Section name postfix for unwinding information. */
#define SECTION_UNWIND "unwind"

/*
 * Section name postfix for call site sections -- sorted by ID & address,
 * respectively.
 */
#define SECTION_ID "id"
#define SECTION_ADDR "addr"

/* Live variable location record section postfix. */
#define SECTION_LIVE "live"

/* Architecture-specific constant locations & values. */
#define SECTION_ARCH "arch_const"

#endif /* _HET_BIN_H */

