#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"
#include "libc.h"

void __procfdname(char *, unsigned);

int fstat(int fd, struct stat *st)
{
	int ret = __syscall(SYS_fstat, fd, st);
	if (ret != -EBADF || __syscall(SYS_fcntl, fd, F_GETFD) < 0)
		return __syscall_ret(ret);

	char buf[15+3*sizeof(int)];
	__procfdname(buf, fd);
}

LFS64(fstat);
//#include <sys/stat.h>
//#ifdef __aarch64__
//#include <bits/stat_aarch64.h>
//#endif
//#include <errno.h>
//#include <fcntl.h>
//#include <string.h>
//#include "syscall.h"
//#include "libc.h"
//
//void __procfdname(char *, unsigned);
//
//int fstat(int fd, struct stat *st)
//{
//#ifdef __aarch64__
//	struct stat_aarch64 sts;
//	int ret = __syscall(SYS_fstat, fd, &sts);
//	/* Copy values over from arch specific stat to generic stat so include
//	 * from application side can use the translation layer irrespective of
//	 * architecture. There may be a better more generic way of doing this
//	 * but doing a direct member to member copy will suffice for now. */
//	st->st_dev = sts.st_dev;
//	st->st_ino = sts.st_ino;
//	st->st_mode = sts.st_mode;
//	st->st_nlink = sts.st_nlink;
//	st->st_uid = sts.st_uid;
//	st->st_gid = sts.st_gid;
//	st->st_rdev = sts.st_rdev;
//	st->st_size = sts.st_size;
//	st->st_blksize = sts.st_blksize;
//	st->st_blocks = sts.st_blocks;
//	st->st_atim = sts.st_atim;
//	st->st_mtim = sts.st_mtim;
//	st->st_ctim = sts.st_ctim;
//	if (ret != -EBADF || __syscall(SYS_fcntl, fd, F_GETFD) < 0)
//		return __syscall_ret(ret);
//
//	char buf[15+3*sizeof(int)];
//	__procfdname(buf, fd);
//#elif __x86_64__
//	struct stat st1;
//	int ret = __syscall(SYS_fstat, fd, &st1);
//	memcpy(st, &st1, sizeof(struct stat));
//	return __syscall_ret(ret);
//#endif
//	return __syscall(SYS_fstat, fd, st);
//}
//
//LFS64(fstat);
