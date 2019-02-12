/*
 * APIs for accessing frame-specific data, i.e. live values, return address,
 * and saved frame pointer location.
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
 * Copy a value from its location in the source context to its location in the
 * destination context.  This function implicitly uses the current stack frame
 * in the source & destination rewriting context.
 *
 * @param src the source rewriting context
 * @param src_val a live value's location in the source context
 * @param dest the destination rewriting context
 * @param dest_val a live value's location in the destination context
 * @param size the size of the data to copy between contexts
 */
void put_val(rewrite_context src,
             const live_value* src_val,
             rewrite_context dest,
             const live_value* dest_val);

/*
 * Put an architecture-specific constant value into a location.  This function
 * implicitly uses the current stack frame in the rewriting context.
 *
 * @param ctx the rewriting context
 * @param val the architecture-specific constant value
 */
void put_val_arch(rewrite_context ctx, const arch_live_value *val);

/*
 * Put data into a location.  Used for general-purpose touch-ups, such as
 * fixing-up pointers to the stack.
 *
 * @param ctx the rewriting context
 * @param val a live value's location in the context
 * @param act the activation in which the value is live
 * @param data the live value's new data
 */
void put_val_data(rewrite_context ctx,
                  const live_value* val,
                  int act,
                  uint64_t data);

/*
 * Return whether or not a pointer points to some location on the stack.  If
 * so, return the pointer's value.  If not, return NULL.
 *
 * @param ctx the rewriting context
 * @param val a values's metadata
 * @return the pointed-to address on the stack, or NULL if it is not a pointer
 *         to the stack
 */
void* points_to_stack(const rewrite_context ctx,
                      const live_value* val);

/*
 * Return whether or not a pointer refers to the specified live value in the
 * rewriting context.  If so, return the pointer's translated value for the
 * destination context.  If not, return NULL.
 *
 * @param src the source rewriting context
 * @param src_val the pointed-to live value on the source stack
 * @param dest the destination rewriting context
 * @param dest_val the pointed-to live value on the destination stack
 * @param src_ptr the source pointer which refers to the source stack
 * @return the translated pointer for the destination if src_ptr points to
 *         src_val, or NULL otherwise
 */
void* points_to_data(const rewrite_context src,
                     const live_value* src_val,
                     const rewrite_context dest,
                     const live_value* dest_val,
                     void* src_ptr);

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

#ifdef CHAMELEON

/*
 * Convert an originally-encoded offset from the fbp to its randomized offset
 * from the fbp.
 *
 * @param ctx a rewriting context
 * @param act activation in which address resides
 * @return the randomized offset
 */
int32_t translate_fbp_offset(rewrite_context ctx, int act, int32_t offset);

/*
 * Convert an originally-encoded offset from the sp to its randomized offset
 * from the sp.
 *
 * @param ctx a rewriting context
 * @param act activation in which address resides
 * @return the randomized offset
 */
int32_t translate_sp_offset(rewrite_context ctx, int act, int32_t offset);

/*
 * Convert an originally-encoded offset from a register to its randomized
 * offset from the register.
 *
 * @param ctx a rewriting context
 * @param act activation in which address resides
 * @param reg the base register
 * @return the randomized offset
 */
int32_t translate_offset_from_reg(rewrite_context ctx,
                                  int act,
                                  uint16_t reg,
                                  int32_t offset);

/*
 * Translate a stack address to the location in chameleon's buffers.
 *
 * @param ctx a rewriting context
 * @param addr a stack address from the child
 * @return a stack address in chameleon's buffer or NULL if it could not be
 *         translated
 */
void *child_to_chameleon(rewrite_context ctx, void *addr);

#endif

#endif /* _DATA_H */

