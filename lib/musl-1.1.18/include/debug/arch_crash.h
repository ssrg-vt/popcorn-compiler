#ifndef _ARCH_CRASH_H
#define _ARCH_CRASH_H

#include <stdbool.h>

/* Crash, placing up to 4 values in the first 4 integer registers of the
 * architecture on which we're executing. */
bool crash(long a, long b, long c, long d);

/* These APIs will crash an application *only* if called on the corresponding
 * architecture, i.e., crash_aarch64() will only crash the program if the
 * calling thread is executing on aarch64. Similar to above, place up to 4
 * values in the first 4 integer registers. */

bool crash_aarch64(long a, long b, long c, long d);
bool crash_powerpc64(long a, long b, long c, long d);
bool crash_x86_64(long a, long b, long c, long d);

#endif
