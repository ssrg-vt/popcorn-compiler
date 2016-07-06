/*
 * Timer implementation.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/3/2016
 */

#include <stdio.h>

#define _NO_DECLARE_TIMERS
#include "config.h"
#include "timer.h"

#if _TIMER_SRC == CLOCK_GETTIME
# include <time.h>
#elif _TIMER_SRC == GETTIMEOFDAY
# include <sys/time.h>
#else
# error Unknown timer source!
#endif

///////////////////////////////////////////////////////////////////////////////
// Declarations & definitions
///////////////////////////////////////////////////////////////////////////////

/* Convert from library timer structures to raw nanoseconds */
#define TV_TO_NS( timeval ) (timeval.tv_sec * 1e9 + timeval.tv_usec * 1e3)
#define TS_TO_NS( timespec ) (timespec.tv_sec * 1e9 + timespec.tv_nsec)

/*
 * An timer structure encapsulating data required for accumulating the time
 * spent in an individual function.
 */
struct timer
{
  const char* const name;
  unsigned long num_timings;
  unsigned long start;
  unsigned long elapsed;
};

/*
 * A single struct type containing all timers, declared so that it's easy to
 * calculate how many there are.
 */
struct named_timers
{
#define X( timer_name ) struct timer timer_name;
ALL_TIMERS
#undef X
};
#define NUM_TIMERS (sizeof(struct named_timers) / sizeof(struct timer))

/* Declare a struct that allows timer access based on name or index. */
struct timers
{
  union {
    struct named_timers named;
    struct timer arr[NUM_TIMERS];
  };
};

/* Define timer storage. */
static struct timers timers = {
#define X( timer_name ) \
  .named.timer_name = \
  { \
    .name = #timer_name, \
    .num_timings = 0, \
    .start = 0, \
    .elapsed = 0 \
  },
ALL_TIMERS
#undef X
};

/* Define pointers used by others. */
#define X( timer_name ) timer __timer_##timer_name = &timers.named.timer_name;
ALL_TIMERS
#undef X

///////////////////////////////////////////////////////////////////////////////
// Timer API
///////////////////////////////////////////////////////////////////////////////

int timer_start(timer timer)
{
  int ret;
#if _TIMER_SRC == CLOCK_GETTIME
  struct timespec time;
  ret = clock_gettime(CLOCK_MONOTONIC, &time);
  if(ret == 0) timer->start = TS_TO_NS(time);
#else
  struct timeval time;
  ret = gettimeofday(&time, NULL);
  if(ret == 0) timer->start = TV_TO_NS(time);
#endif
  return ret;
}

int timer_stop_and_accum(timer timer)
{
  int ret;
  unsigned long end;
#if _TIMER_SRC == CLOCK_GETTIME
  struct timespec time;
  ret = clock_gettime(CLOCK_MONOTONIC, &time);
  end = TS_TO_NS(time);
#else
  struct timeval time;
  ret = gettimeofday(&time, NULL);
  end = TV_TO_NS(time);
#endif
  if(ret == 0) {
    timer->num_timings++;
    timer->elapsed += (end - timer->start);
    timer->start = 0;
  }
  return ret;
}

void timer_reset(timer timer)
{
  timer->num_timings = 0;
  timer->start = 0;
  timer->elapsed = 0;
}

unsigned long timer_get_elapsed(timer timer)
{
  return timer->elapsed;
}

void timer_print_all(void)
{
  int i;
  printf("[Timing] Elapsed time (%s):\n",
#if _TIMER_SRC == CLOCK_GETTIME
    "clock_gettime()"
#else
    "gettimeofday()"
#endif
  );
  for(i = 0; i < NUM_TIMERS; i++)
    printf("[Timing]   %s - %lu time(s) - %.3f us total, %.3f us average\n",
      timers.arr[i].name,
      timers.arr[i].num_timings,
      ((double)timers.arr[i].elapsed) / 1000.0,
      (((double)timers.arr[i].elapsed) / 1000.0) /
        (double)timers.arr[i].num_timings);
}

