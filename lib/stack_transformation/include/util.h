/*
 * Utility definitions and functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 10/23/2015
 */

#ifndef _UTIL_H
#define _UTIL_H

#include "properties.h"
#include "definitions.h"

///////////////////////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////////////////////

/*
 * Return the name of the specified architecture, defined by <elf.h>.
 *
 * @param arch an architecture number
 * @return an architecture name
 */
const char* arch_name(uint16_t arch);


/*
 * Get register operations for the specified architecture.
 *
 * @param arch an architecture number
 * @return a register operations struct
 */
regset_t get_regops(uint16_t arch);

/*
 * Get stack properties for the specified architecture.
 *
 * @param arch an architecture number
 * @return a properties struct
 */
properties_t get_properties(uint16_t arch);

/*
 * Print the DIE's source code name, if available.
 *
 * @param handle a stack transformation handle
 * @param die a DIE
 */
void print_die_name(st_handle handle, Dwarf_Die die);

/*
 * Print the DIE's tag.  Since there is no dynamic allocation/freeing, we don't
 * need the stack transformation handle.
 *
 * @param die a DIE
 */
void print_die_type(Dwarf_Die die);

/*
 * Print all attributes available for a DIE.
 *
 * @param handle a stack transformation handle
 * @param die a DIE
 */
void print_die_attrs(st_handle handle, Dwarf_Die die);

/*
 * Decode an unsigned little-endian base-128 (LEB128) number.
 *
 * @param raw pointer to byte buffer containing a raw LEB128 value
 * @return an unsigned 64-bit integer
 */
Dwarf_Unsigned decode_leb128u(Dwarf_Unsigned raw);

/*
 * Decode a signed little-endian base-128 (LEB128) number.
 *
 * @param raw pointer to byte buffer containing a raw LEB128 value
 * @return a signed 64-bit integer
 */
Dwarf_Signed decode_leb128s(Dwarf_Unsigned raw);

#endif /* _UTIL_H */

