.extern pthread_get_migrate_args
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
   */
  subi 1, 1, 16
  mflr 0
  std 0, 0(1) /* Don't clobber the link register */
  bl pthread_get_migrate_args
  cmpdi 3, 0
  beq .Lcrash
  ld 4, 16(3) /* Get regset pointer */
  cmpdi 4, 0
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
  ld 2, 40(4)
  ld 14, 136(4)
  ld 15, 144(4)
  ld 16, 152(4)
  ld 17, 160(4)
  ld 18, 168(4)
  ld 19, 176(4)
  ld 20, 184(4)
  ld 21, 192(4)
  ld 22, 200(4)
  ld 23, 208(4)
  ld 24, 216(4)
  ld 25, 224(4)
  ld 26, 232(4)
  ld 27, 240(4)
  ld 28, 248(4)
  ld 29, 256(4)
  ld 30, 264(4)
  ld 31, 272(4)

  /* Floating-point registers */
  lfd 14, 392(4)
  lfd 15, 400(4)
  lfd 16, 408(4)
  lfd 17, 416(4)
  lfd 18, 424(4)
  lfd 19, 432(4)
  lfd 20, 440(4)
  lfd 21, 448(4)
  lfd 22, 456(4)
  lfd 23, 464(4)
  lfd 24, 472(4)
  lfd 25, 480(4)
  lfd 26, 488(4)
  lfd 27, 496(4)
  lfd 28, 504(4)
  lfd 29, 512(4)
  lfd 30, 520(4)
  lfd 31, 528(4)

  /* Cleanup & return to C! */
  ld 0, 0(1)
  mtlr 0
  addi 1, 1, 16
  ld 4, 24(3) /* Load post_syscall target PC */
  xor 3, 3, 3 /* Return successful migration */
  mtctr 4
  bctrl

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (r3): struct shim_data pointer
   *  arg 2 (r4): regset pointer
   *  arg 3 (r5): return address
   */
  ld 5, 0(1)
  bl crash_powerpc64

.endif

