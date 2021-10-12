.extern pthread_get_migrate_args
.extern crash_riscv64

.section .text.__migrate_fixup_riscv64, "ax"
.globl __migrate_fixup_riscv64
.type __migrate_fixup_riscv64,@function
.align 4
__migrate_fixup_riscv64:
.ifdef __riscv64__

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
  sd ra, -16(sp) /* Don't clobber the link register */
  jal pthread_get_migrate_args
  beqz a0, .Lcrash
  ld a1, 16(a0) /* Get regset pointer */
  beqz a1, .Lcrash

  /*
   * According to the ABI, registers x8-x9, x18-x27, f8-f9, and f18-v27 are
   * callee-saved.
   *
   * x* registers: address = a1 + 8 + (reg# * 8)
   * f* registers: address = a1 + 8 + (31 * 8) + (reg# * 8)
   */

  /* General-purpose registers */
  /* ld x8, 80(a1) */

  ld x9, 80(a1)
  ld x18, 152(a1) /* s2 */
  ld x19, 160(a1)
  ld x20, 168(a1)
  ld x21, 176(a1)
  ld x22, 184(a1)
  ld x23, 192(a1)
  ld x24, 200(a1)
  ld x25, 208(a1)
  ld x26, 216(a1)
  ld x27, 224(a1)
  ld x28, 232(a1)

  /* Floating-point registers */
  addi a1, a1, 264 /* Update base, otherwise offsets will be out of range */
  fld f8, 64(a1)
  fld f9, 72(a1)
  fld f18, 144(a1)
  fld f19, 152(a1)
  fld f20, 160(a1)
  fld f21, 168(a1)
  fld f22, 176(a1)
  fld f23, 184(a1)
  fld f24, 192(a1)
  fld f25, 200(a1)
  fld f26, 208(a1)
  fld f27, 216(a1)

  /* Cleanup & return to C! */
  ld ra, -16(sp)
  ld a1, 24(a0) /* Load post_syscall target PC */
  mv a0, zero /* Return successful migration */

  jr a1

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (x0): struct shim_data pointer
   *  arg 2 (x1): regset pointer
   *  arg 3 (x2): return address
   */
  ld a3, (sp)
  jal crash_riscv64

/*  .comm debug_lock, 4, 4  */
.endif
