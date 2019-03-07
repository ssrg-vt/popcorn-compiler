#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

void __procfdname(char *, unsigned);

int fstat(int fd, struct stat *st)
{
	union stat_union stu;
	int ret = __syscall(SYS_fstat, fd, &stu. carch);
	if (ret != -EBADF || __syscall(SYS_fcntl, fd, F_GETFD) < 0)
	{
		translate_stat(st, &stu);
		return __syscall_ret(ret);
	}

	char buf[15+3*sizeof(int)];
	__procfdname(buf, fd);
#ifdef SYS_stat
	ret=syscall(SYS_stat, buf, &stu. carch);
#else
	ret=syscall(SYS_fstatat, AT_FDCWD, buf, &stu. carch, 0);
#endif
	translate_stat(st, &stu);
	return ret;
}

LFS64(fstat);
