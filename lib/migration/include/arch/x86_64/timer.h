/*
 * Use x86-64's invariant time stamp counter facility.
 */

#ifndef _X86_64_TIMER_H
#define _X86_64_TIMER_H

#include <x86intrin.h>

#define TIMESTAMP( ts ) (ts = __rdtsc())

// Note: the constants used in the conversion from TSC values to nanoseconds
// are hardcoded for our Intel Xeon E5-2620v4 and are derived from the blog
// post http://blog.tinola.com/?e=54
#define TIMESTAMP_DIFF( start, end ) (((end - start) * 487) >> 10)

#endif /* _X86_64_TIMER_H */

