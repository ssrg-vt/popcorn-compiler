#define _BSD_SOURCE
#include <sys/uio.h>
#include <unistd.h>
#include "syscall.h"
#include "libc.h"

ssize_t __pwritev(int fd, const struct iovec *iov, int count, off_t ofs)
{
	return syscall_cp(SYS_pwritev, fd, iov, count,
		(long)(ofs), (long)(ofs>>32));
}

LFS64(__pwritev);
weak_alias(__pwritev, pwritev);
