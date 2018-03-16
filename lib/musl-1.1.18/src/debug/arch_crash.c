#include <stdbool.h>
#include "atomic.h"
#include "libc.h"

/* Only crash if we're executing on aarch64 */
bool __crash_aarch64()
{
#ifdef __aarch64__
  a_crash();
  return true;
#else
  return false;
#endif
}

/* Only crash if we're executing on powerpc64 */
bool __crash_powerpc64()
{
#ifdef __powerpc64__
  a_crash();
  return true;
#else
  return false;
#endif
}

/* Only crash if we're executing on x86-64 */
bool __crash_x86_64()
{
#ifdef __x86_64__
  a_crash();
  return true;
#else
  return false;
#endif
}

weak_alias(__crash_aarch64, crash_aarch64);
weak_alias(__crash_powerpc64, crash_powerpc64);
weak_alias(__crash_x86_64, crash_x86_64);
