/**
 * Architecture-specific declarations & definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#ifndef _ARCH_H
#define _ARCH_H

#include <elf.h>

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

