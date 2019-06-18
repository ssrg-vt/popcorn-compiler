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
    printf("%d.%d\n", __NEWLIB__, __NEWLIB_MINOR__);

	return 0;
}
