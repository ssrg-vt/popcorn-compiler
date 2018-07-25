#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <hermit/migration.h>

int data_var = 10;
int bss_var;
int *heap_ptr;
__thread int tdata_var = 10;
__thread int tbss_var;

extern int sys_msleep(unsigned int ms);

inline unsigned sleep(unsigned int secs) {
	return sys_msleep(secs * 1000);
}

int main(int argc, char **argv) {
	int i, stack_var;

	heap_ptr = malloc(1 * sizeof(int));

	*heap_ptr = 0;
	stack_var = 0;
	bss_var = 0;
	data_var = 0;
	tdata_var = 0;
	tbss_var = 0;

	for(i=0; i<10; i++) {
		sleep(1);
		printf("iteration %d\n", i);
		printf(" - stack: %d\n", stack_var++);
		printf(" - bss:   %d\n", bss_var++);
		printf(" - data:  %d\n", data_var++);
		printf(" - heap:  %d\n", (*heap_ptr)++);
		printf(" - tdata:  %d\n", tdata_var++);
		printf(" - tbss:  %d\n", tbss_var++);

		if(i == 3)
			HERMIT_FORCE_MIGRATION();
		HERMIT_MIGPOINT();
	}

	free(heap_ptr);

	return 0;
}
