/*
 *  * Assembly to prepare stack for migration & to migrate between architectures
 *   * on powerpc64.
 *    */

#ifndef _MIGRATE_powerpc64_H
#define _MIGRATE_powerpc64_H

#define SYSCALL_SCHED_MIGRATE 379
#define SYSCALL_PROPOSE_MIGRATION 380
#define SYSCALL_MIGRATION_PROPOSED 381
#define SYSCALL_GET_NODE_INFO 382

#define GET_LOCAL_REGSET \
    struct regset_powerpc64 regs_src; \
    READ_REGS_POWERPC64(regs_src)

#define LOCAL_STACK_FRAME \
    (void *)regs_src.r[1]


#if _NATIVE == 1 /* Safe for native execution/debugging */

#define REWRITE_STACK \
    ({ \
      int ret = 1; \
      if(st_userspace_rewrite_powerpc64(LOCAL_STACK_FRAME, &regs_src, &regs_src)) \
      { \
        fprintf(stderr, "Could not rewrite stack!\n"); \
        ret = 0; \
      } \
      ret; \
    })

#define SET_FP_REGS // N/A

#define MIGRATE \
    { \
      SET_REGS_POWERPC64(regs_src); \
      SET_FRAME_POWERPC64(bp, sp); \
      SET_PC_IMM(__migrate_shim_internal); \
    }

#else /* Heterogeneous migration */

#define REWRITE_STACK \
    ({ \
      int ret = 1; \
      if (dst_arch == X86_64) { \
        ret = st_userspace_rewrite(LOCAL_STACK_FRAME, \
                                   &regs_src, &regs_dst.x86); \
      } else if (dst_arch == AARCH64) { \
        ret = st_userspace_rewrite(LOCAL_STACK_FRAME, \
                                   &regs_src, &regs_dst.aarch); \
      } else if (dst_arch == POWERPC64) { \
        ret = st_userspace_rewrite(LOCAL_STACK_FRAME, \
                                   &regs_src, &regs_dst.powerpc); \
      } \
      ret; \
    })

#define SET_FP_REGS \
    SET_FP_REGS_NOCLOBBER_POWERPC64(*(struct regset_powerpc64 *)data_ptr->regset)

#define MIGRATE \
    asm volatile ("mr 3, %0;" \
                  "mr 4, %1;" \
                  "mr 1, %2;" \
                  "mr 31,%3;" \
                  "li 0, %4;" \
                  "sc ;" \
                  : /* Outputs */ \
                  : /* Inputs */ \
                  "r"(nid), "r"(&regs_dst), "r"(sp), "r"(bp), \
                  "i"(SYSCALL_SCHED_MIGRATE) \
                  : /* Clobbered */ \
                  "r3", "r4", "r0" \
    )

#endif

#endif /* _MIGRATE_powerpc64_H */
