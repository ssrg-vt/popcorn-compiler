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
 * Instead of a heterogeneous rewrite/migration, do a homogeneous rewrite
 * (src ISA = dest ISA) and simulate a migration.  Useful for debugging.
 */
#ifndef _NATIVE
#define _NATIVE 0
#endif

/*
 * Calculate time between when threads are signalled to migrate and when they
 * enter the migration library.
 */
#ifndef _TIME_RESPONSE_DELAY
#define _TIME_RESPONSE_DELAY 0
#endif

/* Time how long it takes the stack transformation library to do its thing. */
#ifndef _TIME_REWRITE
#define _TIME_REWRITE 0
#endif

/* Use environment variables to specify at which function to migrate. */
#ifndef _ENV_SELECT_MIGRATE
#define _ENV_SELECT_MIGRATE 0
#endif

/* Use signals to trigger thread migrations.  If set, which signal to use. */
#ifndef _SIG_MIGRATION
#define _SIG_MIGRATION 0
#endif

#if _SIG_MIGRATION == 1
# include <signal.h>
# define MIGRATE_SIGNAL SIGRTMIN
#endif

/*
 * Debug the migration process by spinning on the destination post-migration.
 * Allows the user to attach a debugger to the process and resume exeuction.
 *
 * Note: to resume execution on remote, set variable "__hold" to be zero.
 */
#ifndef _DEBUG
#define _DEBUG 0
#endif

#endif /* _CONFIG_H */

