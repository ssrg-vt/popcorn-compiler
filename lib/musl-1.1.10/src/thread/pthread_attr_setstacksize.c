#include "pthread_impl.h"

int pthread_attr_setstacksize(pthread_attr_t *a, size_t size)
{
	if (size-PTHREAD_STACK_MIN > SIZE_MAX/4) return EINVAL;
	a->_a_stacksize = size - DEFAULT_STACK_SIZE;
	return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *a, size_t* base)
{
	a->_a_stackaddr = (unsigned long) base;
	return 0;
}
