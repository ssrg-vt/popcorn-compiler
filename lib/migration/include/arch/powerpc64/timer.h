/*
 * Use POWER8's timebase facility (defined in the ISA manual).
 */

#ifndef _PPC64_TIMER_H
#define _PPC64_TIMER_H

#include <float.h>

/* Conversion factor from timebase values to nanoseconds. */
static double __cyc2ns = DBL_MAX;

/*
 * The kernel exposes the timebase update frequency through /proc/cpuinfo.
 * Adapted from code at https://pastebin.com/cHHgaV8k
 */
// Note: the timebase update frequency can be changed by the OS, but we'll
// assume it's constant for now.
static void __attribute__((constructor)) __get_timebase_freq()
{
  unsigned long long v;
  char line[1024], *p, *end;
  FILE *cpuinfo;

  cpuinfo = fopen("/proc/cpuinfo", "r");
  if(!cpuinfo)
  {
    perror("Could not read /proc/cpuinfo");
    return;
  }

  while(fgets(line, sizeof(line), cpuinfo))
  {
    if(strncmp(line, "timebase", 8) == 0 && (p = strchr(line, ':')))
    {
      v = strtoul(p + 1, &end, 0);
      if(end != p + 1)
      {
        __cyc2ns = (double)v / 1e9;
        break;
      }
    }
  }

  fclose(cpuinfo);
}

#define TIMESTAMP( ts ) asm volatile("mftb %0" : "=r" (ts))

#define TIMESTAMP_DIFF( start, end ) \
  (unsigned long long)((double)(end - start) / __cyc2ns)

#endif /* _PPC_TIMER_H */

