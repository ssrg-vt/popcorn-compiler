#include "pthread_impl.h"
#include <threads.h>
#include <pthread.h>
#include "libc.h"

/* Popcorn migration data access */
void** pthread_migrate_args()
{
	struct pthread *self = __pthread_self();
	return &(self->popcorn_migrate_args);
}

