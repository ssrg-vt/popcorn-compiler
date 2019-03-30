#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <hermit/migration.h>

#define SINGLE_VAR_HEAP_SZ	(1024*1024*10)

extern int sys_msleep(unsigned int ms);

inline unsigned sleep(unsigned int secs) {
	return sys_msleep(secs * 1000);
}

int main(int argc, char **argv) {
	int i;
	uint32_t *heap1 = malloc(SINGLE_VAR_HEAP_SZ);
	uint32_t *heap2 = malloc(SINGLE_VAR_HEAP_SZ);

	for(i=0; i<(SINGLE_VAR_HEAP_SZ / sizeof(uint32_t)); i++)
		heap1[i] = i;

	HERMIT_FORCE_MIGRATION();

	for(i=0; i<(SINGLE_VAR_HEAP_SZ / sizeof(uint32_t)); i++)
		heap2[i] = i;

	for(i=0; i<(SINGLE_VAR_HEAP_SZ / sizeof(uint32_t)); i++) {
		if(heap1[i] != i) {
			printf("Heap init before migration: issue at offset%d\n", i);
			return -1;
		}

		if(heap2[i] != i) {
			printf("Heap init before migration: issue at offset%d\n", i);
			return -1;
		}
	}

	printf("Test succeeded!!\n");

	free(heap1);
	free(heap2);

	return 0;
}
