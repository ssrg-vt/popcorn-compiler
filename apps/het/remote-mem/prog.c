#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define HEAP_BUFFER_SZ_MB		30
#define HEAP_BUFFER_SZ_BYTES	(HEAP_BUFFER_SZ_MB*1024*1204)

#define SLEEP					0
#define MIGRATE					1

extern void migrate(int, void *, void *);
int main(int argc, char *argv[])
{
	struct timeval start, stop, total;
	int elements_num = HEAP_BUFFER_SZ_BYTES/sizeof(int);

	printf("Allocating buffer ...\n");
	int *buf = malloc(HEAP_BUFFER_SZ_BYTES);

	printf("Initializing buffer ...\n");
	for(int i=0; i<(HEAP_BUFFER_SZ_BYTES/sizeof(int)); i++)
		buf[i] = i;

#if MIGRATE
#ifdef __aarch64__
	printf("Migrating to x86\n");
	migrate(0, NULL, NULL);
#else
	printf("Migrating to arm\n");
	migrate(1, NULL, NULL);
#endif
#endif

	printf("Starting heap consistency verification ...\n");
	gettimeofday(&start, NULL);
	for(int i=0; i<elements_num; i++) {

#if SLEEP
		/* Sleep a bit to give soem time for the background thread to grab
		 * some pages too */
		if(i == elements_num/2 || i == elements_num/4 || i == (elements_num/4)*3)
			sys_msleep(1000);
#endif

		if(buf[i] != i) {
			fprintf(stderr, "Error heap buffer corrupted at offset %d, "
					"read %d expected %d\n", i, buf[i], i);
			exit(-1);
		}
	}
	gettimeofday(&stop, NULL);

	printf("Test succeeded!\n");
	free(buf);

	timersub(&stop, &start, &total);
	printf("Verification took: %ld.%06ld seconds\n", total.tv_sec,
			total.tv_usec);

	return 0;
}
