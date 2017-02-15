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
 * Get the value of the specified variable in the current stack frame from the
 * rewrite context.
 *
 * @param ctx a rewriting context
 * @param var a variable whose value to read
 * @return a value for the variable in the current frame of the context
 */
value get_var_val(rewrite_context ctx, const call_site_value* var);

/*
 * Copy a value from its location in the source context to its location in the
 * destination context.  This function implicitly uses the current stack frame
 * in the source & destination rewriting context.
 *
 * @param src the source rewriting context
 * @param src_val a value in the source context
 * @param dest the destination rewriting context
 * @param dest_val a value's location in the destination context
 * @param size the size of the data to copy between contexts
 */
void put_val(rewrite_context src,
             const value src_val,
             rewrite_context dest,
             value dest_val,
             uint64_t size);

/*
 * Put an architecture-specific constant value into a location.
 *
 * @param ctx the rewriting context
 * @param var the architecture-specific constant value
 */
void put_val_arch(rewrite_context ctx, const arch_const_value *val);

/*
 * Set the return address in the current stack frame of a rewriting context.
 *
 * @param ctx a rewriting context
 * @param retaddr the return address
 */
void set_return_address(rewrite_context ctx, void* retaddr);

/*
 * Set the return address in the current stack frame of a rewriting context.
 * This handles the case where the function has not yet set up the frame base
 * pointer, i.e., directly upon function entry.
 *
 * @param ctx a rewriting context
 * @param retaddr the return address
 */
void set_return_address_funcentry(rewrite_context ctx, void* retaddr);

/*
 * Get the location in the current stack frame of the saved/old frame pointer
 * pushed in the function prologue.
 *
 * @param ctx a rewriting context
 * @return a pointer to the location of the saved frame base pointer for the
 *         current frame
 */
uint64_t* get_savedfbp_loc(rewrite_context ctx);

#endif /* _DATA_H */

