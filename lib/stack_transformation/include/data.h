/*
 * APIs for accessing frame-specific data, i.e. arguments/local variables/live
 * values, return address, and saved frame pointer location.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/25/2016
 */

#ifndef _DATA_H
#define _DATA_H

#include "definitions.h"

///////////////////////////////////////////////////////////////////////////////
// Frame data access
///////////////////////////////////////////////////////////////////////////////

/*
 * Get the value of the specified variable from the current rewrite context.
 *
 * @param ctx a rewriting context
 * @param var a variable whose value to read
 * @return a value for the variable in the current frame of the context
 */
value get_var_val(rewrite_context ctx, const variable* var);

/*
 * Copy a value from its source location (in the source context) to its
 * destination location in the destination context.
 *
 * @param src the source rewriting context
 * @param src_val a value in the source context
 * @param dest the destination rewriting context
 * @param dest_val a value's location in the destination context
 * @param size the size of the data to copy between contexts
 */
void put_val(rewrite_context src,
             value src_val,
             rewrite_context dest,
             value dest_val,
             uint64_t size);

/*
 * Set the return address in the current stack frame of a rewriting context.
 *
 * @param ctx a rewriting context
 * @param retaddr the return address
 */
void set_return_address(rewrite_context ctx, void* retaddr);

/*
 * Get the location in the current stack frame of the saved/old frame pointer
 * pushed in the function prologue.
 *
 * @param ctx a rewriting context
 * @return a pointer to the location of the saved frame base pointer for the
 *         current frame
 */
uint64_t* get_savedfbp_loc(rewrite_context ctx);

/*
 * Get the value described by a location description.
 *
 * @param ctx a rewriting context
 * @param loc a location description
 * @return a value read using the location description
 */
value get_val_from_desc(rewrite_context ctx, const Dwarf_Locdesc* loc);

#endif /* _DATA_H */

