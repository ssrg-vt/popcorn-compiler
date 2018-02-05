#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <migrate.h>
#include <stdarg.h>

#define __before_io() int __pio_cn = current_nid(); migrate(get_origin_nid(), NULL, NULL)
#define __after_io()  migrate(__pio_cn, NULL, NULL)

int open(const char *filename, int flags, ...)
{
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	__before_io();
	int ret = __open_(filename, flags, mode);;
	__after_io();
	return ret;
}

int close(int fd)
{
	__before_io();
	int ret = __close(fd);;
	__after_io();
	return ret;
}

int creat(const char *filename, mode_t mode)
{
	__before_io();
	int ret = __creat(filename, mode);;
	__after_io();
	return ret;
}

inline ssize_t pread(int fd, void *buf, size_t size, off_t ofs) 
{ 
	
	__before_io();
	ssize_t ret = __pread(fd, buf, size, ofs);;
	__after_io();
	return ret;
}
inline ssize_t preadv(int fd, const struct iovec *iov, int count, off_t ofs) 
{ 
	
	__before_io();
	ssize_t ret = __preadv(fd, iov, count, ofs);
	__after_io();
	return ret;
}
inline ssize_t pwrite(int fd, const void *buf, size_t size, off_t ofs) 
{ 
	
	__before_io();
	ssize_t ret = __pwrite(fd, buf, size, ofs);
	__after_io();
	return ret;
}
inline ssize_t pwritev(int fd, const struct iovec *iov, int size, off_t ofs) 
{
	__before_io();
	ssize_t ret = __pwritev(fd, iov, size, ofs);
	__after_io();
	return ret;
}
inline ssize_t read(int fd, void *buf, size_t count) 
{
	__before_io();
	ssize_t ret = __read(fd, buf, count);
	__after_io();
	return ret;
}
inline ssize_t readv(int fd, const struct iovec *iov, int count) 
{
	__before_io();
	ssize_t ret = __readv(fd, iov, count);
	__after_io();
	return ret;
}
inline ssize_t write(int fd, const void *buf, size_t count) 
{ 
	__before_io();
	ssize_t ret = __write(fd, buf, count);
	__after_io();
	return ret;
}
inline ssize_t writev(int fd, const struct iovec *iov, int count) 
{
	__before_io();
	ssize_t ret = __writev(fd, iov, count);
	__after_io();
	return ret;
}

