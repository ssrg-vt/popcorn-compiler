/*
 * This tool is basically a hack to overcome the limitations
 * of the Gold linker, which creates ELF executables that are
 * PIE, but cannot make a PIE executable with a base address
 * of 0, which is necessary for runtime randomization to work
 * properly with various relocations.
 * This tool adjusts the pointers/addresses in the dynamic segment
 * and in the relocation entries to the values that they would
 * be if the binary did have a base address of 0. The kernel relocates
 * the binary to a random address, and the musl-libc initialization
 * code is then able to apply relative relocations, as well as
 * TLSDESC relocations (As needed in aarch64).
 */

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

#define ELF_PAGEALIGN(_v, _a) (((_v) + _a - 1) & ~(_a - 1))

int main(int argc, char **argv)
{
	uint8_t *mem;
	struct stat st;
	int fd;
	int i, j;

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

	Elf64_Dyn *dyn = NULL;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr;
	Elf64_Ehdr *ehdr;

	int count = 0;
	size_t dsize;
	uint64_t base;
	ehdr = (Elf64_Ehdr *)mem;
	phdr = (Elf64_Phdr *)&mem[ehdr->e_phoff];
	shdr = (Elf64_Shdr *)&mem[ehdr->e_shoff];

	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr->p_type == PT_LOAD && count == 0) {
			printf("Base: %#lx\n", phdr->p_vaddr);
			base = phdr->p_vaddr;
			count++;
		}
		if (phdr->p_type == PT_DYNAMIC) {
			dyn = (Elf64_Dyn *)&mem[phdr->p_offset];
			dsize = phdr->p_filesz;
			break;
		}
		phdr++;
	}
	for (i = 0; i < dsize / sizeof(Elf64_Dyn); i++) {
		switch(dyn[i].d_tag) {
		case DT_JMPREL:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_PLTGOT:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_RELA:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_SYMTAB:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_STRTAB:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_GNU_HASH:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_INIT:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_FINI:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_INIT_ARRAY:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_FINI_ARRAY:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_TLSDESC_PLT:
			dyn[i].d_un.d_ptr -= base;
			break;
		case DT_TLSDESC_GOT:
			dyn[i].d_un.d_ptr -= base;
			break;
		}
	}
	char *StringTable = (char *)&mem[shdr[ehdr->e_shstrndx].sh_offset];
	Elf64_Rela *rela;
	for (i = 0; i < ehdr->e_shnum; i++) {
		if (strcmp(&StringTable[shdr[i].sh_name], ".rela.dyn") != 0)
			continue;
		printf("Found .rela.dyn section\n");
		printf("Patching relocation entries with updated r_offset's\n");
		rela = (Elf64_Rela *)&mem[shdr[i].sh_offset];
		for (j = 0; j < shdr[i].sh_size / shdr[i].sh_entsize; j++) {
			if (ehdr->e_machine == EM_X86_64) {
				if (ELF64_R_TYPE(rela[j].r_info) == R_X86_64_DTPMOD64)
					continue;
			} else if (ehdr->e_machine == EM_AARCH64) {
				if (ELF64_R_TYPE(rela[j].r_info) == R_AARCH64_TLSDESC) {
					rela[j].r_offset -= base;
					rela[j].r_addend -= base;
					continue;
				}
			}
			printf("Changing %#lx to %#lx\n", rela[j].r_offset,
			    rela[j].r_offset - base);
			rela[j].r_offset -= base;
			rela[j].r_addend -= base;
		}
	}
	if (ehdr->e_machine != EM_AARCH64)
		goto done;
	for (i = 0; i < ehdr->e_shnum; i++) {
		if (strcmp(&StringTable[shdr[i].sh_name], ".rela.plt") != 0)
			continue;
		printf("Found rela.plt section\n");
		printf("Patching relocation entries\n");
		rela = (Elf64_Rela *)&mem[shdr[i].sh_offset];
		for (j = 0; j < shdr[i].sh_size / shdr[i].sh_entsize; j++) {
			if (ELF64_R_TYPE(rela[j].r_info) == R_AARCH64_TLSDESC) {
				printf("Changing %#lx to %#lx\n", rela[j].r_offset,
				    rela[j].r_offset - base);
				rela[j].r_offset -= base;
				continue;
			}
		}
	}
done:
	msync(mem, st.st_size, MS_SYNC);
	munmap(mem, st.st_size);
	exit(EXIT_SUCCESS);
}





