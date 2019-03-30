#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <hermit/migration.h>

#define SINGLE_VAR_HEAP_SZ	(1024*1024*250)

int main(int argc, char **argv) {
	int i;
	struct timeval start, stop, res;
	uint32_t *heap1 = NULL;
	uint32_t *heap2 = NULL;

	gettimeofday(&start, NULL);

	heap1 = malloc(SINGLE_VAR_HEAP_SZ);
	heap2 = malloc(SINGLE_VAR_HEAP_SZ);

	for(i=0; i<(SINGLE_VAR_HEAP_SZ / sizeof(uint32_t)); i++)
		heap1[i] = heap2[i] = i;

	HERMIT_FORCE_MIGRATION();

	for(i=0; i<(SINGLE_VAR_HEAP_SZ / sizeof(uint32_t)); i++) {
		if(heap1[i] != i) {
			printf("Heap issue at offset%d\n", i);
			return -1;
		}
		if(heap2[i] != i) {
			printf("Heap2 issue at offset%d\n", i);
			return -1;
		}

	}

	printf("Test succeeded!!\n");

	free(heap1);

	gettimeofday(&stop, NULL);

	timersub(&stop, &start, &res);
	printf("Test took: %ld.%06ld seconds\n", res.tv_sec, res.tv_usec);

	return 0;
}
