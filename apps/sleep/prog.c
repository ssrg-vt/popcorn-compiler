#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define ITERATIONS	10

extern uint32_t should_migrate;

extern void sys_msleep(int msec);

void sleep(int sec) {
	sys_msleep(1000 * sec);
}

int main(void) {
	int i;

	printf("hi\n");

	for(i=0; i<ITERATIONS; i++) {
		printf("iteration %d\n", i);
		printf("should %d\n", should_migrate);
		sleep(1);
	}

	return 0;
}
