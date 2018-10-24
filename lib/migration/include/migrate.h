#ifndef _MIGRATE_H
#define _MIGRATE_H

#if !defined(__aarch64__) && !defined(__x86_64__)
# error Unknown/unsupported architecture!
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return whether a node is available as a migration target.
 * @param nid the node ID
 * @return one if the node is available, or zero otherwise
 */
int node_available(int nid);

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
 * Migrate thread according to a thread schedule created by thread placement
 * analysis.  The optional callback function will be invoked before execution
 * resumes on destination architecture.
 *
 * @param region a region identifier used to look up a mapping for a particular
 *               application region
 * @param popcorn_tid a Popcorn-specific thread ID, returned by one of its
 *                    runtime systems
 * @param callback a callback function to be invoked before execution resumes
 *                 on destination architecture
 * @param callback_data data to be passed to the callback function
 */
void migrate_schedule(size_t region,
                      int popcorn_tid,
                      void (*callback)(void*),
                      void *callback_data);

#ifdef __cplusplus
}
#endif

#endif /* _MIGRATE_H */

