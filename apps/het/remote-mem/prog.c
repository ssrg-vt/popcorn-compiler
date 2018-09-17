#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

/* 1 == heap, 0 == bss */
#define USE_HEAP	0

#define BUFFER_SZ_MB		30
#define BUFFER_SZ_BYTES	(BUFFER_SZ_MB*1024*1204)

#if USE_HEAP == 0
int buf[BUFFER_SZ_BYTES/sizeof(int)];
char *type_str = "bss";
#else
char *type_str = "heap";
#endif

/* Regularly sleep during verification to test background pulling thread */
#define SLEEP					0
/* Migrate before verification */
#define MIGRATE					1

extern void migrate(int, void *, void *);
int main(int argc, char *argv[])
{
	struct timeval start, stop, total;
	int elements_num = BUFFER_SZ_BYTES/sizeof(int);

#if USE_HEAP == 1
	printf("Allocating buffer ...\n");
	int *buf = malloc(BUFFER_SZ_BYTES);
#endif

	printf("Initializing buffer ...\n");
	for(int i=0; i<(BUFFER_SZ_BYTES/sizeof(int)); i++)
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

	printf("Starting %s consistency verification ...\n", type_str);
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

#if USE_HEAP == 1
	free(buf);
#endif

	timersub(&stop, &start, &total);
	printf("Verification took: %ld.%06ld seconds\n", total.tv_sec,
			total.tv_usec);


	sys_msleep(3000);

	return 0;
}
