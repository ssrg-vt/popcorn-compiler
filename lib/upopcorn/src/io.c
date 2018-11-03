#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "config.h"


int __open_(const char *filename, int flags, mode_t mode);
int open(const char *filename, int flags, ...)
{
	int ret;
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	ret = __open_(filename, flags, mode);
	//up_log("%s return error %d, errno %d\n", __func__, ret, errno);
	if(errno == EFAULT)
	{
		ret = strlen(filename);//touch the path. Can be done more efficiently: per-page or just ask dsm?
		up_log("%s touching the filename and retrying %d\n", __func__, ret);
		ret = __open_(filename, flags, mode);	
	}
	return ret;
}

/*
FILE *__fopen(const char *restrict filename, const char *restrict mode);
FILE *fopen(const char *restrict filename, const char *restrict mode)
{
	int ret;
	FILE* fr;
	fr=__fopen(filename, mode);
	up_log("%s return %p, errno %d\n", __func__, fr, errno);
	if((fr==NULL) && (errno==EFAULT))
	{
		ret = strlen(filename);//touch the path. Can be done more efficiently: per-page or just ask dsm?
		up_log("%s touching the filename and retrying %d\n", __func__, ret);
		fr = __fopen(filename, mode);	
	}
	return fr;
}*/
