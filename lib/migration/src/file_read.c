#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "config.h"

#if _FILE_SELECT_MIGRATE == 1

#define MAX_ACTIVE_CHECK_MIGRATES	(50)
#define CHECK_MIGRATE_CONFIG_X86		"migrate_x86.conf"
#define CHECK_MIGRATE_CONFIG_ARM64		"migrate_arm64.conf"


int active_x86_check_migrates[MAX_ACTIVE_CHECK_MIGRATES];
int num_active_x86_check_migrates = 0;

int active_arm64_check_migrates[MAX_ACTIVE_CHECK_MIGRATES];
int num_active_arm64_check_migrates = 0;

void randomize_migration()
{
	//printf("Randomize migration called \n");
}

bool migration_point_active(void* addr)
{
	uint64_t address_value = (uint64_t) addr;
	bool retval = false;
	int i;
#ifdef __x86_64__
	for (i = 0; i < num_active_x86_check_migrates; ++i) {
		if (address_value == active_x86_check_migrates[i]) {
			retval = true;
			break;
		}
	}

#elif defined(__aarch64__)
	for (i = 0; i < num_active_arm64_check_migrates; ++i) {
		if (address_value == active_arm64_check_migrates[i]) {
			retval = true;
			break;
		}
	}
#endif

	return retval;
}

void __attribute__((constructor)) __load_migration_points()
{
        FILE *fp_x86;
        FILE *fp_arm64;
	char line[1024];

        fp_x86 = fopen(CHECK_MIGRATE_CONFIG_X86, "r"); // read mode
        fp_arm64 = fopen(CHECK_MIGRATE_CONFIG_ARM64, "r"); // read mode

	if (fp_x86 == NULL) {
		perror("Error while opening the x86 config file.\n");
		exit(EXIT_FAILURE);
	}

	if (fp_arm64 == NULL) {
		perror("Error while opening the arm config file.\n");
		exit(EXIT_FAILURE);
	}

	/* Read in the x86 migration point addresses */
	while (fgets(line, 1024, fp_x86)) {
		const char* tok;
		for (tok = strtok(line, ",");
		        tok && *tok;
		        tok = strtok(NULL, ",\n"),
			num_active_x86_check_migrates++) {
			active_x86_check_migrates[num_active_x86_check_migrates]=
				(int)strtoul(tok, NULL, 16);
		}
	}
	printf("Total number active x86 check migrates: %d\n",
	       num_active_x86_check_migrates);

	fclose(fp_x86);

	/* Read in the arm64 migration point addresses */
	while (fgets(line, 1024, fp_arm64)) {
		const char* tok;
		for (tok = strtok(line, ",");
		        tok && *tok;
		        tok = strtok(NULL, ",\n"),
			num_active_arm64_check_migrates++) {
			active_arm64_check_migrates[num_active_arm64_check_migrates] =
				(int)strtoul(tok, NULL, 16);
		}
	}
	printf("Total number active arm check migrates: %d\n",
	       num_active_arm64_check_migrates);

	fclose(fp_arm64);
}

#endif /*_FILE_SELECT_MIGRATE */
