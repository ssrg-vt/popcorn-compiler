#include "uio-private.h"
#include <string.h>

ssize_t uio_write(int fd, void *buf, size_t count)
{
	struct file_s* file;

	file = get_fd_file(fd);

	count = set_size(file, count);

	memcpy(file->buff->buff, buf, count);

	return count;
}
