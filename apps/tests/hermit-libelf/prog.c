#include <stdio.h>
#include <stdlib.h>
#include <libelf/libelf.h>

int main(int argc, char *argv[])
{
	if(elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "error in elf library initialization\n");
		return -1;
	}

	printf("Test OK!\n");

	return 0;
}
