#include <sys/epoll.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include "syscall.h"

#ifdef __x86_64__
	#define arch_specific_struct epoll_event_x86_64
#else
	#define arch_specific_struct epoll_event_common
#endif

#ifndef NULL
#define NULL ((void*) 0)
#endif

#define MAX_EVENTS 32

//#define debug printf
#define debug(...) /*printf*/

static inline void translate_epoll_event(struct epoll_event *usr, struct arch_specific_struct *kus)
{
	if(!kus || !usr) return;
	usr->events=kus->events;
	usr->data=kus->data;
	debug("%s, %d: kus events %d, data->ptr %p; user events %d, data->ptr %p\n", kus->events, kus->data.ptr, usr->events, usr->data.ptr);
}

static inline void translate_epoll_event_rev(struct epoll_event *usr, struct arch_specific_struct *kus)
{
	if(!kus || !usr) return;
	kus->events=usr->events;
	kus->data=usr->data;
	debug("%s, %d: kus events %d, data->ptr %p; user events %d, data->ptr %p\n", kus->events, kus->data.ptr, usr->events, usr->data.ptr);
}


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
	struct arch_specific_struct __lev;
	struct arch_specific_struct *lev = &__lev;

	if(ev) translate_epoll_event_rev(ev, lev);
	else lev=NULL;

	debug("%s, %d: local struct size %ld, ptr %p, user struct ptr %p\n", __func__, __LINE__, sizeof(__lev), lev, ev);
	int ret= syscall(SYS_epoll_ctl, fd, op, fd2, lev);

	if(ev) translate_epoll_event(ev, lev);

	return ret;
}

//assumes that arch_specific_struct is smaller than user struct (epoll_event)
int epoll_pwait(int fd, struct epoll_event *ev, int cnt, int to, const sigset_t *sigs)
{
	//translate to arch specific
	struct arch_specific_struct lev[MAX_EVENTS];

	int r = __syscall(SYS_epoll_pwait, fd, &lev, MAX_EVENTS, to, sigs, _NSIG/8);
#ifdef SYS_epoll_wait
	if (r==-ENOSYS && !sigs) r = __syscall(SYS_epoll_wait, fd, &lev, MAX_EVENTS, to);
#endif

	r=__syscall_ret(r);
	//translate to user specific
	if(ev  &&  (r>0))
	{
		int i=0; for(;i<r; i++)
		{
			translate_epoll_event(&ev[i], &lev[i]);
		}
	}
	//done translating

	return r;
}

int epoll_wait(int fd, struct epoll_event *ev, int cnt, int to)
{
	return epoll_pwait(fd, ev, cnt, to, 0);
}
