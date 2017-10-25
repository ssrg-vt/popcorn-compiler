/*
 * This file includes configuration & implementation details internal
 * to the migration library.  It should *not* be exported outside!
 */

#ifndef _CONFIG_H
#define _CONFIG_H

/* Architecture-specific includes. */
#ifdef __aarch64__
# include <arch/aarch64/migrate.h>
#elif defined __powerpc64__
# include <arch/powerpc64/migrate.h>
#elif defined __x86_64__
# include <arch/x86_64/migrate.h>
#else
# error Unknown/unsupported architecture!
#endif

/*
 * Calculate time between when threads are signalled to migrate and when they
 * enter the migration library.
 */
#define _TIME_RESPONSE_DELAY 1

/* Time how long it takes the stack transformation library to do its thing. */
#define _TIME_REWRITE 0

/* Use environment variables to specify at which function to migrate. */
#define _ENV_SELECT_MIGRATE 0

/* Use signals to trigger thread migrations. */
#define _SIG_MIGRATION 1
#if _SIG_MIGRATION == 1
# include <signal.h>
# define MIGRATE_SIGNAL SIGRTMIN
#endif

/* Maximum number of nodes supported by the operating system. */
#define MAX_POPCORN_NODES 32

/*
 * Debug the migration process by spinning on the destination post-migration.
 * Allows the user to attach a debugger to the process and resume exeuction.
 *
 * Note: to resume execution on remote, set variable "__hold" to be zero.
 */
#define _DEBUG 0

#endif /* _CONFIG_H */

