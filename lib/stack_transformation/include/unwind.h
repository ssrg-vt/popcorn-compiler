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
 * Return whether or not the specified call site record corresponds to one of
 * the starting functions, either for the main or spawned threads.
 *
 * @param id a call site record ID
 * @return true if the record is for a starting function, false otherwise
 */
bool first_frame(uint64_t id);

/*
 * Boot strap the outermost frame's information.  Only needed during
 * initialization as pop_frame performs the same functionality during
 * unwinding.
 *
 * @param ctx a rewriting context
 */
void bootstrap_frame(rewrite_context ctx);

/*
 * Boot strap the outermost frame's information.  This is a special case for
 * setting up information before the function has set up the frame, i.e.,
 * directly upon function entry.  Only needed during initialization as
 * pop_frame_funcentry performs the same functionality during unwinding.
 *
 * @param ctx a rewriting context
 */
void bootstrap_frame_funcentry(rewrite_context ctx);

/*
 * Unwind the current call frame activation from the stack stored in the
 * context.  Sets up the new frame's stack pointer, frame base pointer,
 * canonical frame address, and restores the callee-saved registers.
 *
 * @param ctx a rewriting context
 */
void pop_frame(rewrite_context ctx);

/*
 * Unwind the current call frame activation from the stack stored in the
 * context.  This is a special case for popping the frame before the function
 * has set it up, i.e., directly upon function entry.  Sets up the new frame's
 * stack pointer, frame base pointer & canonical frame address.
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
 * @return the memory of the saved register
 */
void* get_register_save_loc(rewrite_context ctx,
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

