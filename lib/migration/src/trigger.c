#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "config.h"

#if _SIG_MIGRATION == 1

/* Flag set by signal handler indicating a thread should migrate. */
// TODO this should be thread-specific
volatile int __migrate_flag = -1;

#if _TIME_RESPONSE_DELAY == 1

#include "timer.h"

/*
 * Statistics about number of times a migration was signalled and the time
 * between when the migration was signalled and when the thread reached the
 * migration library.
 */
// Note: we don't expect to be signalled *that* much during a single
// application's execution, so only maintain 1024 timings.
static volatile unsigned long num_triggers = 0;
static unsigned long long response_timings[1024] = { 0L }; // In nanoseconds

/*
 * Starting (architecture-specific) timestamp set when a thread executes the
 * migration request signal handler.
 */
// TODO needs to be thread-specific
static unsigned long long start = UINT64_MAX;

/* Output response time statistics. */
// Note: destructor should only be called by one thread and is therefore
// thread-safe.
static void __attribute__((destructor)) __print_response_timing()
{
  unsigned long i;
  printf("Number of migration triggers: %lu\nResponse times:\n", num_triggers);
  for(i = 0; i < num_triggers; i++) printf("  %llu ns\n", response_timings[i]);
}

#endif /* _TIME_RESPONSE_DELAY */

/*
 * Reset the migrate flag when the thread has entered the migration library to
 * avoid continuously attempting migration.
 */
void clear_migrate_flag()
{
#if _TIME_RESPONSE_DELAY == 1
  unsigned long end, idx;
  if(start != UINT64_MAX)
  {
    TIMESTAMP(end);
    idx = __sync_fetch_and_add(&num_triggers, 1);
    if(idx >= 1024) fprintf(stderr, "WARNING: too many response timings!\n");
    else response_timings[idx] = TIMESTAMP_DIFF(start, end);
    start = UINT64_MAX;
  }
  else fprintf(stderr, "WARNING: no starting time stamp");
#endif

  __migrate_flag = -1;
}

/*
 * Handle OS thread migration request.  Communicate to a thread that it should
 * begin the migration process to another node.
 */
static void __migrate_sighandler(int sig, siginfo_t *info, void *args)
{
  // Avoid accidentally triggering this again, which can screw up calculating
  // migration response time.
  if(__migrate_flag >= 0) return;

#if _TIME_RESPONSE_DELAY == 1
  TIMESTAMP(start);
#endif
  // The compiler can instrument code so that threads check this flag during
  // execution to decide whether to call into the migration library.
  // TODO this needs to be set according to the what the OS tells us (maybe in
  // the args argument?)
  __migrate_flag = 1;

  // Tell the OS we're requesting this thread migrate.
  // TODO in the real version, the OS should *know* that the thread is to be
  // migrated and does not need to be told.
  if(syscall(SYSCALL_PROPOSE_MIGRATION, 0, 1))
    perror("Could not propose the migration destination for the thread");
}

/*
 * Register a signal handler to be called when the OS wants a thread to migrate
 * to a new architecture.  
 */
static void __attribute__((constructor)) __register_migrate_sighandler()
{
  struct sigaction sa;
  sa.sa_handler = NULL;
  sa.sa_sigaction = __migrate_sighandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sa.sa_restorer = NULL;
  if(sigaction(MIGRATE_SIGNAL, &sa, NULL))
    perror("Could not register migration trigger signal handler");
}

#endif /* _SIG_MIGRATION */

