/**
 * Architecture-specific declarations & definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#ifndef _ARCH_H
#define _ARCH_H

#include <elf.h>

/*
 * Because we don't generate call site metadata for musl, we hardcode an offset
 * from the beginning of "__libc_start_main" and "start" in order to calculate
 * their return addresses on different architectures.
 */
//#define START_MAIN_OFF_AARCH64 0x68
#define START_MAIN_OFF_AARCH64 0x48
//#define START_MAIN_OFF_X86_64 0x4f
#define START_MAIN_OFF_X86_64 0x33
#define START_MAIN_OFF_POWERPC64 0x5c
//#define START_THREAD_OFF_AARCH64 0x7c
#define START_THREAD_OFF_AARCH64 0x74
#define START_THREAD_OFF_X86_64 0x89
#define START_THREAD_OFF_POWERPC64 0xbc

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
  case EM_PPC64: return START_MAIN_OFF_POWERPC64;
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
  case EM_PPC64: return START_THREAD_OFF_POWERPC64;
  default: return 0;
  }
}

/**
 * LLVM only records the tracked frame size, ignoring implicitly added frame
 * objects that must be tracked for the CFA (e.g., the return address
 * implicitly pushed onto the stack by call on x86-64).  Correct the frame size
 * to include these values.
 * @param arch the architecture
 * @param size the frame size recorded in LLVM's stackmap
 * @return the corrected frame size for a valid CFA
 */
static inline uint64_t cfa_correction(uint16_t arch, uint64_t size)
{
  switch(arch) {
  case EM_X86_64: return size + 8; // Include return address pushed by call
  default: return size;
  }
}

#endif /* _ARCH_H */

