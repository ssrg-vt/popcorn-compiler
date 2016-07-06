/*
 * APIs for querying information about the binary.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/25/2016
 */

#ifndef _QUERY_H
#define _QUERY_H

#include "definitions.h"

///////////////////////////////////////////////////////////////////////////////
// Query operations
///////////////////////////////////////////////////////////////////////////////

/*
 * Get the compilation unit DIE for code pointer PC.
 *
 * @param handle a stack transformation handle
 * @param pc a program counter
 * @return a DIE for the compilation unit encapsulating PC, or NULL otherwise
 */
Dwarf_Die get_cu_die(st_handle handle, void* pc);

/*
 * Get the function DIE for code pointer PC.  Optionally get the compilation
 * unit CU's DIE (where the function is).
 *
 * @param handle a stack transformation handle
 * @param pc a program counter
 * @param func a pointer to the function's DIE
 * @param cu a poitner to the CU's DIE (optionally NULL)
 * @return true if the function's DIE was populated, or false otherwise
 */
bool get_func_die(st_handle handle, void* pc, Dwarf_Die* cu, Dwarf_Die* func);

/*
 * Get the function DIE for function FUNC in compilation unit CU.
 *
 * @param handle a stack transformation handle
 * @param cu a compilation unit name
 * @param func a function name
 * @param func_die a pointer to the function's DIE
 * @param cu_die a pointer to the CU's DIE (optionally NULL)
 * @return true if the function's DIE was populated, or false otherwise
 */
bool get_named_func_die(st_handle handle,
                        const char* cu,
                        const char* func,
                        Dwarf_Die* cu_die,
                        Dwarf_Die* func_die);

/*
 * Get the frame description entry (FDE) and common information entry (CIE) for
 * code pointer PC.
 *
 * @param handle a stack transformation handle
 * @param pc a program counter
 * @param fde a pointer to storage for the FDE
 * @param cie a pointer to storage for the CIE
 */
void get_fde_cie(st_handle handle, void* pc, Dwarf_Fde* fde, Dwarf_Cie* cie);

/*
 * Return the number of direct children of DIE who are of type TAG.
 *
 * @param handle a stack transformation handle
 * @param die a DIE whose children to query
 * @param tag the child type to match
 * @return the number of direct children of type TAG
 */
size_t get_num_children(st_handle handle, Dwarf_Die die, Dwarf_Half tag);

/*
 * Return the direct children of DIE who are of type TAG.
 *
 * @param handle a stack transformation handle
 * @param die a DIE whose children to query
 * @param tag the child type to match
 * @param children a pointer to where to store a dynamically-allocated array
 *                 of matched children
 * @return the number of matched children found
 */
size_t get_children(st_handle handle,
                    Dwarf_Die die,
                    Dwarf_Half tag,
                    Dwarf_Die** children);

#if _LIVE_VALS == DWARF_LIVE_VALS

/*
 * An optimized method for reading a function's arguments and local variables
 * that depends on how the DIEs are layed out in the binary.
 *
 * @param handle a stack transformation handle
 * @param die a function DIE for which to query argument & variable information
 * @param num_args a pointer to an integer to set with the number of arguments
 * @param args a pointer which will be set to an array w/ argument DIEs (if any)
 * @param num_vars a pointer to an integer to set with the number of variables
 * @param vars a pointer which will be set to an array w/ variable DIEs (if any)
 */
bool get_args_locals(st_handle handle,
                     Dwarf_Die func_die,
                     size_t* num_args,
                     variable** args,
                     size_t* num_vars,
                     variable** vars);

/*
 * Returns the size of the datum represented by DIE.
 *
 * @param handle a stack transformation handle
 * @param die a DIE
 * @param is_ptr is the retrieved datum a pointer?
 * @return the size, in bytes, of the datum
 */
Dwarf_Unsigned get_datum_size(st_handle handle, Dwarf_Die die, bool* is_ptr);

#endif /* DWARF_LIVE_VALS */

/*
 * Search through a list of location descriptions and return the one that
 * applies to PC.
 *
 * @param num_locs number of location descriptions in the list
 * @param locs list of location descriptions
 * @param pc a PC for which to search for a location description
 * @return a pointer to a location description if one is found, or NULL
 *         otherwise
 */
Dwarf_Locdesc* get_loc_desc(Dwarf_Signed num_locs,
                            Dwarf_Locdesc** locs,
                            void* pc);

/*
 * Return the section NAME contained in ELF E.
 *
 * @param e an ELF descriptor
 * @param name the name of the section
 * @return the ELF section descriptor for NAME, or NULL if not found
 */
Elf_Scn* get_section(Elf* e, const char* name);

/*
 * Returns the number of entries encoded in section SEC_NAME in ELF E.
 *
 * @param e an ELF descriptor
 * @param sec_name name of the ELF section
 * @return the number of entries, or -1 if an error occurred
 */
int64_t get_num_entries(Elf* e, const char* sec_name);

/*
 * Return the call site entries encoded in section SEC_NAME in ELF E.
 *
 * @param e an ELF descriptor
 * @param sec_name name of the ELF section containing call site metadata
 * @return a pointer to the call site entries, or NULL if an error occurred
 */
const call_site* get_call_sites(Elf* e, const char* sec_name);

#if _LIVE_VALS == STACKMAP_LIVE_VALS
/*
 * Return the call site live value location records encoded in section SEC_NAME
 * in ELF E.
 *
 * @param e an ELF descriptor
 * @param sec_name name of the ELF section containing call site live value
 *                 metadata
 * @return a pointer to the call site entries, or NULL if an error occurred
 */
const call_site_value* get_call_site_values(Elf* e, const char* sec_name);
#endif

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

#endif /* _QUERY_H */

