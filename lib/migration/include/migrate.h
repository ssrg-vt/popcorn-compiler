#ifndef _MIGRATE_H
#define _MIGRATE_H

#if !defined(__aarch64__) && !defined(__powerpc64__) && !defined(__x86_64__)
# error Unknown/unsupported architecture!
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <sched.h>

/* Supported architectures */
enum arch {
  AARCH64 = 0,
  POWERPC64,
  X86_64,
  NUM_ARCHES
};

/**
 * Get source node identification to assign an id for migration_pair -- I assumed the return of int for now
 */
int get_src_arch();

/**
 * Get destination node identification to assign an id for migration_pair -- I assumed the return of int for now
 */
int get_dest_arch();

/**
 * Get the CPUs associated with a given architecture in the system.
 * @param ar the architecture
 * @return the CPUs for the architecture
 */
cpu_set_t arch_to_cpus(enum arch ar);

/**
 * Get the current architecture.
 * @return the architecture on which we're executing
 */
cpu_set_t current_arch();

/**
 * Select a destination architecture for migration.
 * @return the architecture on which to migrate
 */
cpu_set_t select_arch();

/**
 * Check if thread should migrate, and if so, invoke migration.  The optional
 * callback function will be invoked before execution resumes on destination
 * architecture.
 *
 * @param callback a callback function to be invoked before execution resumes
 *                 on destination architecture
 * @param callback_data data to be passed to the callback function
 */
void check_migrate(void (*callback)(void*), void *callback_data);

/**
 * Migrate thread.  The optional callback function will be invoked before
 * execution resumes on destination architecture.
 *
 * @param callback a callback function to be invoked before execution resumes
 *                 on destination architecture
 * @param callback_data data to be passed to the callback function
 */
void migrate(void (*callback)(void*), void *callback_data);

/**
 * Register a function to be used for migration points inserted by
 * -finstrument-functions.
 *
 * Note: does not apply to direct calls to migrate_shim().
 *
 * @param callback a callback function to be invoked before execution resumes
 *                 on destination architecture
 * @param callback_data data to be passed to the callback function
 */
void register_migrate_callback(void (*callback)(void*), void *callback_data);

#ifdef __cplusplus
}
#endif

#endif /* _MIGRATE_H */

