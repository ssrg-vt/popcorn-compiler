#ifdef LIBC
#include <stdio.h>
#include <unistd.h>
#else
#include <uio-private.h>
#endif


int main()
{
	char buff[5];
	int pipes[2];

	pipe(pipes);

	write(pipes[1], "tttt", 4);
	read(pipes[0], &buff, 4);

	buff[4] = '\0';

	printf(buff);
}
