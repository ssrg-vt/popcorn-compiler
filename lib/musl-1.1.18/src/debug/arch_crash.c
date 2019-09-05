#include <stdbool.h>
#include "atomic.h"
#include "libc.h"

/* Only crash if we're executing on aarch64 */
bool __crash_aarch64(long a, long b, long c, long d)
{
#ifdef __aarch64__
  __asm__ __volatile__("mov x0, %0;"
                       "mov x1, %1;"
                       "mov x2, %2;"
                       "mov x3, %3;"
                       "mov x4, xzr; ldr x4, [x4]" ::
    "r"(a), "r"(b), "r"(c), "r"(d) : "x0", "x1", "x2", "x3", "x4");
  return true;
#else
  return false;
#endif
}

/* Only crash if we're executing on powerpc64 */
bool __crash_powerpc64(long a, long b, long c, long d)
{
#ifdef __powerpc64__
  __asm__ __volatile__("mr 0, %0;"
                       "mr 1, %1;"
                       "mr 2, %2;"
                       "mr 3, %3;"
                       ".long 0" ::
    "r"(a), "r"(b), "r"(c), "r"(d) : "r0", "r1", "r2", "r3");
  return true;
#else
  return false;
#endif
}

// CJP: FIXME
/* Only crash if we're executing on riscv64 */
bool __crash_riscv64(long a, long b, long c, long d)
{
#ifdef __riscv64__
  __asm__ __volatile__("addi x1, %0, 0;"
                       "addi x2, %1, 0;"
                       "addi x3, %2, 0;"
                       "addi x4, %3, 0;" ::
    "r"(a), "r"(b), "r"(c), "r"(d) : "x1", "x2", "x3", "x4");
  return true;
#else
  return false;
#endif
}

/* Only crash if we're executing on x86-64 */
bool __crash_x86_64(long a, long b, long c, long d)
{
#ifdef __x86_64__
  __asm__ __volatile__("mov %0, %%rax;"
                       "mov %1, %%rbx;"
                       "mov %2, %%rcx;"
                       "mov %3, %%rdx;"
                       "hlt" ::
    "r"(a), "r"(b), "r"(c), "r"(d) : "rax", "rbx", "rcx", "rdx");
  return true;
#else
  return false;
#endif
}

weak_alias(__crash_aarch64, crash_aarch64);
weak_alias(__crash_powerpc64, crash_powerpc64);
weak_alias(__crash_riscv64, crash_riscv64);
weak_alias(__crash_x86_64, crash_x86_64);

bool __crash(long a, long b, long c, long d)
{
#if defined __aarch64__
  return __crash_aarch64(a, b, c, d);
#elif defined __powerpc64__
  return __crash_powerpc64(a, b, c, d);
#elif defined __riscv64
  return __crash_riscv64(a, b, c, d);
#else /* __x86_64__ */
  return __crash_x86_64(a, b, c, d);
#endif
}

weak_alias(__crash, crash);

