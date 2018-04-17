/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on powerpc64.
 */

#ifndef _MIGRATE_powerpc64_H
#define _MIGRATE_powerpc64_H

#define SYSCALL_SCHED_MIGRATE 379
#define SYSCALL_PROPOSE_MIGRATION 380
#define SYSCALL_GET_THREAD_STATUS 381
#define SYSCALL_GET_NODE_INFO 382

#define GET_LOCAL_REGSET(regset) \
    READ_REGS_POWERPC64(regset.powerpc); \
    regset.powerpc.pc = get_call_site()

#if _NATIVE == 1 /* Safe for native execution/debugging */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    !st_userspace_rewrite((void *)regs_src.powerpc.pc, ARCH_POWERPC64, \
                          &regs_src, ARCH_POWERPC64, &regs_dst)

#define MIGRATE(err) \
    { \
      err = 0; \
      SET_REGS_POWERPC64(regs_src.powerpc); \
      SET_FRAME_POWERPC64(bp, sp); \
      SET_PC_IMM(__migrate_shim_internal); \
    }

#else /* Heterogeneous migration */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    ({ \
      int ret = 1; \
      if(dst_arch != ARCH_POWERPC64) \
        ret = !st_userspace_rewrite((void *)regs_src.powerpc.pc, \
                                    ARCH_POWERPC64, &regs_src, \
                                    dst_arch, &regs_dst); \
      else memcpy(&regs_dst, &regs_src, sizeof(struct regset_powerpc64)); \
      ret; \
    })

#define MIGRATE(err) \
    ({ \
      if(dst_arch != ARCH_POWERPC64) \
      { \
        data.post_syscall = __migrate_shim_internal; \
        asm volatile ("mr 3, %1;" \
                      "mr 4, %2;" \
                      "mr 1, %3;" \
                      "mr 31,%4;" \
                      "li 0, %5;" \
                      "sc;" \
                      "mr %0, 3;" \
                      : /* Outputs */ \
                      "=r"(err) \
                      : /* Inputs */ \
                      "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYSCALL_SCHED_MIGRATE) \
                      : /* Clobbered */ \
                      "r3", "r4", "r0"); \
      } \
      else \
      { \
        asm volatile ("li 3, 1f;" \
                      "std 3, %0;" \
                      "mr 3, %2;" \
                      "mr 4, %3;" \
                      "mr 1, %4;" \
                      "mr 31,%5;" \
                      "li 0, %6;" \
                      "sc;" \
                      "1: mr %1, 3" \
                      : /* Outputs */ \
                      "=m"(data.post_syscall), "=r"(err) \
                      : /* Inputs */ \
                      "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYSCALL_SCHED_MIGRATE) \
                      : /* Clobbered */ \
                      "r3", "r4", "r0"); \
      } \
    )}

#endif

#endif /* _MIGRATE_powerpc64_H */
