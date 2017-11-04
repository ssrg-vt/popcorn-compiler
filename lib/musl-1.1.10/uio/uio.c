#include <stdlib.h>
#include "uio-private.h"

int fds_next = 0;
struct file_s *fds[MAX_FD];
struct file_s files[MAX_FD];

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

int uio_new_fd()
{
	int ret;
	if(fds_next >= MAX_FD)
		return -1;
	ret = fds_next++;
	fds[ret] = &files[ret];
	fds[ret]->offset = 0;
	return ret;
}

int uio_delete_fd(int fd)
{
	if(fds[fd] == NULL)
		return -1;

	fds[fd] = NULL;

	return 0;
}

struct buff_s* uio_new_buff()
{
	struct buff_s *buff = malloc(sizeof(*buff));

	buff->buff = malloc(DEFAULT_SIZE);
	if(buff->buff == NULL)
		perror(__func__);
	buff->size = DEFAULT_SIZE;
	buff->available = 0;

	return buff;
}

int set_fd_buff(int fd, struct buff_s *buff)
{
	fds[fd]->buff = buff;
}

struct file_s* get_fd_file(int fd)
{
	if( fds[fd] == NULL)
		perror(__func__);
	return  fds[fd];
}

int get_size(struct file_s *file, int count)
{
	return MIN(count, file->buff->available - file->offset);
}

int set_size(struct file_s *file, int count)
{
	void *new;
	int add = DEFAULT_SIZE;
	int remaining = file->buff->size - file->offset;

	if(remaining < count)
	/* allocate more space in buff if necessary */
	{
		add = MAX(add, count);
		if((new = realloc(file->buff->buff, file->buff->size + add)))
			perror(__func__);
		file->buff->buff = new;
		file->buff->size += add;
	}

	file->buff->available += count;
	return count;
}
