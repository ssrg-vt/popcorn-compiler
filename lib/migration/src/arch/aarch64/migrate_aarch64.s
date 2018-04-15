.extern pthread_migrate_args
.extern __migrate_shim_internal
.extern crash_aarch64

.section .text.__migrate_fixup_aarch64, "ax"
.globl __migrate_fixup_aarch64
.type __migrate_fixup_aarch64,@function
.align 4
__migrate_fixup_aarch64:
.ifdef __aarch64__
  /*
   * Get a reference to the migration arguments struct, which is of the form:
   *
   *   struct shim_data {
   *     void (*callback)(void *);
   *     void *callback_data;
   *     void *regset;
   *   };
   *
   * We need a reference to the regset, which we can then use to restore the
   * callee-saved registers.  But, we need to enusre that we don't clobber the
   * link register in the process.
   *
   * Note: we don't need to save caller-saved registers -- due to the stack
   * transformation process, all caller-saved registers should have been saved
   * to the stack in the caller's frame.
   */
  str x30, [sp,#-16]!
  bl pthread_migrate_args
  ldr x1, [x0] /* Get struct shim_data pointer */
  cbz x1, .Lcrash

  /*
   * x0 points to the base of the shim_data struct.  First, load the regset
   * pointer.  Next, load the callee-saved x* and q* registers.  The x*
   * registers start at base + 16, so the address = x1 + 16 + (reg# * 8).  The
   * q* registers start at base + 16 + (31 * 8), so the
   * address = x1 + 16 + (31 * 8) + (reg# * 16).
   *
   * According to the ABI, registers x19-x29 and v8-v15 (lower 64-bits) are
   * callee-saved.  Frame pointer x29 is included with the callee-saved
   * registers.
   */
  ldr x1, [x1,#16] /* Get regset pointer */

  /* General-purpose registers */
  ldr x19, [x1,#168]
  ldr x20, [x1,#176]
  ldr x21, [x1,#184]
  ldr x22, [x1,#192]
  ldr x23, [x1,#200]
  ldr x24, [x1,#208]
  ldr x25, [x1,#216]
  ldr x26, [x1,#224]
  ldr x27, [x1,#232]
  ldr x28, [x1,#240]
  ldr x29, [x1,#248]

  /* Floating-point registers */
  add x1, x1, #264 /* Update base, otherwise offsets will be out of range */
  ldr q8, [x1,#128]
  ldr q9, [x1,#144]
  ldr q10, [x1,#160]
  ldr q11, [x1,#176]
  ldr q12, [x1,#192]
  ldr q13, [x1,#208]
  ldr q14, [x1,#224]
  ldr q15, [x1,#240]

  /* Cleanup & return to C! */
  ldr x30, [sp], #16
  b __migrate_shim_internal

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (x0): return value from pthread_migrate_args
   *  arg 2 (x1): struct shim_data pointer
   *  arg 3 (x2): return address
   */
  ldr x2, [sp]
  bl crash_aarch64

.endif

