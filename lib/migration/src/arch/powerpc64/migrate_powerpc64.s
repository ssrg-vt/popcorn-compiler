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
  subi 1, 1, 8
  mflr 0
  std 0, 0(1)
  bl pthread_migrate_args
  ld 4, 0(3) /* Get struct shim_data pointer */
  cmpdi 4, 0
  beq .Lcrash

  /*
   * r4 points to the base of the shim_data struct.  First, load the regset
   * pointer.  Next, load the callee-saved r* & f* registers.  The r* registers
   * start at base + 24, so the address = r4 + 24 + (reg# * 8).  The f*
   * registers start at base + 24 + (32 * 8), so the
   * address = r4 + 24 + (32 * 8) + (f# * 8).
   *
   * According to the ABI, registers r2, r14 - r31, and f14 - f31 (lower
   * 64-bits) are callee-saved.  Frame pointer x31 is included with
   * callee-saved registers.
   *
   * Note: in POWER parlance, "nonvolatile" refers to callee-saved registers
   * and "volatile" refers to caller-saved registers.
   *
   * TODO callee-saved condition registers
   */
  ld 4, 16(4) /* Get regset pointer */

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
  addi 1, 1, 8
  b __migrate_shim_internal

.Lcrash:
  /*
   * We got garbage data post-migration, crash with the following information:
   *
   *  arg 1 (r3): return value from pthread_migrate_args
   *  arg 2 (r4): struct shim_data pointer
   *  arg 3 (r5): return address
   */
  ld 5, 0(1)
  bl crash_powerpc64

.endif

