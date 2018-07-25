#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include <hermit/migration.h>

#define TARGET_FILE		"test-file.txt"
#define TARGET_FILE2	"test-file2.txt"
#define TEST_STR 		"abcdefghijklmnopqrstuvwxyz"

int rwtest() {
	int fd, fd2, bytes;
	char buf[] = TEST_STR;
	int str_size = strlen(buf);
	size_t offset_before, offset_after;

	fd = open(TARGET_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd == -1) {
		fprintf(stderr, "could not open file\n");
		return -1;
	}

	fd2 = open(TARGET_FILE2, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd == -1) {
		fprintf(stderr, "could not open file2\n");
		return -1;
	}

	bytes = write(fd, buf, str_size);
	if(bytes != str_size) {
		fprintf(stderr, "writing in file returned %d\n", bytes);
		return -1;
	}

	offset_before = lseek(fd, 0x0, SEEK_CUR);
	if(offset_before == -1) {
		fprintf(stderr, "lseek error\n");
		return -1;
	}

	HERMIT_FORCE_MIGRATION();

	offset_after = lseek(fd, 0x0, SEEK_CUR);
	if(offset_after == -1) {
		fprintf(stderr, "lseek error\n");
		return -1;
	}

	if(offset_before != offset_after) {
		fprintf(stderr, "corrupted offset after migration\n");
		return -1;
	}

	if(lseek(fd, 0x0, SEEK_SET) == -1) {
		fprintf(stderr, "lseek error\n");
		return -1;
	}

	bytes = read(fd, buf, str_size);
	if(bytes != str_size) {
		fprintf(stderr, "Reading from file returned %d\n", bytes);
		return -1;
	}
	
	buf[str_size] = '\0';
	if(strcmp(buf, TEST_STR)) {
		fprintf(stderr, "Unexpected file content\n");
		return -1;
	}

	if(write(fd2, "abc", 3) != 3) {
		fprintf(stderr, "Issue writing in fd2\n");
		return -1;
	}

	if(close(fd) == -1) {
		fprintf(stderr, "Close issue\n");
		return -1;
	}

	return 0;
}

int main(void) {
	
	if(rwtest())
		return -1;

	printf("RW test success!\n");
	
	return 0;
}
