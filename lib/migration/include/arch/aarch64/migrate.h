/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on aarch64.
 */

#ifndef _MIGRATE_AARCH64_H
#define _MIGRATE_AARCH64_H

#include <syscall.h>

#define CURRENT_ARCH ARCH_AARCH64

#define GET_LOCAL_REGSET(regset) \
    READ_REGS_AARCH64(regset.aarch); \
    regset.aarch.pc = get_call_site()

/* Get pointer to start of thread local storage region */
#define GET_TLS_POINTER \
  ({ \
    void *self; \
    asm volatile ("mrs %0, tpidr_el0" : "=r"(self)); \
    self + 16; \
  })

#if _NATIVE == 1 /* Safe for native execution/debugging */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    !st_userspace_rewrite((void *)regs_src.aarch.sp, ARCH_AARCH64, &regs_src, \
                          ARCH_AARCH64, &regs_dst)

#define MIGRATE(err) \
    { \
      err = 0; \
      SET_REGS_AARCH64(regs_src.aarch); \
      SET_FRAME_AARCH64(bp, sp); \
      SET_PC_IMM(__migrate_shim_internal); \
    }

#else /* Heterogeneous migration */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    ({ \
      int ret = 1; \
      if(dst_arch != ARCH_AARCH64) \
        ret = !st_userspace_rewrite((void *)regs_src.aarch.sp, ARCH_AARCH64, \
                                    &regs_src, dst_arch, &regs_dst); \
      else memcpy(&regs_dst, &regs_src, sizeof(struct regset_aarch64)); \
      ret; \
    })

#define FIXUP_CLOBBERS "x0", "x1", "x2"

#define MIGRATE(err) \
    ({ \
      if(dst_arch != ARCH_AARCH64) \
      { \
        data.post_syscall = __migrate_shim_internal; \
        asm volatile ("mov w0, %w1;" \
                      "mov x1, %2;" \
                      "mov sp, %3;" \
                      "mov x29, %4;" \
                      "mov x8, %5;" \
                      "svc 0;" \
                      "mov %w0, w0;" \
                      : /* Outputs */ \
                      "=r"(err) \
                      : /* Inputs */ \
                      "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYS_sched_migrate) \
                      : /* Clobbered */ \
                      FIXUP_CLOBBERS, "w0", "x1", "x8"); \
      } \
      else \
      { \
        asm volatile ("adr x0, 1f;" \
                      "str x0, %0;" \
                      "mov w0, %w2;" \
                      "mov x1, %3;" \
                      "mov sp, %4;" \
                      "mov x29, %5;" \
                      "mov x8, %6;" \
                      "svc 0;" \
                      "1: mov %w1, w0;" \
                      : /* Outputs */ \
                      "=m"(data.post_syscall), "=r"(err) \
                      : /* Inputs */ \
                      "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYS_sched_migrate) \
                      : /* Clobbered */ \
                      FIXUP_CLOBBERS, "x0", "x1", "x8"); \
      } \
    })

#endif

#endif /* _MIGRATE_AARCH64_H */
