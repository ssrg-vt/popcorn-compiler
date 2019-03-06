#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

int lstat(const char *restrict path, struct stat *restrict buf)
{
	int ret;
	union stat_union stu;
#ifdef SYS_lstat
	ret = syscall(SYS_lstat, path, &stu);
#else
	ret = syscall(SYS_fstatat, AT_FDCWD, path, &stu, AT_SYMLINK_NOFOLLOW);
#endif
	translate_stat(buf, &stu);
	return ret;
}

LFS64(lstat);
