#ifndef _ARCH_CRASH_H
#define _ARCH_CRASH_H

#include <stdbool.h>

/* These APIs will crash an application *only* if called on the corresponding
 * architecture, i.e., crash_aarch64() will only crash the program if the
 * calling thread is executing on aarch64. */

bool crash_aarch64();
bool crash_powerpc64();
bool crash_x86_64();

#endif
