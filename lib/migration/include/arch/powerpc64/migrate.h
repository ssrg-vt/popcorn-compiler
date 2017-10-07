/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on powerpc64.
 */

#ifndef _MIGRATE_POWERPC64_H
#define _MIGRATE_POWERPC64_H

#ifdef _NATIVE /* Safe for native execution/debugging */

// This design is very inflexible such that putting a 3rd node without changing many things is impossible
// Zero code sharing and zero maintainability
#define REWRITE_STACK \
  ({ \
    int ret = 1; \
    READ_REGS_POWERPC64(regs_powerpc64); \
    regs_powerpc64.pc = get_call_site(); \
    if(st_userspace_rewrite_powerpc64(regs_powerpc64.r[1], &regs_powerpc64, &regs_powerpc64)) \
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
    SET_REGS_POWERPC64(regs_powerpc64); \
    SET_FRAME_POWERPC64(regs_powerpc64.r[31], regs_powerpc64.r[1]); \
    SET_PC_IMM(new_pc); \
  }

#else /* Heterogeneous migration */

#define REWRITE_STACK_X86 \
  ({ \
    int ret = 1; \
    READ_REGS_POWERPC64(regs_powerpc64); \
    regs_powerpc64.pc = get_call_site(); \
    if(st_userspace_rewrite((void*)regs_powerpc64.r[1], &regs_powerpc64, &regs_x86_64)) \
    { \
      fprintf(stderr, "Could not rewrite stack!\n"); \
      ret = 0; \
    } \
    ret; \
  })

#define SET_FP_REGS \
  SET_FP_REGS_NOCLOBBER_POWERPC64(*(struct regset_powerpc64 *)data_ptr->regset)

#define SAVE_REGSET_X86 { data.regset = &regs_x86_64; }

#define MIGRATE_X86( pid, cpu_set_size, cpu_set, new_pc ) \
  asm volatile ("li 0, %0;" \
                "li 3, %1;" \
                "li 4, %2;" \
                "li 5, %3;" \
                "li 6, 0;" \
                "mr 7, %4;" \
                "li 8, 285;" /* __NR_sched_setaffinity_popcorn */ \
                "mr 1, %5;" \
                "mr 31, %6;" \
                "sc 0;" /*TODO: It's different from musl 1.16's syscall example. Check in case of a bug */\
    : /* Outputs */ \
    : "i"(pid), "i"(cpu_set_size), "r"(cpu_set), "r"(new_pc), \
      "r"(&regs_x86_64), "r"(regs_x86_64.rsp), "r"(regs_x86_64.rbp) /* Inputs */ \
    : "memory", "cr0", "r9", "r10", "r11", "r12"); /* Clobbered */

#define REWRITE_STACK_AARCH64 \
  ({ \
    int ret = 1; \
    READ_REGS_POWERPC64(regs_powerpc64); \
    regs_powerpc64.pc = get_call_site(); \
    if(st_userspace_rewrite((void*)regs_powerpc64.r[1], &regs_powerpc64, &regs_aarch64)) \
    { \
      fprintf(stderr, "Could not rewrite stack!\n"); \
      ret = 0; \
    } \
    ret; \
  })

#define SET_FP_REGS \
  SET_FP_REGS_NOCLOBBER_POWERPC64(*(struct regset_powerpc64 *)data_ptr->regset)

#define SAVE_REGSET_AARCH64 { data.regset = &regs_aarch64; }

#define MIGRATE( pid, cpu_set_size, cpu_set, new_pc ) \
  asm volatile ("li 0, %0;" \
                "li 3, %1;" \
                "li 4, %2;" \
                "li 5, %3;" \
                "li 6, 0;" \
                "mr 7, %4;" \
                "li 8, 285;" /* __NR_sched_setaffinity_popcorn */ \
                "mr 1, %5;" \
                "mr 31, %6;" \
                "sc 0;" /*TODO: It's different from musl 1.16's syscall example. Check in case of a bug */\
    : /* Outputs */ \
    : "i"(pid), "i"(cpu_set_size), "r"(cpu_set), "r"(new_pc), \
      "r"(&regs_aarch64), "r"(regs_aarch64.sp), "r"(regs_aarch64.x[29]) /* Inputs */ \
    : "memory", "cr0", "r9", "r10", "r11", "r12"); /* Clobbered */


#endif

#endif /* _MIGRATE_POWERPC64_H */

