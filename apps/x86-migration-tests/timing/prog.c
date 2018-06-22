#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <hermit/migration.h>

extern int sys_msleep(unsigned int ms);

inline unsigned sleep(unsigned int secs) {
	return sys_msleep(secs * 1000);
}

int main(void) {
	int i;
	struct timeval start, stop, res;

	gettimeofday(&start, 0x0);

	for(i=0; i<5; i++) {
		HERMIT_MIGPOINT();
		sleep(1);
	}

	gettimeofday(&stop, 0x0);

	timersub(&stop, &start, &res);
	printf("Result: %ld.%06ld\n", res.tv_sec, res.tv_usec);

	return 0;
}
