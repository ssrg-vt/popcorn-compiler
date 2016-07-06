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

/******************************** Variables **********************************/

/*
 * Get the value of the specified variable from the current rewrite context.
 *
 * @param ctx a rewriting context
 * @param arg an variable whose value to read
 * @return a value for the variable in the current context
 */
value get_var_val(rewrite_context ctx, const variable* var);

/*
 * Get the location of the specified variable from the current rewrite context.
 *
 * @param ctx a rewriting context
 * @param arg an variable whose location to read
 * @return a location for the variable in the current context
 */
value_loc get_var_loc(rewrite_context ctx, const variable* var);

/*
 * Put the specified value at the location of the specified variable in the
 * rewrite context.  Return the location where the value was stored.
 *
 * @param ctx a rewriting context
 * @param var a variable for which to store a value
 * @param val a value to store in the variable's location
 * @return the location where the value was stored
 */
value_loc put_var_val(rewrite_context ctx, const variable* var, value val);

/************************** Values & Value Locations *************************/

/*
 * Put the specified value at the specified location & frame in the rewrite
 * context.  This is used internally by put_var_val and to fix-up stack
 * pointers once the pointed-to data has been discovered.
 *
 * @param ctx a rewriting context
 * @param var a value to store at a location
 * @param size the size of the data to store
 * @param loc a location at which to store the value
 * @param act the activation in which to insert the value
 */
void put_val_loc(rewrite_context ctx,
                 value val,
                 uint64_t size,
                 value_loc loc,
                 size_t act);

/************************ DWARF Location Description *************************/

/*
 * Get the value described by a location description.
 *
 * @param ctx a rewriting context
 * @param loc a location description
 * @return a value read using the location description
 */
value get_val_from_desc(rewrite_context ctx, const Dwarf_Locdesc* loc);

/*
 * Put the specified value in the rewrite context at the location specified by
 * the location description.
 *
 * @param ctx a rewriting context
 * @param loc a location description
 * @param size the size of the value
 * @param val the value to store in the rewrite context
 * @return the location where the value was stored
 */
value_loc put_val_from_desc(rewrite_context ctx,
                            const Dwarf_Locdesc* loc,
                            uint64_t size,
                            value val);

/********************************* Others ************************************/

/*
 * Set the return address in the outer-most stack frame.
 *
 * @param ctx a rewriting context
 * @param retaddr the return address
 */
void set_return_address(rewrite_context ctx, void* retaddr);

/*
 * Get the location in the current stack frame of the saved/old frame pointer
 * pushed in the function prolog.
 *
 * @param ctx a rewriting context
 * @return a pointer to the location of the saved frame pointer in the frame
 */
uint64_t* get_savedfbp_loc(rewrite_context ctx);

#endif /* _DATA_H */

