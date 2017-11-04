#define MAX_FD 256
#define DEFAULT_SIZE 4096
#include <stdlib.h>

struct file_s;

struct buff_s{
	void* buff;
	size_t size;
	size_t available;
};

struct file_s{
	int offset;
	struct buff_s *buff;
};

int uio_new_fd();
int uio_delete_fd(int fd);
struct buff_s* uio_new_buff();
int set_fd_buff(int fd, struct buff_s *buff);
struct file_s* get_fd_file(int fd);
int get_size(struct file_s *file, int count);
int set_size(struct file_s *file, int count);
