/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on aarch64.
 */

#ifndef _MIGRATE_AARCH64_H
#define _MIGRATE_AARCH64_H

#ifdef _NATIVE /* Safe for native execution/debugging */

#define REWRITE_STACK \
  ({ \
    int ret = 1; \
    READ_REGS_AARCH64(regs_aarch64); \
    if(st_userspace_rewrite_aarch64(regs_aarch64.sp, &regs_aarch64, &regs_aarch64)) \
    { \
      fprintf(stderr, "Could not rewrite stack!\n"); \
      ret = 0; \
    } \
    ret; \
  })

#define SET_FP_REGS // N/A

#define SAVE_REGSET // N/A

#define MIGRATE( pid, cpu_set_size, cpu_set, new_pc ) \
  { \
    SET_REGS_AARCH64(regs_aarch64); \
    SET_FRAME_AARCH64(regs_aarch64.x[29], regs_aarch64.sp); \
    SET_PC_IMM(new_pc); \
  }

#else /* Heterogeneous migration */

#define REWRITE_STACK \
  ({ \
    int ret = 1; \
    READ_REGS_AARCH64(regs_aarch64); \
    if(st_userspace_rewrite(regs_aarch64.sp, &regs_aarch64, &regs_x86_64)) \
    { \
      fprintf(stderr, "Could not rewrite stack!\n"); \
      ret = 0; \
    } \
    ret; \
  })

#define SET_FP_REGS \
  SET_FP_REGS_NOCLOBBER_AARCH64(*(struct regset_aarch64 *)data_ptr->regset)

#define SAVE_REGSET { data.regset = &regs_x86_64; }

#define MIGRATE( pid, cpu_set_size, cpu_set, new_pc ) \
  asm volatile ("mov x0, %0;" \
                "mov x1, %1;" \
                "mov x2, %2;" \
                "mov x3, %3;" \
                "mov x4, #0;" \
                "mov x5, %4;" \
                "mov x8, #274;" /* __NR_sched_setaffinity_popcorn */ \
                "mov sp, %5;" \
                "mov x29, %6;" \
                "svc 0;" \
    : /* Outputs */ \
    : "i"(pid), "i"(cpu_set_size), "r"(cpu_set), "r"(new_pc), \
      "r"(&regs_x86_64), "r"(regs_x86_64.rsp), "r"(regs_x86_64.rbp) /* Inputs */ \
    : "x0", "x1", "x2", "x3", "x4", "x5", "x8" /* Clobbered */ \
  )

#endif

#endif /* _MIGRATE_AARCH64_H */

