/*
 * APIs for unwinding/un-unwinding stack frame activations.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/26/2016
 */

#ifndef _UNWIND_H
#define _UNWIND_H

#include "definitions.h"
#include "data.h"

///////////////////////////////////////////////////////////////////////////////
// Stack unwinding
///////////////////////////////////////////////////////////////////////////////

/*
 * Initialize libDWARF-internal vairables to prepare it for unwinding.
 *
 * @param handle a stack transformation handle
 */
void init_unwinding(st_handle handle);

/*
 * Read in the unwinding rules for the current frame in the re-write context.
 *
 * @param ctx a rewriting context
 */
void read_unwind_rules(rewrite_context ctx);

/*
 * Returns function information for the function if the current stack frame is
 * the first frame of the stack, i.e. the entry function of the thread.
 *
 * @param handle a stack transformation handle
 * @param pc a program counter
 * @return the first function's information handle if PC is in the first frame,
 *         or NULL otherwise
 */
func_info first_frame(st_handle handle, void* pc);

/*
 * Unwind a call frame activation from the stack stored in the handle.
 *
 * @param ctx a rewriting context
 */
void pop_frame(rewrite_context ctx);

/*
 * Process the unwind rule to get the saved storage location for the register
 * (or the constant value).
 *
 * @param ctx a rewriting context
 * @param rule an unwind rule
 * @param is_cfa is the rule for the CFA?
 * @return the location of the saved register
 */
value get_stored_loc(rewrite_context ctx,
                     activation* act,
                     Dwarf_Regtable_Entry3* rule,
                     bool is_cfa);

/*
 * Free the information describing the specified stack activation.
 *
 * @param handle a stack transformation handle
 * @param act a stack activation
 */
void free_activation(st_handle handle, activation* act);

#endif /* _UNWIND_H */

