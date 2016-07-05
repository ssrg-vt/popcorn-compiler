/**
 * File descriptor declarations, definitions & handling functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#ifndef _BIN_H
#define _BIN_H

#include <elf.h>
#include <libelf/libelf.h>
#include <libelf/gelf.h>

#include "definitions.h"

/* File descriptor information for a binary */
typedef struct bin {
  const char *name;
  uint16_t arch;
  int fd;
  Elf *e;
} bin;

/**
 * Open an ELF binary descriptor.
 * @param name of binary ELF file
 * @param b_ptr pointer to bin struct to be populated
 * @return 0 if it was successfully opened, an error code otherwise
 */
ret_t init_elf_bin(const char *bin_fn, bin **b_ptr);

/**
 * Free an ELF binary descriptor.
 * @param b an ELF binary descriptor
 * @return 0 if it was freed, an error code otherwise
 */
ret_t free_elf_bin(bin *b);

#endif /* _BIN_H */

