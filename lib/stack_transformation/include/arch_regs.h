/*
 * Shim to include architecture-specific class definitions, only needed when
 * constructing new register set objects, as the functions can then be accessed
 * via the newly created object's function pointers.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/26/2016
 */

#ifndef _ARCH_REGS_H
#define _ARCH_REGS_H

#include "arch/aarch64/internal.h"
#include "arch/x86_64/internal.h"

#endif
