/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on x86-64.
 */

#ifndef _MIGRATE_X86_64_H
#define _MIGRATE_X86_64_H

#include <syscall.h>

#ifndef SYS_sched_migrate
#define SYS_sched_migrate 330
#endif

#define GET_LOCAL_REGSET(regset) \
    READ_REGS_X86_64(regset.x86); \
    regset.x86.rip = get_call_site();

/* Get pointer to start of thread local storage region */
#define GET_TLS_POINTER \
  ({ \
    void *self; \
    asm volatile ("movq %%fs:0x0, %0" : "=r"(self)); \
    self + MUSL_PTHREAD_DESCRIPTOR_SIZE; \
  })

#if _NATIVE == 1 /* Safe for native execution/debugging */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    !st_userspace_rewrite((void *)regs_src.x86.rsp, ARCH_X86_64, &regs_src, \
                          ARCH_X86_64, &regs_dst)

#define MIGRATE(err) \
    { \
      err = 0; \
      SET_REGS_X86_64(regs_src.x86); \
      SET_FRAME_X86_64(bp, sp); \
      SET_RIP_IMM(__migrate_shim_internal); \
    }

#else /* Heterogeneous migration */

#define REWRITE_STACK(regs_src, regs_dst, dst_arch) \
    ({ \
      int ret = 1; \
      if(dst_arch != ARCH_X86_64) \
        ret = !st_userspace_rewrite((void *)regs_src.x86.rsp, ARCH_X86_64, \
                                    &regs_src, dst_arch, &regs_dst); \
      else memcpy(&regs_dst, &regs_src, sizeof(struct regset_x86_64)); \
      ret; \
    })

/* Registers clobbered during __migrate_fixup_x86_64 */
#define FIXUP_CLOBBERS "rax", "rdx", "rcx"

#define MIGRATE(err) \
    ({ \
      if(dst_arch != ARCH_X86_64) \
      { \
        data.post_syscall = __migrate_shim_internal; \
        asm volatile ("movl %1, %%edi;" \
                      "movq %2, %%rsi;" \
                      "movq %3, %%rsp;" \
                      "movq %4, %%rbp;" \
                      "movl %5, %%eax;" \
                      "syscall;" \
                      "movl %%eax, %0;" \
                      : /* Outputs */ \
                      "=g"(err) \
                      : /* Inputs */ \
                      "g"(nid), "g"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYS_sched_migrate) \
                      : /* Clobbered */ \
                      FIXUP_CLOBBERS, "edi", "rsi", "eax"); \
      } \
      else \
      { \
        asm volatile ("movq $1f, %0;" \
                      "movl %2, %%edi;" \
                      "movq %3, %%rsi;" \
                      "movq %4, %%rsp;" \
                      "movq %5, %%rbp;" \
                      "movl %6, %%eax;" \
                      "syscall;" \
                      "1: movl %%eax, %1;" \
                      : /* Outputs */ \
                      "=m"(data.post_syscall), "=g"(err) \
                      : /* Inputs */ \
                      "g"(nid), "g"(&regs_dst), "r"(sp), "r"(bp), \
                      "i"(SYS_sched_migrate) \
                      : /* Clobbered */ \
                      FIXUP_CLOBBERS, "edi", "rsi", "eax"); \
      } \
    })

#endif

#endif /* _MIGRATE_X86_64_H */
