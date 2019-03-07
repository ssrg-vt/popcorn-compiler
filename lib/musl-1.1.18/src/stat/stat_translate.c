#include <sys/stat.h>
#include <stdio.h>

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
	printf("%s: arch size %ld\n", __func__, sizeof(*stu));
	printf("%s: arch x86_64 size %ld\n", __func__, sizeof(stu->x86_64));
	printf("%s: arch x86_64 size %ld\n", __func__, sizeof(stu->aarch64));
	printf("%s: common size %ld\n", __func__, sizeof(*st));
}
