#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

void copy_stat(struct stat *st, struct stat_internal *sti);

int lstat(const char *restrict path, struct stat *restrict buf)
{
	struct stat_internal sti;
	int ret;

#ifdef SYS_lstat
	ret = syscall(SYS_lstat, path, &sti);
#else
	ret = syscall(SYS_fstatat, AT_FDCWD, path, &sti, AT_SYMLINK_NOFOLLOW);
#endif
	copy_stat(buf, &sti);
	return ret;
}

LFS64(lstat);
