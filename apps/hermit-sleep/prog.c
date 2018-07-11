#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define ITERATIONS 1

extern void sys_msleep(int ms);
void sleep(int sec) {
	sys_msleep(sec*1000);
}

int main(void) {

	for (int i = 0; i < ITERATIONS; ++i) {
		printf("Iteration %d\n", i);
		sleep(1);
	}

	return 0;
}
