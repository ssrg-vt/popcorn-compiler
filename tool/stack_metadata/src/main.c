/**
 * Utility for reading information produced by Popcorn LLVM compiler and
 * encoding stack transformation meta-data into ELF binaries.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/6/2016
 */

#include <unistd.h>

#include "bin.h"
#include "stackmap.h"
#include "write.h"
#include "util.h"
#include "het_bin.h"
#include "gelf.h"

///////////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////////

static const char *args = "hf:s:i:v";
static const char *help =
"gen-stackinfo -- post-process object files (and their LLVM-generated stack \
maps) to tag call-sites with globally-unique identifiers & generate stack \
transformation meta-data\n\n\
\
Usage: ./gen-stackinfo [ OPTIONS ]\n\
Options:\n\
\t-h      : print help & exit\n\
\t-f name : object file or executable to post-process\n\
\t-s name : section name prefix added to object file (default is '" SECTION_PREFIX "')\n\
\t-i num  : number at which to begin generating call site IDs\n\
\t-v      : be verbose\n\n\
\
Note: this tool *must* be run after symbol alignment!";

static const char *file = NULL;
static char unwind_addr_name[512];
static const char *section_name = SECTION_PREFIX;
static uint64_t start_id = 0;
bool verbose = false;

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

static void print_help()
{
  printf("%s\n", help);
  exit(0);
}

static void parse_args(int argc, char **argv)
{
  int arg;

  while((arg = getopt(argc, argv, args)) != -1)
  {
    switch(arg)
    {
    case 'h':
      print_help();
      break;
    case 'f':
      file = optarg;
      break;
    case 's':
      section_name = optarg;
      break;
    case 'i':
      start_id = atol(optarg);
      break;
    case 'v':
      verbose = true;
      break;
    default:
      fprintf(stderr, "Unknown argument '%c'\n", arg);
      break;
    }
  }

  if(!file) die("please specify a file to post-process", INVALID_ARGUMENT);

  if(verbose)
    printf("Processing file '%s', adding section '%s.*', beginning IDs at %lu\n",
           file, section_name, start_id);
}

static ret_t populate_entsize(bin *b)
{
  Elf64_Shdr *shdr;
  Elf_Scn *scn;

  if(!(scn = get_section_by_name(b->e, ".stack_transform.unwind")))
    return FIND_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn)))
    return READ_ELF_FAILED;
  if (shdr->sh_entsize == 0)
    shdr->sh_entsize = SECTION_UNWIND_SIZE;

  if(!(scn = get_section_by_name(b->e, ".stack_transform.unwind_arange")))
    return FIND_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn)))
    return READ_ELF_FAILED;
  if (shdr->sh_entsize == 0)
    shdr->sh_entsize = SECTION_UNWIND_ADDR_SIZE;

  return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// Driver
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
  ret_t ret;
  size_t num_sm;
  bin *b;
  stack_map_section *sm;

  parse_args(argc, argv);
  snprintf(unwind_addr_name, 512, "%s.%s", section_name, SECTION_UNWIND_ADDR);

  /* Initialize libELF & open ELF descriptors */
  if(elf_version(EV_CURRENT) == EV_NONE)
    die("could not initialize libELF", INVALID_ELF_VERSION);
  if((ret = init_elf_bin(file, &b)))
    die("could not initialize ELF information", ret);

  /* Read stack map information */
  if((ret = init_stackmap(b, &sm, &num_sm)))
    die("could not read stack map section", ret);

  /* Populate the entsize of the UNWIND and UNWIND_ADDR sections. */
  if((ret = populate_entsize(b)))
    die("could not update unwind section entsize", ret);
  
  /* Sort the unwind address range section */
  if((ret = update_function_addr(b, unwind_addr_name)))
    die("could not sort unwind address range section", ret);

  /* Add stack transformation sections. */
  if((ret = add_sections(b, sm, num_sm, section_name, start_id,
                         unwind_addr_name)))
    die("could not add stack transformation sections", ret);

  free_stackmaps(sm, num_sm);
  free_elf_bin(b);

  return 0;
}

