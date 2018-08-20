#include <stdio.h>
#include <stdlib.h>
#include <migrate.h>

extern void force_migration_flag(int val);

int main(int argc, char *argv[])
{
	printf("hello before\n");

	force_migration_flag(1);
	migrate(1, NULL, NULL);

	printf("hello after\n");

	return 0;
}
