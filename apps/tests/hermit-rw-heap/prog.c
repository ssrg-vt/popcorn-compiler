#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __aarch64__
#define BINARY "/tmp/prog_aarch64"
#else
#define BINARY "./prog_x86-64"
#endif

int main(int argc, char **argv) {
	int fd2;

	char *buf4 = malloc(250*1024*1204);
	memset(buf4, 0x0, 250*1024*1024);

	char * buf3 = malloc(8192);
	memset(buf3, 0x0, 8192);

	printf("hi\n");

	fd2 = open(BINARY, O_RDONLY, 0x0);
	if(fd2 == -1) {
		printf("error open\n");
		return -1;
	}

	char *buf = malloc(4096*4);
	if(!buf) {
		fprintf(stderr, "error malloc!\n");
		return -1;
	}
	memset(buf, 0x0, 4096*4);
	printf("malloc returned %p\n", buf);

	read(fd2, buf, 4096*4);
	printf("read:\n");
	for(int i=0; i<4; i++)
		printf("%c", buf[i]);
	printf("\n");


	free(buf4);

	close(fd2);
	printf("bye!\n");
	return 0;
}
