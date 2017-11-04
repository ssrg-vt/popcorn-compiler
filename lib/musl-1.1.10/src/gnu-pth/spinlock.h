#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "atomic.h"

typedef int pthread_spinlock_t;

static int pthread_spin_init(pthread_spinlock_t *s, int shared)
{
	return *s = 0;
}

int pthread_spin_destroy(pthread_spinlock_t *s)
{
	return 0;
}

static inline int pthread_spin_trylock(pthread_spinlock_t *s)
{
        return a_cas(s, 0, EBUSY);
}

static int pthread_spin_lock(pthread_spinlock_t *s)
{
	while (*(volatile int *)s || a_cas(s, 0, EBUSY)) a_spin();
	return 0;
}

static inline int pthread_spin_unlock(pthread_spinlock_t *s)
{
	a_store(s, 0);
	return 0;
}


#endif
