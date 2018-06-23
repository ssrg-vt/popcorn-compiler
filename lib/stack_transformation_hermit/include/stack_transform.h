/*
 * The public API for the stack transformation runtime.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 10/23/2015
 */

#ifndef _ST_H
#define _ST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Architecture-specific utilities for accessing registers */
#include <arch/aarch64/regs.h>
#include <arch/powerpc64/regs.h>
#include <arch/x86_64/regs.h>

#include "arch.h"

/* Handle containing per-binary rewriting information */
typedef struct _st_handle* st_handle;

/* Thread stack bounds */
typedef struct stack_bounds {
  void* high;
  void* low;
} stack_bounds;

//void *src_act_cfa;
///////////////////////////////////////////////////////////////////////////////
// Initialization & teardown
///////////////////////////////////////////////////////////////////////////////

/* 
 * Open the ELF file named by FN & prep it to be used for rewriting.  The
 * handle returned from the function will be used in stack transformation
 * functions to provide information about the specified binary.
 *
 * @param fn the filename for the ELF file
 * @return a stack transformation handle on success, or NULL otherwise
 */
st_handle st_init(const char* fn);

/*
 * Clean up and free a stack transformation handle.  Releases ELF information.
 *
 * @param handle a stack transformation handle
 */
void st_destroy(st_handle handle);

///////////////////////////////////////////////////////////////////////////////
// Performing stack transformation
///////////////////////////////////////////////////////////////////////////////

/*
 * Rewrite the stack from user-space.
 *
 * Note: specific to Popcorn Compiler/the migration wrapper.
 *
 * @param sp the current stack pointer
 * @param src_arch the source ISA
 * @param src_regs the current register set
 * @param dest_arch the destination ISA
 * @param dest_regs the transformed destination register set
 * @return 0 if the stack was successfully re-written, 1 otherwise
 */
int st_userspace_rewrite(void* sp,
                         enum arch src_arch,
                         void* src_regs,
                         enum arch dest_arch,
                         void* dest_regs);

/*
 * Rewrite the stack in its entirety from its current form (source) to the
 * requested form (destination).
 *
 * @param src a stack transformation handle which has transformation metadata
 *            for the source binary
 * @param regset_src a pointer to a filled register set representing the
 *                   thread's state
 * @param sp_base_src source stack base, i.e., highest stack address
 * @param dest a stack transformation handle which has transformation metadata
 *             for the destination binary
 * @param regset_dest a pointer to a register set to be filled with destination
 *                    thread's state
 * @param sp_base_dest destination stack base, i.e., highest stack address
 *                     (will fill downwards with activation records)
 * @return 0 if succesful, or 1 otherwise
 */
int st_rewrite_stack(st_handle src,
                     void* regset_src,
                     void* sp_base_src,
                     st_handle dest,
                     void* regset_dest,
                     void* sp_base_dest);

/*
 * Rewrite only the top frame of the stack.  Previous frames will be
 * re-written on-demand as the thread unwinds the call stack.
 *
 * @param src a stack transformation handle which has transformation metadata
 *            for the source binary
 * @param regset_src a pointer to a filled register set representing the
 *                   thread's state
 * @param sp_base_src source stack base, i.e., highest stack address
 * @param dest a stack transformation handle which has transformation metadata
 *             for the destination binary
 * @param regset_dest a pointer to a register set to be filled with destination
 *                    thread's state
 * @param sp_base_dest destination stack base, i.e., highest stack address
 *                     (will fill downwards with activation records)
 * @return 0 if succesful, or 1 otherwise
 */
// TODO not yet implemented
int st_rewrite_ondemand(st_handle src,
                        void* regset_src,
                        void* sp_base_src,
                        st_handle dest,
                        void* regset_dest,
                        void* sp_base_dest);

/*
 * Return the current thread's stack bounds.
 *
 * @return this thread's stack bounds information
 */
stack_bounds get_stack_bounds();

#ifdef __cplusplus
}
#endif

#endif /* _ST_H */

