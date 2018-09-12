#include <stdlib.h>
#include <errno.h>
#include <platform.h>

void *__malloc0(size_t);

void *calloc(size_t m, size_t n)
{
	if (n && m > (size_t)-1/n) {
		errno = ENOMEM;
		return 0;
	}
	return __malloc0(n * m);
}

void *__popcorn_malloc0(size_t, int);

void *popcorn_calloc(size_t m, size_t n, int nid)
{
	if (n && m > (size_t)-1/n) {
		errno = ENOMEM;
		return 0;
	}
	return __popcorn_malloc0(n * m, nid);
}

void *popcorn_calloc_cur(size_t m, size_t n)
{
  return popcorn_calloc(m, n, popcorn_getnid());
}
