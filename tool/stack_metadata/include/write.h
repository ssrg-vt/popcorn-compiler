/**
 * Functions for writing information to ELF binaries.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/8/2016
 */

#ifndef _WRITE_H
#define _WRITE_H

#include "definitions.h"
#include "bin.h"
#include "stackmap.h"

/**
 * Update the offsets in the function metadata records and sort by function
 * address.
 * @param b a binary descriptor
 * @param sec prefix of sections to be added
 * @return 0 if the section was sorted, an error code otherwise
 */
ret_t update_function_records(bin *b, const char *sec);

/**
 * Add stack transformation metadata to the object.
 * @param b a binary descriptor
 * @param sm stack map data for the descriptor
 * @param num_sm number of stack maps pointed to by sm
 * @param sec prefix of sections to be added
 * @param start_id beginning call site ID
 * @return 0 if the sections were added, an error code otherwise
 */
ret_t add_sections(bin *b,
                   stack_map_section *sm,
                   size_t num_sm,
                   const char *sec,
                   uint64_t start_id);

#endif /* _WRITE_H */

