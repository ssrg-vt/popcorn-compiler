#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

void __procfdname(char *, unsigned);

void copy_stat(struct stat *st, struct stat_internal *sti)
{
	st->st_dev = sti->st_dev;
	st->st_ino = sti->st_ino;
	st->st_nlink = sti->st_nlink;
	st->st_mode = sti->st_mode;
	st->st_uid = sti->st_uid;
	st->st_gid = sti->st_gid;
	st->st_rdev = sti->st_rdev;
	st->st_size = sti->st_size;
	st->st_blksize = sti->st_blksize;
	st->st_blocks = sti->st_blocks;
	st->st_atim = sti->st_atim;
	st->st_mtim = sti->st_mtim;
	st->st_ctim = sti->st_ctim;
}

int fstat(int fd, struct stat *st)
{
	struct stat_internal sti;
	int ret = __syscall(SYS_fstat, fd, &sti);
	copy_stat (st, &sti);
	if (ret != -EBADF || __syscall(SYS_fcntl, fd, F_GETFD) < 0)
		return __syscall_ret(ret);
	char buf[15+3*sizeof(int)];
	__procfdname(buf, fd);
#ifdef SYS_stat
	ret = syscall(SYS_stat, buf, &sti);
	copy_stat (st, &sti);
	return ret;
#else
	ret = syscall(SYS_fstatat, AT_FDCWD, buf, &sti, 0);
	copy_stat (st, &sti);
	return ret;
#endif
}

LFS64(fstat);
