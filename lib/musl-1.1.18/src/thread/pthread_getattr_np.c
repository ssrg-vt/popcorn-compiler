#define _GNU_SOURCE
#include "pthread_impl.h"
#include "libc.h"
#include <sys/mman.h>

/* libstack_transformation requires an 8mb stack.  Rather than have
   musl probe for the proper stack size using unsupported mremap,
   hard-code the MAX_STACK_SIZE to 8MB.  */
#define MAX_STACK_SIZE (8UL * 1024UL * 1024UL)

int pthread_getattr_np(pthread_t t, pthread_attr_t *a)
{
	*a = (pthread_attr_t){0};
	a->_a_detach = !!t->detached;
	if (t->stack) {
		a->_a_stackaddr = (uintptr_t)t->stack;
		a->_a_stacksize = t->stack_size;
	} else {
		char *p = (void *)libc.auxv;
		size_t l = PAGE_SIZE;
		p += -(uintptr_t)p & PAGE_SIZE-1;
		a->_a_stackaddr = (uintptr_t)p;
		a->_a_stacksize = MAX_STACK_SIZE;
		while (mremap(p-l-PAGE_SIZE, PAGE_SIZE, 2*PAGE_SIZE, 0)==MAP_FAILED && errno==ENOMEM)
			l += PAGE_SIZE;
		a->_a_stacksize = l;
	}
	return 0;
}
