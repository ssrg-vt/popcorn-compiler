#include <stdio.h>
#include <stdlib.h>
#include <migrate.h>

extern void force_migration_flag(int val);

int test_function(int x) {

	printf("test_fn called with parameter %d\n", x);

#ifdef __aarch64__
	migrate(1, NULL, NULL);
#else
	migrate(0, NULL, NULL);
#endif

	printf("test_fn returns %d\n", x);

	return x;
}

int main(int argc, char *argv[])
{
	int ret;
	printf("hello before\n");

	force_migration_flag(1);
	ret = test_function(42);

	printf("hello after, returned %d\n", ret);

	return 0;
}
