#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 	(1024 * 1024)
#define TMP_FILE		"tmp.txt"
#define STRING_TO_WRITE	"abcd"

int main(void) {
	char *buffer;
	FILE *f;

	printf("hi\n");

	buffer = malloc(BUFFER_SIZE);
	memset(buffer, 0x0, BUFFER_SIZE);

	f = fopen(TMP_FILE, "w+");
	if(!f) {
		perror("fopen");
		return -1;
	}

	strcpy(buffer, STRING_TO_WRITE);
	if(fwrite(buffer, strlen(STRING_TO_WRITE), 1, f) != 1) {
		perror("frwite");
		return -1;
	}

	memset(buffer, 0x0, BUFFER_SIZE);

	if(fseek(f, 0x0, SEEK_SET)) {
		perror("fseek");
		return -1;
	}

	if(fread(buffer, strlen(STRING_TO_WRITE), 1, f) != 1) {
		perror("fread");
		return -1;
	}

	printf("read: %s\n", buffer);

	free(buffer);
	fclose(f);

	return 0;
}
