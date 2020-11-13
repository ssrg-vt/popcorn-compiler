#include <sys/stat.h>
#include "syscall.h"
#include "libc.h"

void copy_stat(struct stat *st, struct stat_internal *sti);

int fstatat(int fd, const char *restrict path, struct stat *restrict buf, int flag)
{
	struct stat_internal sti;
	int ret = syscall(SYS_fstatat, fd, path, &sti, flag);
	copy_stat(buf, &sti);
	return ret;
}

LFS64(fstatat);
