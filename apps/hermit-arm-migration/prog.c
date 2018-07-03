#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <hermit/migration.h>

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
		HERMIT_MIGPOINT();
		printf("iteration %d\n", i);
		sleep(1);
	}

	return 0;
}
