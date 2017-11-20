/*
 * Assembly to prepare stack for migration & to migrate between architectures
 * on aarch64.
 */

#ifndef _MIGRATE_AARCH64_H
#define _MIGRATE_AARCH64_H

#define REGS_AARCH64(_regs) ((_regs).aarch)

#define SET_REGS_PTR(_regs) \
	SET_REGS_AARCH64(REGS_AARCH64(*_regs));

#define SET_FRAME(bp,sp) \
	SET_FRAME_AARCH64(bp, sp)

#define GET_LOCAL_REGSET(_regs) \
    READ_REGS_AARCH64(REGS_AARCH64(_regs))

#define LOCAL_STACK_FRAME \
	(void *)REGS_AARCH64(regs_src).sp

#define SET_IP_IMM(_imm) \
	SET_PC_IMM(_imm)

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

#define SET_FP_REGS(_regs) // N/A

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
				&REGS_AARCH64(regs_src), &regs_dst.x86); \
    } else if (dst_arch == AARCH64) { \
		ret = st_userspace_rewrite_aarch64(LOCAL_STACK_FRAME, \
				&REGS_AARCH64(regs_src), &regs_dst.aarch); \
	} \
	ret; \
  })

#define SET_FP_REGS_PTR(_regset) \
  SET_FP_REGS_NOCLOBBER_AARCH64(REGS_AARCH64(*_regset))

#endif

#endif /* _MIGRATE_AARCH64_H */
