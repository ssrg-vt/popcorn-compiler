#include <sys/uio.h>
#include "syscall.h"
#include "libc.h"

ssize_t __writev(int fd, const struct iovec *iov, int count)
{
	return syscall_cp(SYS_writev, fd, iov, count);
}
weak_alias(__writev, writev);
