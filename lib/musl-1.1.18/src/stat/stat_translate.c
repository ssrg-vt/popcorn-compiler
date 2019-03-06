#include <sys/stat.h>
#include <stdio.h>

/* Copy values over from arch specific stat to generic stat so include
 * from application side can use the translation layer irrespective of
 * architecture. There may be a better more generic way of doing this
 * but doing a direct member to member copy will suffice for now. */
#ifdef __x86_64__
	#define carch x86_64
#elif __aarch64__
	#define carch aarch64
#else
	#error "usuported architecture"
#endif

void translate_stat(struct stat *st, union stat_union *stu)
{
	st->st_dev = stu->carch.st_dev;
	st->st_ino = stu->carch.st_ino;
	st->st_mode = stu->carch.st_mode;
	st->st_nlink = stu->carch.st_nlink;
	st->st_uid = stu->carch.st_uid;
	st->st_gid = stu->carch.st_gid;
	st->st_rdev = stu->carch.st_rdev;
	st->st_size = stu->carch.st_size;
	st->st_blksize = stu->carch.st_blksize;
	st->st_blocks = stu->carch.st_blocks;
	st->st_atim = stu->carch.st_atim;
	st->st_mtim = stu->carch.st_mtim;
	st->st_ctim = stu->carch.st_ctim;
}
