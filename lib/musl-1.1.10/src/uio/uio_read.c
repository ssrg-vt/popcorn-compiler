#include "uio-private.h"

ssize_t uio_read(int fd, void *buf, size_t count)
{
	struct file_s* file;

	file = get_fd_file(fd);

	count = get_size(file, count);

	memcpy(buf, file->buff.buff, count);

	return count;
}
