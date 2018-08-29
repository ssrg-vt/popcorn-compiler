#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

extern void sys_msleep(int ms);
void sleep(int s) {
	return sys_msleep(s*1000);
}

int main(void) {
	struct timeval s1, s2, res;

	gettimeofday(&s1, NULL);
	printf("GTOD sec: %lu, usec: %lu\n", s1.tv_sec, s1.tv_usec);
	sleep(1);
	gettimeofday(&s2, NULL);
	printf("GTOD sec: %lu, usec: %lu\n", s2.tv_sec, s2.tv_usec);

	return 0;
}
