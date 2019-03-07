#include <sys/stat.h>
#include "syscall.h"
#include "libc.h"

int fstatat(int fd, const char *restrict path, struct stat *restrict buf, int flag)
{
	union stat_union stu;
	int ret=syscall(SYS_fstatat, fd, path, &stu. carch, flag);
	translate_stat(buf, &stu);
	return ret;
}

LFS64(fstatat);
