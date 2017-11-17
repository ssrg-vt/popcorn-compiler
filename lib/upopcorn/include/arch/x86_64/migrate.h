/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on x86-64.
 */

#ifndef _MIGRATE_X86_64_H
#define _MIGRATE_X86_64_H

#define REGS_X86(_regs) (*((struct regset_x86_64*)&_regs))

#define SET_REGS(_regs) \
	SET_REGS_X86_64(REGS_X86(_regs));

#define SET_FRAME(bp,sp) \
	SET_FRAME_X86_64(bp, sp)

#define GET_LOCAL_REGSET(_regs) \
    READ_REGS_X86_64(REGS_X86(_regs))

#define LOCAL_STACK_FRAME \
	(void *)REGS_X86(regs_src).rsp

#define SET_IP_IMM(_imm) \
	SET_RIP_IMM(_imm)


#ifdef _NATIVE /* Safe for native execution/debugging */

#define REWRITE_STACK \
  ({ \
    int ret = 1; \
    if(st_userspace_rewrite_x86_64(LOCAL_STACK_FRAME, &regs_src, &regs_src)) \
    { \
      fprintf(stderr, "Could not rewrite stack!\n"); \
      ret = 0; \
    } \
    ret; \
  })

#define SET_FP_REGS(_regset) // N/A

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
	if (dst_arch == X86_64) { \
		ret = st_userspace_rewrite_x86_64(LOCAL_STACK_FRAME, \
				&REGS_X86(regs_src), &regs_dst.x86); \
    	} else if (dst_arch == AARCH64) { \
		ret = st_userspace_rewrite(LOCAL_STACK_FRAME, \
				&REGS_X86(regs_src), &regs_dst.aarch); \
	} \
	ret; \
  })

#define SET_FP_REGS(_regset) \
  SET_FP_REGS_NOCLOBBER_X86_64(*(struct regset_x86_64 *)_regset)

#endif

#endif /* _MIGRATE_X86_64_H */
