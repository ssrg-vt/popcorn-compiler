/*
 * Timing infrastructure, including timer declarations/definitions & APIs.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/3/2016
 */

#ifndef _TIMER_H
#define _TIMER_H

///////////////////////////////////////////////////////////////////////////////
// Declarations & macros
///////////////////////////////////////////////////////////////////////////////

/* A simple timer struct which accumulates time spent in the function FUNC. */
typedef struct timer* timer;

/*
 * Runtime timers.  Coarse-grained timers are meant for timing of high-level
 * operations (e.g. popping a frame), while fine-grained timers are meant for
 * timing lower-level operations (e.g. reading a function's arguments and
 * locals).
 *
 * Coarse-grained timers are enabled with -D_TIMING, while fine-grained timers
 * are enabled with -D_FINE_GRAINED_TIMING.  Fine-grained timing also
 * *requires* -D_TIMING.
 *
 * To add a timer to the system, append an X( <timer name> ) to either of the
 * lists.
 */

/* Coarse-grained timers for high-level operations */
#define COARSE_TIMERS \
  X(st_init) \
  X(st_destroy) \
  X(st_rewrite_stack) \
  X(init_src_context) \
  X(init_dest_context) \
  X(unwind_and_size) \
  X(rewrite_stack) \
  X(free_context)

/* Fine-grained timers for timing of individual operations */
#define FINE_TIMERS \
  X(rewrite_frame) \
  X(pop_frame) \
  X(put_val) \
  X(get_site_by_addr) \
  X(get_site_by_id) \
  X(get_unwind_offset_by_addr)

/* All timers available to the runtime. */
#if defined(_TIMING) && defined(_FINE_GRAINED_TIMING)
# define ALL_TIMERS COARSE_TIMERS FINE_TIMERS
#elif defined (_TIMING)
// TODO disable fine timers
# define ALL_TIMERS COARSE_TIMERS FINE_TIMERS
#else
# define ALL_TIMERS
#endif

/* Declare all timers for use. */
#ifndef _NO_DECLARE_TIMERS
# define X( timer_name ) extern timer __timer_##timer_name;
ALL_TIMERS
#endif

/* Macros to control enabling/disabling timer function calls. */
#ifdef _TIMING
# define TIMER_START( timer_name ) st_timer_start(__timer_##timer_name)
# define TIMER_STOP( timer_name ) st_timer_stop_and_accum(__timer_##timer_name)
# define TIMER_RESET( timer_name ) st_timer_reset(__timer_##timer_name)
# define TIMER_ELAPSED( timer_name ) st_timer_get_elapsed(__timer_##timer_name)
# define TIMER_PRINT st_timer_print_all()
#else
# define TIMER_START( timer_name )
# define TIMER_STOP( timer_name )
# define TIMER_RESET( timer_name )
# define TIMER_ELAPSED( timer_name )
# define TIMER_PRINT
#endif /* _TIMING */

// TODO combine with TIMER_* macros above
#ifdef _FINE_GRAINED_TIMING
# define TIMER_FG_START( timer_name ) st_timer_start(__timer_##timer_name)
# define TIMER_FG_STOP( timer_name ) st_timer_stop_and_accum(__timer_##timer_name)
# define TIMER_FG_RESET( timer_name ) st_timer_reset(__timer_##timer_name)
# define TIMER_FG_ELAPSED( timer_name ) st_timer_get_elapsed(__timer_##timer_name)
# define TIMER_FG_PRINT st_timer_print_all()
#else
# define TIMER_FG_START( timer_name )
# define TIMER_FG_STOP( timer_name )
# define TIMER_FG_RESET( timer_name )
# define TIMER_FG_ELAPSED( timer_name )
# define TIMER_FG_PRINT
#endif /* _TIMING */

///////////////////////////////////////////////////////////////////////////////
// Timer API
///////////////////////////////////////////////////////////////////////////////

/*
 * Start a timer.
 *
 * @param timer a timer type pointer
 * @return 0 if the timer was successfully started, -1 otherwise
 */
int st_timer_start(timer timer);

/*
 * Stop a timer and accumulate the elapsed time.
 *
 * @param timer a timer type pointer
 * @return 0 if the timer was successfully stopped, -1 otherwise
 */
int st_timer_stop_and_accum(timer timer);

/*
 * Reset a timer to initial values.
 *
 * @param timer a timer type pointer
 */
void st_timer_reset(timer timer);

/*
 * Return the total elapsed time accumulated by the timer.
 *
 * @param timer a timer type pointer
 * @return the accumulated elapsed time, in nanoseconds
 */
unsigned long st_timer_get_elapsed(timer timer);

/*
 * Print elapsed time for all timers.
 */
void st_timer_print_all(void);

#endif /* _TIMER_H */

