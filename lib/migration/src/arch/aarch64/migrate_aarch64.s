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
   *     void *post_syscall;
   *   };
   *
   * We use the regset to restore the callee-saved registers and post_syscall
   * as the destination at which we return to normal execution.
   *
   * Note: we don't need to save caller-saved registers -- due to the stack
   * transformation process, all caller-saved registers should have been saved
   * to the stack in the caller's frame.
   */
  str x30, [sp,#-16]! /* Don't clobber the link register */
  bl pthread_migrate_args
  cbz x0, .Lcrash
  ldr x1, [x0] /* Get struct shim_data pointer */
  cbz x1, .Lcrash
  ldr x2, [x1,#16] /* Get regset pointer */
  cbz x2, .Lcrash

  /*
   * According to the ABI, registers x19-x29 and v8-v15 (lower 64-bits) are
   * callee-saved.
   *
   * x* registers: address = x2 + 16 + (reg# * 8)
   * q* registers: address = x2 + 16 + (31 * 8) + (reg# * 16)
   */

  /* General-purpose registers */
  ldr x19, [x2,#168]
  ldr x20, [x2,#176]
  ldr x21, [x2,#184]
  ldr x22, [x2,#192]
  ldr x23, [x2,#200]
  ldr x24, [x2,#208]
  ldr x25, [x2,#216]
  ldr x26, [x2,#224]
  ldr x27, [x2,#232]
  ldr x28, [x2,#240]
  ldr x29, [x2,#248]

  /* Floating-point registers */
  add x2, x2, #264 /* Update base, otherwise offsets will be out of range */
  ldr q8, [x2,#128]
  ldr q9, [x2,#144]
  ldr q10, [x2,#160]
  ldr q11, [x2,#176]
  ldr q12, [x2,#192]
  ldr q13, [x2,#208]
  ldr q14, [x2,#224]
  ldr q15, [x2,#240]

  /* Cleanup & return to C! */
  ldr x30, [sp], #16
  mov w0, wzr /* Return successful migration */
  ldr x1, [x1,#24] /* Load post_syscall target PC */
  br x1

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (x0): return value from pthread_migrate_args
   *  arg 2 (x1): struct shim_data pointer
   *  arg 3 (x2): regset pointer
   *  arg 4 (x3): return address
   */
  ldr x3, [sp]
  bl crash_aarch64

.endif

