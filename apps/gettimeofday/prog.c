#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int main(void) {
	struct timeval s1;

	gettimeofday(&s1, NULL);

	printf("GTOD sec: %lu, usec: %lu\n", s1.tv_sec, s1.tv_usec);

	return 0;
}
