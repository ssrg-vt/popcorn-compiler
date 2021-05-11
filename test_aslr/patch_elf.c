#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdint.h>
#include <sys/mman.h>
#include <elf.h>

int main(int argc, char **argv)
{
	uint8_t *mem;
	struct stat st;
	int fd;
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
	int i;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <elfbin>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	if (fstat(fd, &st) < 0) {
		perror("fstat");
		exit(EXIT_FAILURE);
	}
	mem = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}
	ehdr = (Elf64_Ehdr *)mem;
	phdr = (Elf64_Phdr *)&mem[ehdr->e_phoff];
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr->p_type == PT_LOAD) {
			if (phdr->p_flags == PF_R)
				phdr->p_flags |= PF_W;
		}
		phdr++;
	}

	msync(mem, st.st_size, MS_SYNC);
	munmap(mem, st.st_size);
	exit(EXIT_SUCCESS);
}





