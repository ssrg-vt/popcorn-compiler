#include "pthread_impl.h"
#include <threads.h>
#include <pthread.h>
#include "libc.h"

void pthread_set_migrate_args(void *args)
{
  __pthread_self()->popcorn_migrate_args = args;
}

void *pthread_get_migrate_args()
{
  return __pthread_self()->popcorn_migrate_args;
}

