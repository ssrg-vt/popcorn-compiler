#include "uio-private.h"

int uio_pipe(int fd[2])
{
	int fd1, fd2;
	struct buff_s buff;
	fd1 = uio_new_fd();
	fd2 = uio_new_fd();
	uio_new_buff(&buff);
	set_fd_buff(fd1, &buff);
	set_fd_buff(fd2, &buff);

	fd[0] = fd1;
	fd[0] = fd2;

	return 0;
}
