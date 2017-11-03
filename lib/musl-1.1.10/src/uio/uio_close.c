#include "uio-private.h"

int uio_close(int fd)
{
	return uio_delete_fd(fd);
}
