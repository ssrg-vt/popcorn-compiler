#ifndef _MIGRATE_H
#define _MIGRATE_H

#if !defined __aarch64__ && !defined __x86_64__
# error Unknown/unsupported architecture!
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <sched.h>

#define MAX_POPCORN_NODES 32

/* Supported architectures */
enum arch {
  ARCH_UNKNOWN = -1,
  ARCH_AARCH64 = 0,
  ARCH_X86_64,
  NUM_ARCHES,
};

/**
 * Get the current architecture.
 * @return the architecture on which we're executing
 */
enum arch current_arch(void);

/**
 * Get the current node id.
 * @return the node id on which this thread is running
 */
int current_nid(void);


/**
 * Get the origin node id.
 * @return the node id on which this thread is running
 */
int get_origin_nid(void)

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
void migrate(int nid, void (*callback)(void*), void *callback_data);

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

