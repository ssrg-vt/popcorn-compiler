#include <unistd.h>
#include "syscall.h"
#include "libc.h"

ssize_t __pread(int fd, void *buf, size_t size, off_t ofs)
{
	return syscall_cp(SYS_pread, fd, buf, size, __SYSCALL_LL_O(ofs));
}

LFS64(__pread);
weak_alias(__pread, pread);
