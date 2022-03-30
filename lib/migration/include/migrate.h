#ifndef _MIGRATE_H
#define _MIGRATE_H

#if !defined(__aarch64__) && !defined(__powerpc64__) && !defined(__x86_64__)
# error Unknown/unsupported architecture!
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* _MIGRATE_H */

