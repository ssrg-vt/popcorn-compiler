#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include "syscall.h"


int epoll_create(int size)
{
	return epoll_create1(0);
}

int epoll_create1(int flags)
{
	int r = __syscall(SYS_epoll_create1, flags);
#ifdef SYS_epoll_create
	if (r==-ENOSYS && !flags) r = __syscall(SYS_epoll_create, 1);
#endif
	return __syscall_ret(r);
}

int epoll_ctl(int fd, int op, int fd2, struct epoll_event *ev)
{
	union epoll_event_union uee;
	translate_epoll_event_rev(ev, &uee);
	int ret= syscall(SYS_epoll_ctl, fd, op, fd2, &(uee. epoll_arch));
	translate_epoll_event(ev, &uee);

	return ret;
}

int epoll_pwait(int fd, struct epoll_event *ev, int cnt, int to, const sigset_t *sigs)
{
	union epoll_event_union uee;
	translate_epoll_event_rev(ev, &uee);
	int r = __syscall(SYS_epoll_pwait, fd, &(uee. epoll_arch), cnt, to, sigs, _NSIG/8);
#ifdef SYS_epoll_wait
	if (r==-ENOSYS && !sigs) r = __syscall(SYS_epoll_wait, fd, &(uee. epoll_arch), cnt, to);
#endif
	translate_epoll_event(ev, &uee);
	return __syscall_ret(r);
}

int epoll_wait(int fd, struct epoll_event *ev, int cnt, int to)
{
	return epoll_pwait(fd, ev, cnt, to, 0);
}
