.extern popcorn_migrate_args
.extern crash_aarch64
.extern migrate_lock

/* Specify GNU-stack to allow the linker to automatically set
 * noexecstack.  The Popcorn kernel forbids pages set with the
 * execute bit from migrating.  */
.section .note.GNU-stack,"",%progbits

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
   */
  str x30, [sp,#-16]! /* Don't clobber the link register */

.Llock:
/*
  adrp	x0, migrate_lock
  ldr	w0, [x0, :lo12:migrate_lock]
  cbnz	w0, .Llock
*/

  adrp x0, popcorn_migrate_args
  ldr x0, [x0, :lo12:popcorn_migrate_args]
  /*add x0, x0, :lo12:popcorn_migrate_args */

  cbz x0, .Lcrash
  ldr x1, [x0,#16] /* Get regset pointer */
  cbz x1, .Lcrash

  /*
   * According to the ABI, registers x19-x29 and v8-v15 (lower 64-bits) are
   * callee-saved.
   *
   * x* registers: address = x2 + 16 + (reg# * 8)
   * q* registers: address = x2 + 16 + (31 * 8) + (reg# * 16)
   */

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
  ldr d8, [x1,#128]
  ldr d9, [x1,#144]
  ldr d10, [x1,#160]
  ldr d11, [x1,#176]
  ldr d12, [x1,#192]
  ldr d13, [x1,#208]
  ldr d14, [x1,#224]
  ldr d15, [x1,#240]

  /* Cleanup & return to C! */
  ldr x30, [sp], #16
  ldr x1, [x0,#24] /* Load post_syscall target PC */
  mov w0, wzr /* Return successful migration */
  br x1

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (x0): struct shim_data pointer
   *  arg 2 (x1): regset pointer
   *  arg 3 (x2): return address
   */
  ldr x2, [sp]
  bl crash_aarch64

//.LEF0:
//  .comm migrate_lock,4,4

.else
	nop
	nop
	nop
	nop
	
.endif

