#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_THREADS 	4
#define BUFFER_SIZE 	(1024 * 1024)
#define STRING_TO_WRITE	"abcd"

__thread int thr_data = 4;
__thread int thr_bss;

extern void sys_msleep(unsigned int ms);
void xsleep(unsigned int s) {
	sys_msleep(s * 1000);
}

void* thread_func(void* arg)
{
	int id = *((int*) arg);
	char *buffer;
	char tmp_file[64];
	FILE *f;

	thr_data = id;
	thr_bss = id;

	sprintf(tmp_file, "tmp-%d.txt", id);

	buffer = malloc(BUFFER_SIZE);
	memset(buffer, 0x0, BUFFER_SIZE);

	f = fopen(tmp_file, "w+");
	if(!f) {
		perror("fopen");
		assert(0);
	}

	strcpy(buffer, STRING_TO_WRITE);
	if(fwrite(buffer, strlen(STRING_TO_WRITE), 1, f) != 1) {
		perror("frwite");
		assert(0);
	}

	memset(buffer, 0x0, BUFFER_SIZE);

	if(fseek(f, 0x0, SEEK_SET)) {
		perror("fseek");
		assert(0);
	}

	if(fread(buffer, strlen(STRING_TO_WRITE), 1, f) != 1) {
		perror("fread");
		assert(0);
	}

	printf("read: %s\n", buffer);

	printf("[%d] Hello Thread!!! arg = %d\n", getpid(), id);
	printf("[%d] tdata = %d, tbss = %d\n", getpid(), thr_data, thr_bss);
	printf("[%d] Going to sleep\n", getpid());
	xsleep(1);
	printf("[%d] Sleep done, exiting\n", getpid());

	free(buffer);

	return 0;
}

int main(int argc, char** argv)
{
	pthread_t threads[MAX_THREADS];
	int i, ret, param[MAX_THREADS];

	printf("[%d] Main thread starts ...\n", getpid());
	printf("[%d] Initial value tdata: %d, tbss:%d\n", getpid(), thr_data, thr_bss);

	for(i=0; i<MAX_THREADS; i++) {
		param[i] = i;
		ret = pthread_create(threads+i, NULL, thread_func, param+i);
		if (ret) {
			printf("[%d] Thread creation failed! error =  %d\n", getpid(), ret);
			return ret;
		} else printf("[%d] Created thread %d\n", getpid(), i);
	}


	printf("[%d] Going to sleep\n", getpid());
	xsleep(2);

	printf("[%d] Sleep done, trying to join\n", getpid());
	/* wait until all threads have terminated */
	for(i=0; i<MAX_THREADS; i++)
		pthread_join(threads[i], NULL);	

	printf("[%d] Joined\n", getpid());

	return 0;
}
