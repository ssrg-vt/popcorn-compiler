/*
 * Shim to include architecture-specific class definitions & define some values
 * useful for statically sizing rewriting data.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/26/2016
 */

#ifndef _ARCH_REGS_H
#define _ARCH_REGS_H

#include <sys/param.h>
#include "bitmap.h"
#include "arch/aarch64/internal.h"
#include "arch/aarch64/regs.h"
#include "arch/x86_64/internal.h"
#include "arch/x86_64/regs.h"

/* Largest size values, in bytes, across all supported architectures */

/* Largest register set size  */
#define MAX_REGSET_SIZE MAX(sizeof(struct regset_aarch64), \
                            sizeof(struct regset_x86_64))

/* Largest bitmap size */
#define MAX_CALLEE_SIZE MAX(bitmap_size(AARCH64_NUM_REGS), \
                            bitmap_size(X86_64_NUM_REGS))

#endif
