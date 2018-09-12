/**
 * Compress wasted space from multi-ISA binary emitted by Popcorn compiler.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/13/2018
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "definitions.h"
#include "bin.h"
#include "util.h"

static const char *file;
bool verbose = false;

static void print_help()
{
  printf("compress -- remove wasted space from multi-ISA binaries\n\n"
         "Usage: ./compress -f FILE [ OPTIONS ]\n"
         "Options:\n"
         "  -h      : print help & exit\n"
         "  -f FILE : ELF file to compress\n"
         "  -v      : be verbose\n");
}

static void parse_args(int argc, char **argv)
{
  int arg;

  while((arg = getopt(argc, argv, "hf:v")) != -1)
  {
    switch(arg)
    {
    case 'h': print_help(); exit(0); break;
    case 'f': file = optarg; break;
    case 'v': verbose = true; break;
    default:
      printf("Unknown option '%c'\n", arg);
      print_help();
      break;
    }
  }

  if(!file)
  {
    fprintf(stderr, "Please supply a binary with '-f'!\n");
    print_help();
    exit(1);
  }
}

#define PAGE_MASK( val ) ((val) & 0xfff)

ret_t compress_bss(bin *b)
{
  int diff;
  size_t orig_offset, orig_size, offset, shdrstrndx, nphdr, i;
  bool update = false;
  Elf64_Ehdr *ehdr;
  GElf_Phdr phdr;
  Elf64_Shdr *shdr;
  Elf_Scn *scn;
  Elf_Data *data = NULL;

  /* Find the .bss section & determine if it needs to be stripped */
  if(!(scn = get_section_by_name(b->e, ".bss")))
  {
    if(verbose) printf("No '.bss' section\n");
    return SUCCESS;
  }
  if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
  if(!(data = elf_getdata(scn, data))) return READ_ELF_FAILED;
  if(!data->d_size)
  {
    if(verbose) printf("'.bss' already has zero file size\n");
    return SUCCESS;
  }

  if(verbose)
  {
    if(elf_getshdrstrndx(b->e, &shdrstrndx)) return READ_ELF_FAILED;
    printf("'.bss' section header details:\n"
           "  Address: 0x%lx\n"
           "  Offset: %lu\n"
           "  Size: %lu\n",
           shdr->sh_addr, shdr->sh_offset, shdr->sh_size);
  }

  orig_size = shdr->sh_size;
  orig_offset = shdr->sh_offset;

  /* Remove .bss section's data */
  data->d_buf = NULL;
  data->d_size = 0;
  elf_flagdata(data, ELF_C_SET, ELF_F_DIRTY);

  // TODO need to verify updated section offsets match updated segment offsets

  /* Update offsets for remaining sections */
  offset = orig_offset;
  while((scn = elf_nextscn(b->e, scn)))
  {
    if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;

    /* See updating program headers below for alignment considerations */
    diff = (long)PAGE_MASK(shdr->sh_addr) - (long)PAGE_MASK(offset);
    if(diff < 0) diff = 0x1000 - (-diff);
    offset += diff;

    if(verbose) printf("Updating section '%s' to offset 0x%lx\n",
                       elf_strptr(b->e, shdrstrndx, shdr->sh_name), offset);
    
    shdr->sh_offset = offset;
    offset += shdr->sh_size;
    elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);
  }

  /* Update section header table offset */
  if(!(ehdr = elf64_getehdr(b->e))) return READ_ELF_FAILED;
  ehdr->e_shoff = offset;
  elf_flagehdr(b->e, ELF_C_SET, ELF_F_DIRTY);

  /* Update program headers */
  // Note: this assumes the linker has put .bss in its own segment!
  offset = orig_offset;
  if(elf_getphdrnum(b->e, &nphdr) == -1) return READ_ELF_FAILED;
  for(i = 0; i < nphdr; i++)
  {
    if(gelf_getphdr(b->e, i, &phdr) != &phdr) return READ_ELF_FAILED;
    /*
     * Look for the segment containing .bss.  Once we've found it, reduce its
     * file size to zero (keeping memory size the same) and update the file
     * offsets for the remaining segments.
     */
    if(phdr.p_offset == orig_offset && phdr.p_filesz == orig_size)
    {
      if(verbose)
        printf("Updating segment %lu (.bss) with zero file size\n", i);

      phdr.p_filesz = 0;
      if(!gelf_update_phdr(b->e, i, &phdr)) return WRITE_ELF_FAILED;
      update = true;
    }
    else if(update)
    {
      /*
       * Per the ELF standard:
       *
       *   "Virtual addresses and file offsets for the SYSTEM V architecture
       *    segments are congruent modulo 4 KB (0x1000) or larger powers of 2"
       *
       * The last 3 digits (hex) of the offset & virtual address must match.
       */
      diff = (long)PAGE_MASK(phdr.p_vaddr) - (long)PAGE_MASK(offset);
      if(diff < 0) diff = 0x1000 - (-diff);
      offset += diff;

      if(verbose)
        printf("Setting offset of segment %lu to 0x%lx\n", i, offset);

      phdr.p_offset = offset;
      offset += phdr.p_filesz;
      if(!gelf_update_phdr(b->e, i, &phdr)) return WRITE_ELF_FAILED;
    }
  }

  /* Write changes back to disk. */
  if(elf_update(b->e, ELF_C_WRITE) < 0) return WRITE_ELF_FAILED;
  return SUCCESS;
}

int main(int argc, char **argv)
{
  ret_t ret;
  bin *thebin = NULL;

  parse_args(argc, argv);

  if(elf_version(EV_CURRENT) == EV_NONE)
    die("could not initialize libELF", INVALID_ELF_VERSION);
  if((ret = init_elf_bin(file, &thebin)) != SUCCESS)
    die("could not initialize the binary", ret);

  if((ret = compress_bss(thebin)) != SUCCESS)
    die("could not compress .bss", ret);
  free_elf_bin(thebin);

  return 0;
}

