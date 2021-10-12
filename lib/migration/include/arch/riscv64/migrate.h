/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on riscv64.
 */

#ifndef _MIGRATE_RISCV64_H
#define _MIGRATE_RISCV64_H

#include <syscall.h>

#define GET_LOCAL_REGSET(regset) \
    READ_REGS_RISCV64(regset.riscv); \
    regset.riscv.pc = get_call_site()

/* Get pointer to start of thread local storage region */
#define GET_TLS_POINTER \
  ({ \
    void *self; \
    asm volatile ("addi %0, tp, 0" : "=r"(self)); \
    self - 16; \
  })

#if _NATIVE == 1 /* Safe for native execution/debugging */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    !st_userspace_rewrite((void *)regs_src.riscv.x[2], ARCH_RISCV64, &regs_src, \
                          ARCH_RISCV64, &regs_dst)

#define MIGRATE(err) \
    { \
      err = 0; \
      SET_REGS_RISCV64(regs_src.riscv); \
      SET_FRAME_RISCV64(bp, sp); \
      SET_PC_IMM(__migrate_shim_internal); \
    }

#else /* Heterogeneous migration */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    ({ \
      int ret = 1; \
      if(dst_arch != ARCH_RISCV64) \
        ret = !st_userspace_rewrite((void *)regs_src.riscv.x[2], ARCH_RISCV64, \
                                    &regs_src, dst_arch, &regs_dst); \
      else memcpy(&regs_dst, &regs_src, sizeof(struct regset_riscv64)); \
      ret; \
    })

#define FIXUP_CLOBBERS "a0", "a1", "a2"

#define MIGRATE(err) \
    ({ \
      if(dst_arch != ARCH_RISCV64) \
      { \
        data.post_syscall = __migrate_shim_internal; \
        asm volatile ("addi a0, %1, 0;" \
                      "addi a1, %2, 0;" \
                      "addi sp, %3, 0;" \
                      "addi fp, %4, 0;" \
                      "addi a7, zero, %5;" \
                      "ecall;" \
                      "mv %0, a0;" \
                      : /* Outputs */ \
                      "=r"(err) \
                      : /* Inputs */ \
                      "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYS_sched_migrate) \
                      : /* Clobbered */ \
                      FIXUP_CLOBBERS, "a0", "a7", "s0"); \
      } \
      else \
      { \
        asm volatile ("la a0, 1f;" \
                      "sw a0, %0;" \
                      "mv a0, %2;" \
                      "mv a1, %3;" \
                      "mv sp, %4;" \
                      "mv fp, %5;" \
                      "addi a7, zero, %6;" \
                      "ecall;" \
                      "1: mv %1, a0;" \
                      : /* Outputs */ \
                      "=m"(data.post_syscall), "=r"(err) \
                      : /* Inputs */ \
                      "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYS_sched_migrate) \
                      : /* Clobbered */ \
                      FIXUP_CLOBBERS, "a0", "a7", "s0"); \
      } \
    })

#endif

#endif /* _MIGRATE_RISCV64_H */
