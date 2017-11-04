/*
**  GNU Pth - The GNU Portable Threads
**  Copyright (c) 1999-2006 Ralf S. Engelschall <rse@engelschall.com>
**
**  This file is part of GNU Pth, a non-preemptive thread scheduling
**  library which can be found at http://www.gnu.org/software/pth/.
**
**  This library is free software; you can redistribute it and/or
**  modify it under the terms of the GNU Lesser General Public
**  License as published by the Free Software Foundation; either
**  version 2.1 of the License, or (at your option) any later version.
**
**  This library is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**  Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License along with this library; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
**  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
**
**  pth_syscall.c: Pth direct syscall support
*/
                             /* ``Free Software: generous programmers
                                  from around the world all join
                                  forces to help you shoot yourself
                                  in the foot for free.''
                                                 -- Unknown         */
/*
 * Prevent system includes from declaring the syscalls in order to avoid
 * prototype mismatches. In theory those mismatches should not happen
 * at all, but slight (but still compatible) differences (ssize_t vs.
 * size_t, etc) can lead to a compile-time failure (although run-time
 * would be ok). Hence protect ourself from this situation.
 */
#define fork          __pth_sys_fork
#define waitpid       __pth_sys_waitpid
#define system        __pth_sys_system
#define nanosleep     __pth_sys_nanosleep
#define usleep        __pth_sys_usleep
#define sleep         __pth_sys_sleep
#define sigprocmask   __pth_sys_sigmask
#define sigwait       __pth_sys_sigwait
#define select        __pth_sys_select
#define pselect       __pth_sys_pselect
#define poll          __pth_sys_poll
#define connect       __pth_sys_connect
#define accept        __pth_sys_accept
#define read          __pth_sys_read
#define write         __pth_sys_write
#define readv         __pth_sys_readv
#define writev        __pth_sys_writev
#define recv          __pth_sys_recv
#define send          __pth_sys_send
#define recvfrom      __pth_sys_recvfrom
#define sendto        __pth_sys_sendto
#define pread         __pth_sys_pread
#define pwrite        __pth_sys_pwrite

/* include the private header and this way system headers */
#include "pth_p.h"

#if cpp
#define pth_sc(func) pth_sc_##func
#endif /* cpp */

/*
 * Unprotect us from the namespace conflict with the
 * syscall prototypes in system headers.
 */
#undef fork
#undef waitpid
#undef system
#undef nanosleep
#undef usleep
#undef sleep
#undef sigprocmask
#undef sigwait
#undef select
#undef pselect
#undef poll
#undef connect
#undef accept
#undef recv
#undef send
#undef recvfrom
#undef sendto
#undef read
#undef write
#undef readv
#undef writev
#undef pread
#undef pwrite



/* syscall wrapping initialization */
intern void pth_syscall_init(void)
{
	return;
}

/* syscall wrapping initialization */
intern void pth_syscall_kill(void)
{
	return;
}
#if 0
/* ==== Pth hard syscall wrapper for fork(2) ==== */
pid_t fork(void);
pid_t fork(void)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_fork();
}
#endif
intern pid_t pth_sc_fork(void)
{
    return -1;
}

#if 0
/* ==== Pth hard syscall wrapper for nanosleep(3) ==== */
int nanosleep(const struct timespec *, struct timespec *);
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_nanosleep(rqtp, rmtp);
}
#endif
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_nanosleep necessary! */

/* ==== Pth hard syscall wrapper for usleep(3) ==== */
int usleep(unsigned int);
int usleep(unsigned int sec)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_usleep(sec);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_usleep necessary! */

/* ==== Pth hard syscall wrapper for sleep(3) ==== */
unsigned int sleep(unsigned int);
unsigned int sleep(unsigned int sec)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_sleep(sec);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_sleep necessary! */

/* ==== Pth hard syscall wrapper for system(3) ==== */
int system(const char *);
int system(const char *cmd)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_system(cmd);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_system necessary! */

#if 0
/* ==== Pth hard syscall wrapper for sigprocmask(2) ==== */
int sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_sigmask(how, set, oset);
}
#endif
int sigprocmask(int, const sigset_t *, sigset_t *);
intern int pth_sc_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
    return sigprocmask(how, set, oset);
}

#if 0
/* ==== Pth hard syscall wrapper for sigwait(3) ==== */
int sigwait(const sigset_t *, int *);
int sigwait(const sigset_t *set, int *sigp)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_sigwait(set, sigp);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_sigwait necessary! */

/* ==== Pth hard syscall wrapper for waitpid(2) ==== */
pid_t waitpid(pid_t, int *, int);
pid_t waitpid(pid_t wpid, int *status, int options)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_waitpid(wpid, status, options);
}
#endif
intern pid_t pth_sc_waitpid(pid_t wpid, int *status, int options)
{
    return -1;
}

#if 0
/* ==== Pth hard syscall wrapper for connect(2) ==== */
int connect(int, const struct sockaddr *, socklen_t);
int connect(int s, const struct sockaddr *addr, socklen_t addrlen)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_connect(s, addr, addrlen);
}
#endif
intern int pth_sc_connect(int s, const struct sockaddr *addr, socklen_t addrlen)
{
    return -1;
}

#if 0
/* ==== Pth hard syscall wrapper for accept(2) ==== */
int accept(int, struct sockaddr *, socklen_t *);
int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_accept(s, addr, addrlen);
}
#endif
intern int pth_sc_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return -1;
}

/* ==== Pth hard syscall wrapper for select(2) ==== */
int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#if 0
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_select(nfds, readfds, writefds, exceptfds, timeout);
}
#endif
intern int pth_sc_select(int nfds, fd_set *readfds, fd_set *writefds,
                         fd_set *exceptfds, struct timeval *timeout)
{
    select(nfds, readfds, writefds, exceptfds, timeout);
}

/* ==== Pth hard syscall wrapper for pselect(2) ==== */
int pselect(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *);
int pselect(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds,
            const struct timespec *ts, const sigset_t *mask)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_pselect(nfds, rfds, wfds, efds, ts, mask);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_pselect necessary! */

/* ==== Pth hard syscall wrapper for poll(2) ==== */
int poll(struct pollfd *, nfds_t, int);
int poll(struct pollfd *pfd, nfds_t nfd, int timeout)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_poll(pfd, nfd, timeout);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_poll necessary! */

/* ==== Pth hard syscall wrapper for recv(2) ==== */
ssize_t recv(int, void *, size_t, int);
#if 0
ssize_t recv(int fd, void *buf, size_t nbytes, int flags)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_recv(fd, buf, nbytes, flags);
}
#endif
intern ssize_t pth_sc_recv(int fd, void *buf, size_t nbytes, int flags)
{
    recv(fd, buf, nbytes, flags);
}

/* ==== Pth hard syscall wrapper for send(2) ==== */
ssize_t send(int, void *, size_t, int);
#if 0
ssize_t send(int fd, void *buf, size_t nbytes, int flags)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_send(fd, buf, nbytes, flags);
}
#endif
intern ssize_t pth_sc_send(int fd, void *buf, size_t nbytes, int flags)
{
    send(fd, buf, nbytes, flags);
}

#if 0
/* ==== Pth hard syscall wrapper for recvfrom(2) ==== */
ssize_t recvfrom(int fd, void *buf, size_t nbytes, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_recvfrom(fd, buf, nbytes, flags, from, fromlen);
}
#endif
ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
intern ssize_t pth_sc_recvfrom(int fd, void *buf, size_t nbytes, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    recvfrom(fd,buf,nbytes,flags,from,fromlen);
}

/* ==== Pth hard syscall wrapper for sendto(2) ==== */
#if 0
ssize_t sendto(int fd, const void *buf, size_t nbytes, int flags, const struct sockaddr *to, socklen_t tolen)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_sendto(fd, buf, nbytes, flags, to, tolen);
}
#endif
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
intern ssize_t pth_sc_sendto(int fd, const void *buf, size_t nbytes, int flags, const struct sockaddr *to, socklen_t tolen)
{
    sendto(fd, buf, nbytes, flags, to, tolen);
}

/* ==== Pth hard syscall wrapper for read(2) ==== */
ssize_t __read(int, void *, size_t);
ssize_t read(int fd, void *buf, size_t nbytes)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_read(fd, buf, nbytes);
}
intern ssize_t pth_sc_read(int fd, void *buf, size_t nbytes)
{
	__read(fd,buf,nbytes);
}

/* ==== Pth hard syscall wrapper for write(2) ==== */
ssize_t __write(int, const void *, size_t);
ssize_t write(int fd, const void *buf, size_t nbytes)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_write(fd, buf, nbytes);
}
intern ssize_t pth_sc_write(int fd, const void *buf, size_t nbytes)
{
	__write(fd, buf, nbytes);
}

/* ==== Pth hard syscall wrapper for readv(2) ==== */
ssize_t __readv(int, const struct iovec *, int);
ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_readv(fd, iov, iovcnt);
}
intern ssize_t pth_sc_readv(int fd, const struct iovec *iov, int iovcnt)
{
	__readv(fd,iov,iovcnt);
}

/* ==== Pth hard syscall wrapper for writev(2) ==== */
ssize_t __writev(int, const struct iovec *, int);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_writev(fd, iov, iovcnt);
}
intern ssize_t pth_sc_writev(int fd, const struct iovec *iov, int iovcnt)
{
	__writev(fd,iov,iovcnt);
}

/* ==== Pth hard syscall wrapper for pread(2) ==== */
ssize_t pread(int, void *, size_t, off_t);
ssize_t pread(int fd, void *buf, size_t nbytes, off_t offset)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_pread(fd, buf, nbytes, offset);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_pread necessary! */

/* ==== Pth hard syscall wrapper for pwrite(2) ==== */
ssize_t pwrite(int, const void *, size_t, off_t);
ssize_t pwrite(int fd, const void *buf, size_t nbytes, off_t offset)
{
    /* external entry point for application */
    pth_implicit_init();
    return pth_pwrite(fd, buf, nbytes, offset);
}
/* NOTICE: internally fully emulated, so still no
   internal exit point pth_sc_pwrite necessary! */

