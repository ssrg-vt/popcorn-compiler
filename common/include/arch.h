/**
 * Defines architectures used by multiple components in tools & supporting libraries.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 12/12/2017
 */

#ifndef _ARCH_H
#define _ARCH_H

enum arch {
  ARCH_UNKNOWN = -1,
  ARCH_AARCH64,
  ARCH_X86_64,
  ARCH_POWERPC64,
  NUM_ARCHES
};

#endif /* _ARCH_H */

