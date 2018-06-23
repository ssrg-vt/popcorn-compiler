#include <stdio.h>
//#include <hermit/migration.h>

#define ITERATIONS	5

__attribute__((no_instrument_function)) void __cyg_profile_func_enter  (void *this_fn,
		                               void *call_site) {
	printf("hi!\n");
}

__attribute__((no_instrument_function)) void __cyg_profile_func_exit  (void *this_fn,
		                               void *call_site) {
	printf("hi!\n");
}


__attribute__((no_instrument_function)) extern void sys_msleep(int msec);
__attribute__((no_instrument_function)) void sleep(int sec) {
	return sys_msleep(sec * 1000);
}

void function(int it) {
	printf("iteration %d\n", it);
	sleep(1);
	return;
}

__attribute__((no_instrument_function)) int main(void) {
	int i;

	__cyg_profile_func_enter(NULL, NULL);
	__cyg_profile_func_exit(NULL, NULL);

	for(i=0; i<ITERATIONS; i++)
		function(i);

	return 0;
}
