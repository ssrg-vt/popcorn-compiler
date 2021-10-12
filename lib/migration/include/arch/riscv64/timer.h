/*
 * Use riscv64's generic timer facility (defined in the arm documentation).
 */

#ifndef _RISCV64_TIMER_H
#define _RISCV64_TIMER_H

#define TIMESTAMP( ts ) asm volatile("mrs %0, cntvct_el0" : "=r" (ts))

#define TIMESTAMP_DIFF( start, end ) \
    ({ \
      unsigned long long freq, ns; \
      double cyc2ns; \
      asm volatile("mrs %0, cntfrq_el0" : "=r" (freq)); \
      cyc2ns = (double)freq / 1e9; \
      ns = (end - start) / cyc2ns; \
      ns; \
    })

#endif /* _RISCV64_TIMER_H */

