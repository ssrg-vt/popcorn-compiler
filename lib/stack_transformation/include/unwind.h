/*
 * APIs for unwinding/un-unwinding stack frame activations.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/26/2016
 */

#ifndef _UNWIND_H
#define _UNWIND_H

#include "definitions.h"

///////////////////////////////////////////////////////////////////////////////
// Stack unwinding
///////////////////////////////////////////////////////////////////////////////

/*
 * Set up the frame information, including callee-save bitfield and CFA.
 *
 * @param ctx a rewriting context
 */
void setup_frame_info(rewrite_context ctx);

/*
 * Set up the frame context.  This is a special case for setting up the frame
 * data before the function has set up the frame base pointer, i.e., directly
 * upon function entry.
 *
 * @param ctx a rewriting context
 */
void setup_frame_info_funcentry(rewrite_context ctx);

/*
 * Return whether or not the specified call site record corresponds to one of
 * the starting functions, either for the main or spawned threads.
 *
 * @param id a call site record ID
 * @return true if the record is for a starting function, false otherwise
 */
bool first_frame(uint64_t id);

/*
 * Unwind a call frame activation from the stack stored in the handle.
 *
 * @param ctx a rewriting context
 */
void pop_frame(rewrite_context ctx);

/*
 * Unwind a call frame activation from the stack stored in the handle.  This is
 * a special case for popping the frame before the function has set it up,
 * i.e., directly upon function entry.
 *
 * @param ctx a rewriting context
 */
void pop_frame_funcentry(rewrite_context ctx);

/*
 * Return the spill location for the specified register in the specified
 * activation's call frame.
 *
 * @param ctx a rewriting context
 * @param act an activation in which the register is saved
 * @param reg a saved register
 * @return the location of the saved register
 */
value get_register_save_loc(rewrite_context ctx,
                            activation* act,
                            uint16_t reg);

/*
 * Free the information describing the specified stack activation.
 *
 * @param handle a stack transformation handle
 * @param act a stack activation
 */
void free_activation(st_handle handle, activation* act);

#endif /* _UNWIND_H */

