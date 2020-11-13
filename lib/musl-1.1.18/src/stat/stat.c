#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

void copy_stat(struct stat *st, struct stat_internal *sti);

int stat(const char *restrict path, struct stat *restrict buf)
{
	struct stat_internal sti;
	int ret;

#ifdef SYS_stat
	ret = syscall(SYS_stat, path, buf);
#else
	ret = syscall(SYS_fstatat, AT_FDCWD, path, buf, 0);
#endif

	copy_stat(buf, &sti);
	return ret;
}

LFS64(stat);
