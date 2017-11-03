int uio_pipe(int fd[2]);
int uio_close(int fd);
ssize_t uio_read(int fd, void *buf, size_t count);
ssize_t uio_write(int fd, void *buf, size_t count);
