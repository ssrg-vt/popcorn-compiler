#include <stdio.h>
#include <stdlib.h>
#include <migrate.h>

extern void force_migration_flag(int val);

int test_function(int x) {
	migrate(1, NULL, NULL);

	return x;
}

int main(int argc, char *argv[])
{
	printf("hello before\n");

	force_migration_flag(1);
	test_function(12);

	printf("hello after\n");

	return 0;
}
