#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <migrate.h>
#include <stdarg.h>

int open(const char *filename, int flags, ...)
{
	mode_t mode = 0;

	printf("using uio layer open\n");

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return __open_(filename, flags, mode);
}

int close(int fd)
{
	printf("using uio layer close\n");
	return __close(fd);
}

int creat(const char *filename, mode_t mode)
{
	return __creat(filename, mode);
}

inline ssize_t pread(int fd, void *buf, size_t size, off_t ofs) 
{ 
	printf("using uio layer\n"); 
	return __pread(fd, buf, size, ofs);
}
inline ssize_t preadv(int fd, const struct iovec *iov, int count, off_t ofs) 
{ 
	printf("using uio layer\n"); 
	return __preadv(fd, iov, count, ofs); 
}
inline ssize_t pwrite(int fd, const void *buf, size_t size, off_t ofs) 
{ 
	printf("using uio layer\n"); 
	return __pwrite(fd, buf, size, ofs); 
}
inline ssize_t pwritev(int fd, const struct iovec *iov, int size, off_t ofs) { 
	printf("using uio layer\n"); 
	return __pwritev(fd, iov, size, ofs); 
}
inline ssize_t read(int fd, void *buf, size_t count) { 
	printf("using uio layer\n"); 
	return __read(fd, buf, count); 
}
inline ssize_t readv(int fd, const struct iovec *iov, int count) { 
	printf("using uio layer\n"); 
	return __readv(fd, iov, count); 
}
inline ssize_t write(int fd, const void *buf, size_t count) { 
	printf("using uio layer\n"); 
	return __write(fd, buf, count); 
}
inline ssize_t writev(int fd, const struct iovec *iov, int count) { 
	printf("using uio layer\n"); 
	return __writev(fd, iov, count); 
}

