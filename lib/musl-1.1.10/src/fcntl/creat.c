#include <fcntl.h>
#include "libc.h"

int __creat(const char *filename, mode_t mode)
{
	return open(filename, O_CREAT|O_WRONLY|O_TRUNC, mode);
}

LFS64(__creat);
weak_alias(__creat, creat);
