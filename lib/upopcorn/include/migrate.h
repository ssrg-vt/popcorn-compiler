#ifndef _MIGRATE_H
#define _MIGRATE_H

#include <stack_transform.h>
#include "config.h"

#ifdef __x86_64__
#include "arch/x86_64/migrate.h"
#elif defined __aarch64__
#include "arch/aarch64/migrate.h"
#else
# error Unknown/unsupported architecture!
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <sched.h>
#endif

typedef union{
		struct regset_aarch64 aarch;
		struct regset_x86_64 x86;
} regs_t;


/**
 * Migrate thread.  The optional callback function will be invoked before
 * execution resumes on destination architecture.
 *
 * @param callback a callback function to be invoked before execution resumes
 *                 on destination architecture
 * @param callback_data data to be passed to the callback function
 */
//void new_migrate(int nid, void (*callback)(void*), void *callback_data);
void new_migrate(int nid);

int get_context(void **ctx, int *size);

#ifdef __cplusplus
}
#endif

#endif /* _MIGRATE_H */

