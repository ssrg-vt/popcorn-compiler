#include <unistd.h>
#include "syscall.h"
#include "libc.h"

ssize_t __read(int fd, void *buf, size_t count)
{
	return syscall_cp(SYS_read, fd, buf, count);
}
weak_alias(__read, read);
