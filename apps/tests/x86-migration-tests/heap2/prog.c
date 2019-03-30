#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <hermit/migration.h>

//#define SINGLE_VAR_HEAP_SZ	((1024*1024*1005) + (4096 * 251) + 3760)
#define SINGLE_VAR_HEAP_SZ	(1024*1024*1024)
#define ADDR				0x3fe00000

extern unsigned long long get_cpu_frequency(void);

extern uint64_t virt_to_phys(uint64_t virt);

extern int page_unmap(size_t v, size_t n);

int main(int argc, char **argv) {
	unsigned int i;
	char *heap1 = malloc(SINGLE_VAR_HEAP_SZ);
	char *heap2 = malloc(SINGLE_VAR_HEAP_SZ);
	char *heap3 = malloc(SINGLE_VAR_HEAP_SZ);

	for(i=0; i< SINGLE_VAR_HEAP_SZ; i++) {
		heap1[i] = 'a';
		heap2[i] = 'a';
		heap3[i] = 'a';
	}

	HERMIT_FORCE_MIGRATION();

	for(i=0; i< SINGLE_VAR_HEAP_SZ; i++) {
		if(heap1[i] != 'a' || heap2[i] != 'a' || heap3[i] != 'a') {
			fprintf(stderr, "Element %d check failure\n", i);
			free(heap1);
			free(heap2);
			free(heap3);
			return -1;
		}
	}

	free(heap1);
	free(heap2);
	free(heap3);

	printf("Success!\n");

	return 0;
}
