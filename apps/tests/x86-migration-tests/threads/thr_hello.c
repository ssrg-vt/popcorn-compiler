#include <stdio.h>
#include <pthread.h>
#include <hermit/migration.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_THREADS 2
#define ITERATIONS 5

__thread int th = -1;

extern void sys_msleep(int msec);

void* thread_func(void* arg) {
	int i;
	int id = *((int*) arg);

	th = id;
	for(i=0; i<ITERATIONS; i++) {
		printf("[%d] iteration %d (th == %d)\n", getpid(), i, th);
		HERMIT_MIGPOINT();
		sys_msleep(1 * 1000);
	}

	printf("[%d] exiting\n", getpid());

	return 0;
}

void *f2(void *arg) {
	printf("hi I'm just executing for a sec\n");
	sys_msleep(1000);

	return 0;
}

int main(int argc, char** argv) {
	pthread_t threads[MAX_THREADS];
	int i, ret, param[MAX_THREADS];
	pthread_t th1;

	pthread_create(&th1, NULL, f2, NULL);

	sys_msleep(2000);

	for(i=0; i<MAX_THREADS; i++) {
		param[i] = i;
		ret = pthread_create(threads+i, NULL, thread_func, param+i);
		if (ret) {
			printf("Thread creation failed! error =  %d\n", ret);
			return ret;
		}
	}

	for(i=0; i<ITERATIONS+2; i++) {
		HERMIT_MIGPOINT();
		printf("[p] iteration %d\n", i);
		sys_msleep(1 * 1000);
	}

	/* wait until all threads have terminated */
	printf("Trying to join\n");
	sys_msleep(20);
	for(i=0; i<MAX_THREADS; i++)
		pthread_join(threads[i], NULL);

	return 0;
}
