.extern pthread_migrate_args
.extern __migrate_shim_internal
.extern crash_powerpc64

.section .text.__migrate_fixup_powerpc64, "ax"
.globl __migrate_fixup_powerpc64
.type __migrate_fixup_powerpc64,@function
.align 4
__migrate_fixup_powerpc64:
.ifdef __powerpc64__
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
  subi 1, 1, 16
  mflr 0
  std 0, 0(1) /* Don't clobber the link register */
  bl pthread_migrate_args
  cmpdi 3, 0
  beq .Lcrash
  ld 4, 0(3) /* Get struct shim_data pointer */
  cmpdi 4, 0
  beq .Lcrash
  ld 5, 16(4) /* Get regset pointer */
  cmpdi 5, 0
  beq .Lcrash

  /*
   * According to the ABI, registers r2, r14 - r31, and f14 - f31 (lower
   * 64-bits) are callee-saved.  Frame pointer x31 is included with
   * callee-saved registers.
   *
   * Note: in POWER parlance, "nonvolatile" refers to callee-saved registers
   * and "volatile" refers to caller-saved registers.
   *
   * r* registers: address = r5 + 24 + (reg# * 8)
   * f* registers: address = r5 + 24 + (32 * 8) + (f# * 8).
   *
   * TODO callee-saved condition registers
   */

  /* General purpose registers */
  ld 2, 40(5)
  ld 14, 136(5)
  ld 15, 144(5)
  ld 16, 152(5)
  ld 17, 160(5)
  ld 18, 168(5)
  ld 19, 176(5)
  ld 20, 184(5)
  ld 21, 192(5)
  ld 22, 200(5)
  ld 23, 208(5)
  ld 24, 216(5)
  ld 25, 224(5)
  ld 26, 232(5)
  ld 27, 240(5)
  ld 28, 248(5)
  ld 29, 256(5)
  ld 30, 264(5)
  ld 31, 272(5)

  /* Floating-point registers */
  lfd 14, 392(5)
  lfd 15, 400(5)
  lfd 16, 408(5)
  lfd 17, 416(5)
  lfd 18, 424(5)
  lfd 19, 432(5)
  lfd 20, 440(5)
  lfd 21, 448(5)
  lfd 22, 456(5)
  lfd 23, 464(5)
  lfd 24, 472(5)
  lfd 25, 480(5)
  lfd 26, 488(5)
  lfd 27, 496(5)
  lfd 28, 504(5)
  lfd 29, 512(5)
  lfd 30, 520(5)
  lfd 31, 528(5)

  /* Cleanup & return to C! */
  ld 0, 0(1)
  mtlr 0
  addi 1, 1, 16
  xor 3, 3, 3 /* Return successful migration */
  ld 4, 24(4) /* Load post_syscall target PC */
  mtctr 4
  bctrl

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (r3): return value from pthread_migrate_args
   *  arg 2 (r4): struct shim_data pointer
   *  arg 3 (r5): regset pointer
   *  arg 4 (r6): return address
   */
  ld 6, 0(1)
  bl crash_powerpc64

.endif

