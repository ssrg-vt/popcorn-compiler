#include <unistd.h>
#include "syscall.h"
#include "libc.h"

ssize_t __write(int fd, const void *buf, size_t count)
{
	return syscall_cp(SYS_write, fd, buf, count);
}
weak_alias(__write, write);
