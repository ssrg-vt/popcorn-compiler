/*
 * Migration debugging helper functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 3/3/2018
 */

#include <arch/aarch64/regs.h>
#include <arch/powerpc64/regs.h>
#include <arch/riscv64/regs.h>
#include <arch/x86_64/regs.h>

/*
 * Helper functions to dump a register set's contents to a log file.  Useful
 * for examining register sets post-transformation (and pre-migration).
 * Appends to the log file if it already exists.  If no log file name is passed
 * to the function, print the contents to stderr.
 */

/*
 * Dump an AArch64 register set to a file.
 *
 * @param regset an AArch64 register set pointer
 * @param log the name of the file which to dump the contents
 */
void dump_regs_aarch64(const struct regset_aarch64 *regset, const char *log);

/*
 * Dump a PowerPC register set to a file.
 *
 * @param regset a register set pointer
 * @param log the name of the file which to dump the contents
 */
void dump_regs_powerpc64(const struct regset_powerpc64 *regset,
                         const char *log);

/*
 * Dump a RISCV64 register set to a file.
 *
 * @param regset a register set pointer
 * @param log the name of the file which to dump the contents
 */
void dump_regs_riscv64(const struct regset_riscv64 *regset,
		       const char *log);

/*
 * Dump an x86-64 register set to a file.
 *
 * @param regset a register set pointer
 * @param log the name of the file which to dump the contents
 */
void dump_regs_x86_64(const struct regset_x86_64 *regset, const char *log);

/*
 * Dump a regset, implicitly casting it to the register set corresponding to
 * the architecture on which the thread is currently executing, i.e., cast the
 * to a struct regset_aarch64 if currently executing on AArch64.
 *
 * @param regset a register set pointer
 * @param log the name of the file which to dump the contents
 */
void dump_regs(const void *regset, const char *log);

/*
 * Initialize remote node debug handling.
 * @param nid node on which to initialize data
 */
void remote_debug_init(int nid);

/*
 * Clean up remote node debug handling.
 * @param nid node on which to clean up data
 */
void remote_debug_cleanup(int nid);

