#include <signal.h>
#include <errno.h>
#include "pthread_impl.h"

int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict old)
{
	int r = __pthread_sigmask(how, set, old);
	if (!r) return r;
	errno = r;
	return -1;
}
