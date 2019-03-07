#ifndef _BIT_STAT_H_
#define _BIT_STAT_H_

/* structure used by the user-space */
struct stat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	blksize_t st_blksize;
	blkcnt_t st_blocks;
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
};

struct stat_aarch64 {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	unsigned long __pad;
	off_t st_size;
	blksize_t st_blksize;
	int __pad2;
	blkcnt_t st_blocks;
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	unsigned __unused[2];
};

struct stat_x86_64 {
	dev_t st_dev;
	ino_t st_ino;
	nlink_t st_nlink;

	mode_t st_mode;
	uid_t st_uid;
	gid_t st_gid;
	unsigned int    __pad0;
	dev_t st_rdev;
	off_t st_size;
	blksize_t st_blksize;
	blkcnt_t st_blocks;

	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	long __unused[3];
};

union stat_union {
	struct stat_x86_64 x86_64;
	struct stat_aarch64 aarch64;
};


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

void translate_stat(struct stat *st, union stat_union *stu);

#endif

