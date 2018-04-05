/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on aarch64.
 */

#ifndef _MIGRATE_AARCH64_H
#define _MIGRATE_AARCH64_H

#define SYSCALL_SCHED_MIGRATE 285
#define SYSCALL_PROPOSE_MIGRATION 286
#define SYSCALL_GET_THREAD_STATUS 287
#define SYSCALL_GET_NODE_INFO 288

#define GET_LOCAL_REGSET \
    struct regset_aarch64 regs_src; \
    READ_REGS_AARCH64(regs_src); \
    regs_src.pc = get_call_site()

#define LOCAL_STACK_FRAME \
    (void *)regs_src.sp


#if _NATIVE == 1 /* Safe for native execution/debugging */

#define REWRITE_STACK \
    ({ \
      int ret = 1; \
      if(st_userspace_rewrite(LOCAL_STACK_FRAME, ARCH_AARCH64, &regs_src, \
                              ARCH_AARCH64, &regs_dst)) \
      { \
        fprintf(stderr, "Could not rewrite stack!\n"); \
        ret = 0; \
      } \
      ret; \
    })

#define MIGRATE \
    { \
      SET_REGS_AARCH64(regs_src); \
      SET_FRAME_AARCH64(bp, sp); \
      SET_PC_IMM(__migrate_shim_internal); \
    }

#else /* Heterogeneous migration */

#define REWRITE_STACK \
    ({ \
      int ret = 1; \
      if(st_userspace_rewrite(LOCAL_STACK_FRAME, ARCH_AARCH64, &regs_src, \
                              dst_arch, &regs_dst)) \
      { \
        fprintf(stderr, "Could not rewrite stack!\n"); \
        ret = 0; \
      } \
      ret; \
    })

#define MIGRATE \
    asm volatile ("mov w0, %w0;" \
                  "mov x1, %1;" \
                  "mov sp, %2;" \
                  "mov x29, %3;" \
                  "mov x8, %4;" \
                  "svc 0;" \
                  : /* Outputs */ \
                  : /* Inputs */ \
                  "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                  "i"(SYSCALL_SCHED_MIGRATE) \
                  : /* Clobbered */ \
                  "w0", "x1", "x8" \
    )

#endif

#endif /* _MIGRATE_AARCH64_H */
