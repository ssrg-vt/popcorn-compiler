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
#include <arch/x86_64/regs.h>

/* Handle containing per-binary rewriting information */
typedef struct _st_handle* st_handle;

/* Thread stack bounds */
typedef struct stack_bounds {
  void* high;
  void* low;
} stack_bounds;

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
 * Clean up and free a stack transformation handle.  Releases DWARF and ELF
 * information.
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
 * Note: internally provides synchronization, so is thread-safe.
 * Note: specific to Popcorn Compiler/the migration wrapper.
 *
 * @param sp the current stack pointer
 * @param regs the current register set
 * @param dest_regs the transformed destination register set
 * @return 0 if the stack was successfully re-written, 1 otherwise
 */
int st_userspace_rewrite(void* sp, void* regs, void* dest_regs);

/*
 * Rewrite the stack from user-space (aarch64 -> aarch64).  Useful for
 * debugging homogeneously.
 *
 * Note: internally provides synchronization, so is thread-safe.
 * Note: specific to Popcorn Compiler/the migration wrapper.
 *
 * @param sp the current stack pointer
 * @param regs the current register set
 * @param dest_regs the transformed destination register set.
 * @return 0 if the stack was successfully re-written, 1 otherwise
 */
int st_userspace_rewrite_aarch64(void* sp,
                                 struct regset_aarch64* regs,
                                 struct regset_aarch64* dest_regs);

/*
 * Rewrite the stack from user-space (x86_64 -> x86_64).  Useful for debugging
 * homogeneously.
 *
 * Note: internally provides synchronization, so is thread-safe.
 * Note: specific to Popcorn Compiler/the migration wrapper.
 *
 * @param sp the current stack pointer
 * @param regs the current register set
 * @param dest_regs the transformed destination register set.
 * @return 0 if the stack was successfully re-written, 1 otherwise
 */
int st_userspace_rewrite_x86_64(void* sp,
                                struct regset_x86_64* regs,
                                struct regset_x86_64* dest_regs);

/*
 * Rewrite the stack in its entirety from its current form (source) to the
 * requested destination form (destination).
 *
 * Note: handles cannot be accessed concurrently, not thread safe!
 *
 * @param src a stack transformation handle which has debugging information for
 *            the source binary
 * @param regset_src a pointer to a filled register set representing the
 *                   thread's state
 * @param sp_base_src source stack base
 * @param dest a stack transformation handle which has debugging information
 *             for the destination binary
 * @param regset_dest a pointer to a register set to be filled with destination
 *                    thread's state
 * @param sp_base_dest destination stack base (will fill downwards with
 *                     activation records)
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
 * Note: handles cannot be accessed concurrently, not thread safe!
 *
 * @param src a stack transformation handle which has debugging information for
 *            the source binary
 * @param regset_src a pointer to a filled register set representing the
 *                   thread's state
 * @param sp_base_src source stack base
 * @param dest a stack transformation handle which has debugging information
 *             for the destination binary
 * @param regset_dest a pointer to a register set to be filled with destination
 *                    thread's state
 * @param sp_base_dest destination stack base (will fill downwards with
 *                     activation records)
 * @return 0 if succesful, or 1 otherwise
 */
int st_rewrite_ondemand(st_handle src,
                        void* regset_src,
                        void* sp_base_src,
                        st_handle dest,
                        void* regset_dest,
                        void* sp_base_dest);

///////////////////////////////////////////////////////////////////////////////
// Information retrieval
///////////////////////////////////////////////////////////////////////////////

/*
 * Get the encapsulating compilation unit for the specified instruction
 * pointer.  In other words, get the name of the file containing the code the
 * application is executing.
 *
 * Note: the string returned by the function must be freed by a call to
 * st_free_str().
 * Note: handles cannot be accessed concurrently, so this is not thread safe!
 *
 * @param handle a stack transformation handle
 * @param pc the instruction pointer, pointing to code that is part of a
 *           function being executed
 * @return a string containing the compilation unit's name (must be freed by a
 *         call to st_free_str()), or NULL if there was a problem
 */
char* st_get_cu_name(st_handle handle, void* pc);

/*
 * Get the encapsulating function for the specified instruction pointer.  In
 * other words, get the name of the function the application is executing.
 *
 * Note: the string returned by the function must be freed by a call to
 * st_free_str().
 * Note: handles cannot be accessed concurrently, so this is not thread safe!
 *
 * @param handle a stack transformation handle
 * @param pc the instruction pointer, pointing to code that is part of a
 *           function being executed
 * @return a string containing the function's name (must be freed by a call to
 *         st_free_str())
 */
char* st_get_func_name(st_handle handle, void* pc);

/*
 * Print information about a function being executed, including its formal
 * arguments and local variables.
 *
 * Note: handles cannot be accessed concurrently, so this is not thread safe!
 *
 * @param handle a stack transformation handle
 * @param pc the instruction pointer, pointing to code that is part of a
 *           function being executed
 */
void st_print_func_info(st_handle handle, void* pc);

/*
 * Print out location description information for the function encapsulating
 * the specified instruction pointer.  In particular, print out location
 * description information for all arguments and local variables.
 *
 * Note: handles cannot be accessed concurrently, so this is not thread safe!
 *
 * @param handle a stack transformation handle
 * @param pc the instruction pointer, pointing to code that is part of a
 *           function being executed
 */
void st_print_func_loc_desc(st_handle handle, void* pc);

/*
 * Return the current thread's stack bounds.
 *
 * @return this thread's stack bounds information
 */
stack_bounds get_stack_bounds();

///////////////////////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////////////////////

/*
 * Free a string returned by one of the other stack transformation functions.
 *
 * Note: handles cannot be accessed concurrently, so this is not thread safe!
 *
 * @param handle a stack transformation handle
 * @param str a string returned by a stack transformation function
 */
void st_free_str(st_handle handle, char* str);

#ifdef __cplusplus
}
#endif

#endif /* _ST_H */

