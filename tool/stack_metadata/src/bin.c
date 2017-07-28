/**
 * ELF handling functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#include <unistd.h>
#include <fcntl.h>

#include "bin.h"
#include "util.h"

ret_t init_elf_bin(const char *bin_fn, bin **b_ptr)
{
	ret_t ret = SUCCESS;
	bin *b;

	if(!b_ptr || !bin_fn) return INVALID_ARGUMENT;
  *b_ptr = NULL;
	b = malloc(sizeof(bin));
	memset(b, 0, sizeof(bin));
	b->name = bin_fn;

  /* Open file descriptor. */
	if((b->fd = open(bin_fn, O_RDWR)) == -1)
	{
		ret = OPEN_FILE_FAILED;
		goto free_desc;
	}

  /* Open ELF descriptor. */
	if((b->e = elf_begin(b->fd, ELF_C_RDWR, NULL)) == NULL)
	{
		ret = OPEN_ELF_FAILED;
		goto close_fd;
	}

  /* Ensure it's a supported binary. */
	if(!check_elf_ehdr(b->e))
	{
		ret = INVALID_ELF;
		goto close_elf;
	}
	b->arch = elf64_getehdr(b->e)->e_machine;

  if(verbose)
    printf("Header for '%s': %s, %s (%s), %s ABI, %s\n",
      bin_fn, elf_kind_name(b->e), elf_class_name(b->e), elf_data_name(b->e),
      elf_abi_name(b->e), elf_arch_name(b->e));

  /* Notify libELF that we're controlling layout. */
	if(elf_flagelf(b->e, ELF_C_SET, ELF_F_LAYOUT) == 0 ||
		 elf_flagelf(b->e, ELF_C_SET, ELF_F_LAYOUT_OVERLAP) == 0)
	{
		ret = LAYOUT_CONTROL_FAILED;
		goto close_elf;
	}

	*b_ptr = b;
	goto return_val;

close_elf:
	elf_end(b->e);
close_fd:
	close(b->fd);
free_desc:
	free(b);
return_val:
	return ret;
}

ret_t free_elf_bin(bin *b)
{
  if(!b) return INVALID_ARGUMENT;

	elf_end(b->e);
	close(b->fd);
	free(b);

  return SUCCESS;
}

