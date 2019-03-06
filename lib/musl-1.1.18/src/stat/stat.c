#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

int stat(const char *restrict path, struct stat *restrict buf)
{
	int ret;
	union stat_union stu;
#ifdef SYS_stat
	ret = syscall(SYS_stat, path, &stu);
#else
	ret = syscall(SYS_fstatat, AT_FDCWD, path, &stu, 0);
#endif
	translate_stat(buf, &stu);
	return ret;
}

LFS64(stat);
