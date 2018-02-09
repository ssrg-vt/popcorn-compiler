#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <migrate.h>
#include <stdarg.h>
#include <sys/mman.h>

#ifdef __PIO__

#define __before_io_prep() int __pio_cn = current_nid()
#define __before_io_() migrate(get_origin_nid(), NULL, NULL)
//following macro contains two insructions: be careful
#define __before_io() __before_io_prep(); __before_io_()
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
	//printf("%s: opening %s file request from node %d to node %d\n", __func__, filename, __pio_cn, current_nid());
	int ret = __open_(filename, flags, mode);
	__after_io();
	return ret;
}

int close(int fd)
{
	__before_io();
	int ret = __close(fd);
	__after_io();
	return ret;
}

void *mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
	__before_io_prep();
	if(fd != -1)
	{
		 __before_io_();
	}
	void* ret =__mmap(start, len, prot, flags, fd, off);
	if(fd != -1)
	{
		__after_io();
	}
	return ret;

}

int creat(const char *filename, mode_t mode)
{
	__before_io();
	int ret = __creat(filename, mode);
	__after_io();
	return ret;
}

inline ssize_t pread(int fd, void *buf, size_t size, off_t ofs) 
{ 
	
	__before_io();
	ssize_t ret = __pread(fd, buf, size, ofs);
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
#endif
