/*
 *  * Assembly to prepare stack for migration & to migrate between architectures
 *   * on aarch64.
 *    */

#ifndef _MIGRATE_AARCH64_H
#define _MIGRATE_AARCH64_H

#define SYSCALL_SCHED_MIGRATE 285
#define SYSCALL_PROPOSE_MIGRATION 286
#define SYSCALL_MIGRATION_PROPOSED 287
#define SYSCALL_GET_NODE_INFO 288

#define GET_LOCAL_REGSET \
    struct regset_aarch64 regs_src; \
  READ_REGS_AARCH64(regs_src)

#define LOCAL_STACK_FRAME \
    (void *)regs_src.sp


#ifdef _NATIVE /* Safe for native execution/debugging */

#define REWRITE_STACK \
    ({ \
         int ret = 1; \
         if(st_userspace_rewrite_aarch64(LOCAL_STACK_FRAME, &regs_src, &regs_src)) \
         { \
           fprintf(stderr, "Could not rewrite stack!\n"); \
           ret = 0; \
         } \
         ret; \
       })

#define SET_FP_REGS // N/A

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
      if (dst_arch == X86_64) { \
        ret = st_userspace_rewrite(LOCAL_STACK_FRAME, \
                    &regs_src, &regs_dst.x86); \
         } else if (dst_arch == AARCH64) { \
        ret = st_userspace_rewrite_aarch64(LOCAL_STACK_FRAME, \
                    &regs_src, &regs_dst.aarch); \
         } else if (dst_arch == POWERPC64) { \
        ret = st_userspace_rewrite_powerpc64(LOCAL_STACK_FRAME, \
                    &regs_src, &regs_dst.powerpc); \
         } \
        ret; \
       })

#define SET_FP_REGS \
    SET_FP_REGS_NOCLOBBER_AARCH64(*(struct regset_aarch64 *)data_ptr->regset)

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

