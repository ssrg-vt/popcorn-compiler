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

///////////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////////

static const char *args = "ha:f:s:i:v";
static const char *help =
"gen-stackinfo -- post-process object files (and their LLVM-generated stack \
maps) to tag call-sites with globally-unique identifiers & generate stack \
transformation meta-data\n\n\
\
Usage: ./gen-stackinfo [ OPTIONS ]\n\
Options:\n\
\t-h      : print help & exit\n\
\t-a name : name of unwind address range section (default is '" \
            SECTION_PREFIX "." SECTION_UNWIND_ADDR "')\n\
\t-f name : object file or executable to post-process\n\
\t-s name : section name added to object file (default is '" SECTION_PREFIX "')\n\
\t-i num  : number at which to begin generating call site IDs\n\
\t-v      : be verbose";

static const char *file = NULL;
static const char *unwind_addr_name = SECTION_PREFIX "." SECTION_UNWIND_ADDR;
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
    case 'a':
      unwind_addr_name = optarg;
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

///////////////////////////////////////////////////////////////////////////////
// Driver
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
	ret_t ret;
  size_t num_sm;
	bin *b;
  stack_map *sm;

	parse_args(argc, argv);	

	/* Initialize libELF & open ELF descriptors */
	if(elf_version(EV_CURRENT) == EV_NONE)
		die("could not initialize libELF", INVALID_ELF_VERSION);
	if((ret = init_elf_bin(file, &b)))
		die("could not initialize ELF information", ret);

  /* Read stack map information */
  if((ret = init_stackmap(b, &sm, &num_sm)))
    die("could not read stack map section", ret);

  /* Sort the unwind address range section */
  if((ret = sort_addresses(b, unwind_addr_name)))
    die("could not sort unwind address range section", ret);

  /* Add stack transformation sections. */
  if((ret = add_sections(b, sm, num_sm, section_name, start_id)))
    die("could not add stack transformation sections", ret);

  free_stackmaps(sm, num_sm);
  free_elf_bin(b);

	return 0;
}

