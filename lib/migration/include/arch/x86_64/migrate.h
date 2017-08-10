/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on x86-64.
 */

#ifndef _MIGRATE_X86_64_H
#define _MIGRATE_X86_64_H

#ifdef _NATIVE /* Safe for native execution/debugging */

#define REWRITE_STACK \
  ({ \
    int ret = 1; \
    READ_REGS_X86_64(regs_x86_64); \
    if(st_userspace_rewrite_x86_64((void*)regs_x86_64.rsp, &regs_x86_64, &regs_x86_64)) \
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
    SET_REGS_X86_64(regs_x86_64); \
    SET_FRAME_X86_64(regs_x86_64.rbp, regs_x86_64.rsp); \
    SET_RIP_IMM(new_pc); \
  }

#else /* Heterogeneous migration */

// TODO: This has to be changed once we're able to migrate between 3 architectures
#ifndef __powerpc64__
#define __powerpc64__

#if defined(__powerpc64__)
#define REWRITE_STACK \
  ({ \
    int ret = 1; \
    READ_REGS_X86_64(regs_x86_64); \
      if(st_userspace_rewrite((void*)regs_x86_64.rsp, &regs_x86_64, &regs_powerpc64)) \
      { \
        fprintf(stderr, "Could not rewrite stack!\n"); \
        ret = 0; \
      } \
    ret; \
  })
#elif defined(__aarch64__)
#define REWRITE_STACK \
  ({ \
    int ret = 1; \
    READ_REGS_X86_64(regs_x86_64); \
      if(st_userspace_rewrite((void*)regs_x86_64.rsp, &regs_x86_64, &regs_aarch64)) \
      { \
        fprintf(stderr, "Could not rewrite stack!\n"); \
        ret = 0; \
      } \
    ret; \
  })
#endif

#define SET_FP_REGS \
  SET_FP_REGS_NOCLOBBER_X86_64(*(struct regset_x86_64 *)data_ptr->regset)

#if defined(__powerpc64__)
  #define SAVE_REGSET { data.regset = &regs_powerpc64; }

  #define MIGRATE( pid, cpu_set_size, cpu_set, new_pc ) \
    asm volatile ("movq %0, %%rdi;" \
                  "movq %1, %%rsi;" \
                  "movq %2, %%rdx;" \
                  "movq %3, %%r10;" \
                  "movq %4, %%r8;" \
                  "movq %5, %%r9;" \
                  "mov $315, %%eax;" /* __NR_sched_setaffinity_popcorn */ \
                  "mov %6, %%rsp;" \
                  "mov %7, %%rbp;" \
                  "syscall;" \
      : /* Outputs */ \
      : "i"(pid), "i"(cpu_set_size), "g"(cpu_set), "g"(new_pc), \
        "g"(regs_powerpc64.lr), \
        "g"(&regs_powerpc64), \
        "r"(regs_powerpc64.r[1]), \
        "r"(regs_powerpc64.r[31]) /* Inputs */ \
      : "rdi", "rsi", "rdx", "r10", "r8", "r9" /* Clobbered */ \
    )
#elif defined(__aarch64__)
  #define SAVE_REGSET { data.regset = &regs_aarch64; }

  #define MIGRATE( pid, cpu_set_size, cpu_set, new_pc ) \
    asm volatile ("movq %0, %%rdi;" \
                  "movq %1, %%rsi;" \
                  "movq %2, %%rdx;" \
                  "movq %3, %%r10;" \
                  "movq %4, %%r8;" \
                  "movq %5, %%r9;" \
                  "mov $315, %%eax;" /* __NR_sched_setaffinity_popcorn */ \
                  "mov %6, %%rsp;" \
                  "mov %7, %%rbp;" \
                  "syscall;" \
      : /* Outputs */ \
      : "i"(pid), "i"(cpu_set_size), "g"(cpu_set), "g"(new_pc), \
        "g"(regs_aarch64.x[30]), \
        "g"(&regs_aarch64), \
        "r"(regs_aarch64.sp), \
        "r"(regs_aarch64.x[29]) /* Inputs */ \
      : "rdi", "rsi", "rdx", "r10", "r8", "r9" /* Clobbered */ \
    )

#endif  /* POWERPC64 */


#endif  /* POWERPC64 || AARCH64 */

#endif /* NATIVE || HETEROGENEOUS */ 
#endif /* _MIGRATE_X86_64_H */

