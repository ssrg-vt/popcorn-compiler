/**
 * Architecture-specific declarations & definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#ifndef _ARCH_H
#define _ARCH_H

#include <elf.h>

/* Per-architecture frame pointer offsets from CFA. */
#define AARCH64_FP_OFFSET 16
#define X86_64_FP_OFFSET 8

/**
 * Frame pointer offset from canonical frame address (CFA).
 * @param arch the architecture
 * @return the offset (in bytes) from the CFA
 */
static inline size_t fp_offset(uint16_t arch)
{
  switch(arch) {
  case EM_X86_64: return X86_64_FP_OFFSET;
  case EM_AARCH64: return AARCH64_FP_OFFSET;
  default: return 0;
  }
}

/*
 * Because we don't generate call site metadata for musl, we hardcode an offset
 * from the beginning of "__libc_start_main" and "start" in order to calculate
 * their return addresses on different architectures.
 */
#define START_MAIN_OFF_AARCH64 0x68
#define START_MAIN_OFF_X86_64 0x4f
#define START_THREAD_OFF_AARCH64 0x80
#define START_THREAD_OFF_X86_64 0x89

/**
 * Return address offset from the start of "__libc_start_main".
 * @param arch the architecture
 * @return the return address offset (in bytes) from the start of
 *         "__libc_start_main"
 */
static inline uint64_t main_start_offset(uint16_t arch)
{
  switch(arch) {
  case EM_X86_64: return START_MAIN_OFF_X86_64;
  case EM_AARCH64: return START_MAIN_OFF_AARCH64;
  default: return 0;
  }
}

/**
 * Return address offset from the start of "start".
 * @param arch the architecture
 * @return the return address offset (in bytes) from the start of "start"
 */
static inline uint64_t thread_start_offset(uint16_t arch)
{
  switch(arch) {
  case EM_X86_64: return START_THREAD_OFF_X86_64;
  case EM_AARCH64: return START_THREAD_OFF_AARCH64;
  default: return 0;
  }
}

#endif /* _ARCH_H */

