/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on x86-64.
 */

#ifndef _MIGRATE_X86_64_H
#define _MIGRATE_X86_64_H

#define SYSCALL_SCHED_MIGRATE 330
#define SYSCALL_PROPOSE_MIGRATION 331
#define SYSCALL_MIGRATION_PROPOSED 332
#define SYSCALL_GET_NODE_INFO 333

#define GET_LOCAL_REGSET \
    struct regset_x86_64 regs_src; \
    READ_REGS_X86_64(regs_src)

#define LOCAL_STACK_FRAME \
    (void *)regs_src.rsp


#if _NATIVE == 1 /* Safe for native execution/debugging */

#define REWRITE_STACK \
    ({ \
      int ret = 1; \
      if(st_userspace_rewrite(LOCAL_STACK_FRAME, ARCH_X86_64, &regs_src, \
                              ARCH_X86_64, &regs_dst)) \
      { \
        fprintf(stderr, "Could not rewrite stack!\n"); \
        ret = 0; \
      } \
      ret; \
    })

#define SET_FP_REGS // N/A

#define MIGRATE \
    { \
      SET_REGS_X86_64(regs_src); \
      SET_FRAME_X86_64(bp, sp); \
      SET_RIP_IMM(__migrate_shim_internal); \
    }

#else /* Heterogeneous migration */

#define REWRITE_STACK \
    ({ \
      int ret = 1; \
      if(st_userspace_rewrite(LOCAL_STACK_FRAME, ARCH_X86_64, &regs_src, \
                              dst_arch, &regs_dst)) \
      { \
        fprintf(stderr, "Could not rewrite stack!\n"); \
        ret = 0; \
      } \
      ret; \
    })

#define SET_FP_REGS \
    SET_FP_REGS_NOCLOBBER_X86_64(*(struct regset_x86_64 *)data_ptr->regset)

#define MIGRATE \
    asm volatile ("mov %0, %%rdi;" \
                  "movq %1, %%rsi;" \
                  "movq %2, %%rsp;" \
                  "movq %3, %%rbp;" \
                  "mov %4, %%eax;" \
                  "syscall;" \
                  : /* Outputs */ \
                  : /* Inputs */ \
                  "g"(nid), "g"(&regs_dst), "r"(sp), "r"(bp), \
                  "i"(SYSCALL_SCHED_MIGRATE) \
                  : /* Clobbered */ \
                  "rdi", "rsi" \
    )

#endif

#endif /* _MIGRATE_X86_64_H */

