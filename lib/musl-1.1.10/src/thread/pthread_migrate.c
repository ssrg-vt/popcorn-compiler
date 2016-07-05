//#define _GNU_SOURCE
#include "pthread_impl.h"
#include <threads.h>
#include <pthread.h>
#include "libc.h"

#include <sched.h>

/* Popcorn migration data access */
void** pthread_migrate_args()
{
	//pthread_t self = __pthread_self();
	struct pthread *self = __pthread_self();
        return &(self->__args);
}

int* pthread_migrate_migration_phase()
{
	//not working???
	//pthread_t self = __pthread_self();
	struct pthread *self = __pthread_self();
        return &(self->__migration_phase);
}

int* pthread_migrate_fix_stack()
{
	struct pthread *self = __pthread_self();
	return &(self->__fix_stack);
}

#ifdef _GNU_SOURCE
typedef cpu_set_t mcpu_set_t;
#else
typedef pcpu_set_t mcpu_set_t;
#endif

mcpu_set_t* pthread_migrate_orig_cpus()
{
	//pthread_t self = __pthread_self();
	struct pthread *self = __pthread_self();
        return &(self->__orig_cpus);
}

mcpu_set_t* pthread_migrate_cpus()
{
	//pthread_t self = __pthread_self();
	struct pthread *self = __pthread_self();
        return &(self->__cpus);
}
