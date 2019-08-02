#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "config.h"
#include "migrate.h"

#if _FILE_SELECT_MIGRATE == 1

#define MAX_ACTIVE_CHECK_MIGRATES	(50)
#define CHECK_MIGRATE_CONFIG_X86		"migrate_x86.conf"
#define CHECK_MIGRATE_CONFIG_ARM64		"migrate_arm64.conf"


int active_x86_check_migrates[MAX_ACTIVE_CHECK_MIGRATES];
int num_active_x86_check_migrates = 0;

int active_arm64_check_migrates[MAX_ACTIVE_CHECK_MIGRATES];
int num_active_arm64_check_migrates = 0;

#ifdef _RANDOMIZE_MIGRATION
#define RANDOMIZE_MIGRATE_CONFIG		"random.conf"
unsigned int mig_percentage_x86 = 0;
unsigned int mig_percentage_arm64 = 0;
#endif

int migration_point_active(void* addr)
{
	uint64_t address_value = (uint64_t) addr;
	int retval = 0;
	int i;
#ifdef __x86_64__
	for (i = 0; i < num_active_x86_check_migrates; ++i) {
		if (address_value == active_x86_check_migrates[i]) {
			retval = 1;
			break;
		}
	}

#elif defined(__aarch64__)
	for (i = 0; i < num_active_arm64_check_migrates; ++i) {
		if (address_value == active_arm64_check_migrates[i]) {
			retval = 1;
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
	FILE *fp_random;
	int random_iterator=0;
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

#ifdef _RANDOMIZE_MIGRATION
        fp_random = fopen(RANDOMIZE_MIGRATE_CONFIG, "r"); // read mode

	if (fp_random == NULL) {
		perror("Error while opening the random config file.\n");
		exit(EXIT_FAILURE);
	}

	/* Read in the random migration probabilities */
	while (fgets(line, 1024, fp_random)) {
		const char* tok;
		for (tok = strtok(line, ",");
		        tok && *tok;
		        tok = strtok(NULL, ",\n"),
			random_iterator++) {
			if (random_iterator == 0)
				mig_percentage_x86 = (int)strtoul(tok, NULL, 10);
			else if(random_iterator == 1)
				mig_percentage_arm64 = (int)strtoul(tok, NULL,
								    10);
		}
	}
	printf("Percentage of migrate from x86 to arm: %d\n",
	       mig_percentage_x86);
	printf("Percentage of migrate from arm to x86: %d\n",
	       mig_percentage_arm64);




#endif

	fclose(fp_arm64);
}

#endif /*_FILE_SELECT_MIGRATE */
