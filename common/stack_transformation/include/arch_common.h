/**
 * Architecture-specific declarations and definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 7/22/2016
 */

#ifndef _ARCH_COMMON_H
#define _ARCH_COMMON_H

/* Per-architecture frame pointer offsets from CFA. */
#define AARCH64_FP_OFFSET 16
#define X86_64_FP_OFFSET 8

/**
 * Frame pointer offset from canonical frame address (CFA).
 * @param arch the architecture
 * @return the offset (in bytes) from the CFA
 */
static inline uint32_t fp_offset(uint16_t arch)
{
  switch(arch) {
  case EM_X86_64: return X86_64_FP_OFFSET;
  case EM_AARCH64: return AARCH64_FP_OFFSET;
  default: return 0;
  }
}

#endif /* _ARCH_COMMON_H */

