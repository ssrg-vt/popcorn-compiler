#include <sys/stat.h>
#include <translated/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

void __procfdname(char *, unsigned);

int fstat(int fd, struct stat *st)
{
	struct stat_specific sts;
	int ret = __syscall(SYS_fstat, fd, &sts);
	/* Copy values over from arch specific stat to generic stat so include
	 * from application side can use the translation layer irrespective of
	 * architecture. There may be a better more generic way of doing this
	 * but doing a direct member to member copy will suffice for now. */
	st->st_dev = sts.st_dev;
	st->st_ino = sts.st_ino;
	st->st_mode = sts.st_mode;
	st->st_nlink = sts.st_nlink;
	st->st_uid = sts.st_uid;
	st->st_gid = sts.st_gid;
	st->st_rdev = sts.st_rdev;
	st->st_size = sts.st_size;
	st->st_blksize = sts.st_blksize;
	st->st_blocks = sts.st_blocks;
	st->st_atim = sts.st_atim;
	st->st_mtim = sts.st_mtim;
	st->st_ctim = sts.st_ctim;
	if (ret != -EBADF || __syscall(SYS_fcntl, fd, F_GETFD) < 0)
		return __syscall_ret(ret);

	char buf[15+3*sizeof(int)];
	__procfdname(buf, fd);
#ifdef SYS_stat
	return syscall(SYS_stat, buf, st);
#else
	return syscall(SYS_fstatat, AT_FDCWD, buf, st, 0);
#endif
}

LFS64(fstat);
