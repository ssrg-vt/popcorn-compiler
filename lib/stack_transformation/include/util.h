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
regops_t get_regops(uint16_t arch);

/*
 * Get stack properties for the specified architecture.
 *
 * @param arch an architecture number
 * @return a properties struct
 */
properties_t get_properties(uint16_t arch);

/*
 * Return the section NAME contained in ELF E.
 *
 * @param e an ELF descriptor
 * @param sec the name of the section
 * @return the ELF section descriptor for NAME, or NULL if not found
 */
Elf_Scn* get_section(Elf* e, const char* sec);

/*
 * Returns the number of entries encoded in section SEC_NAME in ELF E.
 *
 * @param e an ELF descriptor
 * @param sec name of the ELF section
 * @return the number of entries, or -1 if an error occurred
 */
int64_t get_num_entries(Elf* e, const char* sec);

int64_t my_get_num_entries(Elf* e, const char* sec);
/*
 * Return the section data encoded in section SEC_NAME in ELF E.
 *
 * @param e an ELF descriptor
 * @param sec name of the ELF section
 * @return a pointer to data in the section, or NULL if an error occurred
 */
const void* get_section_data(Elf* e, const char* sec);

const void* my_get_section_data(Elf* e, const char* sec);

/*
 * Return the call site information for the specified return address.
 *
 * @param handle a stack transformation handle
 * @param ret_addr return address defining the call site
 * @param site pointer to call_site structure to populate with information
 * @return true if the site was found, false otherwise
 */
bool get_site_by_addr(st_handle handle, void* ret_addr, call_site* site);

/*
 * Return the call site information for the specified call site ID.
 *
 * @param handle a stack transformation handle
 * @param csid a call site id
 * @param site pointer to call_site structure to populate with information
 * @return true if the site was found, false otherwise
 */
bool get_site_by_id(st_handle handle, uint64_t csid, call_site* site);

/*
 * Return the address of the function containing the specified program
 * location.  This is used to bootstrap in the outer frame, where we have an
 * exact program location for the source but need to restart the function on
 * the destination.
 *
 * @param handle a stack transformation handle
 * @param pc a program location
 * @return the address of the function containing the address, or NULL if it
 *         could not be found
 */
void* get_function_address(st_handle handle, void* pc);

/*
 * Return the function unwinding metadata (offset into the unwinding
 * information section & number of unwinding records) for the function
 * enclosing the specified address.  This is used as a fallback when the PC is
 * not at a call site, i.e., when bootstrapping in the outermost frame.
 *
 * @param handle a stack transformation handle
 * @param addr a program counter address
 * @param metadata metadata describing unwinding information for the function
 * @return true if the enclosing function was found, false otherwise
 */
bool get_unwind_offset_by_addr(st_handle handle,
                               void* addr,
                               unwind_addr* meta);

#endif /* _UTIL_H */

