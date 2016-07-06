/*
 * APIs for reading, querying, and freeing function-specific information.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/26/2016
 */

#ifndef _FUNC_H
#define _FUNC_H

#include "definitions.h"

///////////////////////////////////////////////////////////////////////////////
// Function handling
///////////////////////////////////////////////////////////////////////////////

/*
 * Read in information about the function encapsulating PC.  In particular,
 * read in location description information for the function's formal arguments
 * and local variables.
 *
 * @param handle a stack transformation handle
 * @param pc a program counter
 * @return a handle for querying function information
 */
func_info get_func_by_pc(st_handle handle, void* pc);

/*
 * Read in information about function FUNC_NAME.  In particular, read in
 * location description information for the function's formal arguments and
 * local variables.
 *
 * @param handle a stack transformation handle
 * @param cu the compilation unit in which to search for the function
 * @param func name of the function
 * @return a handle for querying function information
 */
func_info get_func_by_name(st_handle handle,
                           const char* cu,
                           const char* func);

/*
 * Free a function information handle.
 *
 * @param handle a stack transformation handle
 * @param info a function information handle
 */
void free_func_info(st_handle handle, func_info info);

/*
 * Check if the specified PC is located within the function.
 *
 * @param handle a function information handle
 * @param pc a program counter
 * @return true if the two functions are equal, false otherwise
 */
bool is_func(func_info handle, void* pc);

/*
 * Return the function's name.
 *
 * @param handle a function information handle
 * @return the function's name
 */
const char* get_func_name(func_info handle);

/*
 * Return the starting address of the function's code.
 *
 * @param handle a function information handle
 * @return the function's starting address
 */
void* get_func_start_addr(func_info handle);

#if _LIVE_VALS == DWARF_LIVE_VALS

/*
 * Return the function's frame base location description (should only be one
 * according to libDWARF documentation).
 *
 * @param handle a function information handle
 * @return the function's frame base location description
 */
const Dwarf_Locdesc* get_func_fb(func_info handle);

/*
 * Return the number of formal arguments for a function.
 *
 * @param handle a function information handle
 * @return the number of formal arguments to the function
 */
size_t num_args(func_info handle);

/*
 * Search for a formal argument named NAME.
 *
 * @param handle a function information handle
 * @param name the name of the argument
 * @return a pointer to an argument if found, or NULL otherwise
 */
const variable* get_arg_by_name(func_info handle, const char* name);

/*
 * Query a formal argument by location.
 *
 * @param handle a function information handle
 * @param pos the position of the argument in the argument list
 * @return a pointer to an argument if POS is valid, or NULL otherwise
 */
const variable* get_arg_by_pos(func_info handle, size_t pos);

/*
 * Return the number of local variables for a function.
 *
 * @param handle a function information handle
 * @return the number of local variables to the function
 */
size_t num_vars(func_info handle);

/*
 * Search for a local variable named NAME.
 *
 * @param handle a function information handle
 * @param name the name of the variable
 * @return a pointer to an variable if found, or NULL otherwise
 */
const variable* get_var_by_name(func_info handle, const char* name);

/*
 * Query a local variable by location.
 *
 * @param handle a function information handle
 * @param pos the position of the local variable in the DWARF variable list
 * @return a pointer to a variable if POS is valid, or NULL otherwise
 */
const variable* get_var_by_pos(func_info handle, size_t pos);

#endif /* DWARF_LIVE_VALS */

#endif /* _FUNC_H */

