#include "stdio_impl.h"

static unsigned char buf_stdin[BUFSIZ+UNGET];
static FILE f = {
	.buf = buf_stdin+UNGET,
	.buf_size = sizeof buf_stdin-UNGET,
	.fd = 0,
	.flags = F_PERM | F_NOWR,
	.read = __stdio_read,
	.seek = __stdio_seek,
	.close = __stdio_close,
	.lock = -1,
};
FILE *const stdin = &f;
FILE *volatile __stdin_used = &f;
