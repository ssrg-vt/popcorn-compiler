/**
 * Check LLVM stackmaps to ensure that the same number of stackmaps and live
 * variable locations for each stackmap were generated for all binaries.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 7/7/2016
 */

#include <unistd.h>

#include "definitions.h"
#include "bin.h"
#include "stackmap.h"
#include "util.h"
#include "het_bin.h"

///////////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////////

static const char *args = "ha:x:";
static const char *help =
"check-stackmaps - check LLVM stackmap sections for matching metadata across "
"binaries\n\n\
\
Usage: ./dump_metadata [ OPTIONS ]\n\
Options:\n\
\t-h      : print help & exit\n\
\t-a file : name of AArch64 executable\n\
\t-x file : name of x86-64 executable\n\n\
\
Note: this tool assumes binaries have been through the alignment tool, as it \
stackmap checking based on function virtual address";

static const char *bin_aarch64_fn = NULL;
static const char *bin_x86_64_fn = NULL;
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
      bin_aarch64_fn = optarg;
      break;
    case 'x':
      bin_x86_64_fn = optarg;
      break;
    default:
      fprintf(stderr, "Unknown argument '%c'\n", arg);
      break;
    }
  }

  if(!bin_aarch64_fn || !bin_x86_64_fn)
    die("please specify binaries (run with -h for more information)",
        INVALID_ARGUMENT);
}

///////////////////////////////////////////////////////////////////////////////
// Checking LLVM-generated metadata
///////////////////////////////////////////////////////////////////////////////

/*
 * The goal of the checker is to print out as much non-matching information as
 * possible rather than to die on errors.  So, we'll iterate over everything
 * and print out where we find inconsistencies.
 */
void check_stackmaps(stack_map *sm_a, size_t num_sm_a,
                     stack_map *sm_b, size_t num_sm_b)
{
  char buf[BUF_SIZE];
  size_t i, j, num_sm, num_records;
  uint64_t func_a, func_b;

  /*
   * Errors here indicate there's probably a file compiled without the
   * stackmap-insertion pass, i.e., there's no stackmap section.
   */
  num_sm = (num_sm_a > num_sm_b ? num_sm_b : num_sm_a);
  if(num_sm_a != num_sm_b)
  {
    snprintf(buf, BUF_SIZE, "number of stackmaps doesn't match (%lu vs. %lu)",
             num_sm_a, num_sm_b);
    warn(buf);
  }

  /* Iterate over all accumulated stackmap sections (one per .o file) */
  for(i = 0; i < num_sm; i++)
  {
    /*
     * Errors here indicate different numbers of stackmap intrinsics inserted
     * into the IR.
     */
    num_records = (sm_a[i].num_records > sm_b[i].num_records ?
                   sm_b[i].num_records : sm_a[i].num_records);
    if(sm_a[i].num_records != sm_b[i].num_records)
    {
      snprintf(buf, BUF_SIZE,
               "number of records for stackmap section %lu doesn't match (%u vs. %u)",
               i, sm_a[i].num_records, sm_b[i].num_records);
      warn(buf);
    }

    /*
     * Iterate over all records in the stackmap, i.e., all
     * llvm.experimental.stackmap intrinsics
     */
    for(j = 0; j < num_records; j++)
    {
      func_a = sm_a[i].stack_sizes[sm_a[i].stack_maps[j].func_idx].func_addr;
      func_b = sm_b[i].stack_sizes[sm_b[i].stack_maps[j].func_idx].func_addr;

      /*
       * Errors here indicate stackmaps inside of different functions.
       */
      if(func_a != func_b)
      {
        snprintf(buf, BUF_SIZE,
                 "stackmap %lu corresponds to different functions (%lx vs. %lx)",
                 j, func_a, func_b);
        warn(buf);
      }

      /*
       * Errors here indicate different numbers of live values at the stackmap
       * intrinsic call site.
       */
      if(sm_a[i].stack_maps[j].locations->num !=
         sm_b[i].stack_maps[j].locations->num)
      {
        snprintf(buf, BUF_SIZE,
                 "stackmap %lu has different numbers of location records (%u vs. %u)",
                 j, sm_a[i].stack_maps[j].locations->num,
                 sm_b[i].stack_maps[j].locations->num);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Driver
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
  ret_t ret;
  bin *bin_aarch64 = NULL, *bin_x86_64 = NULL;
  stack_map *sm_aarch64 = NULL, *sm_x86_64 = NULL;
  size_t num_sm_aarch64, num_sm_x86_64;

  parse_args(argc, argv);

  if(elf_version(EV_CURRENT) == EV_NONE)
    die("could not initialize libELF", INVALID_ELF_VERSION);

  if((ret = init_elf_bin(bin_aarch64_fn, &bin_aarch64)) != SUCCESS)
    die("could not initialize the binary (aarch64)", ret);
  if((ret = init_elf_bin(bin_x86_64_fn, &bin_x86_64)) != SUCCESS)
    die("could not initialize the binary (x86-64)", ret);

  if((ret = init_stackmap(bin_aarch64, &sm_aarch64, &num_sm_aarch64)) != SUCCESS)
    die("could not read stackmaps (aarch64)", ret);
  if((ret = init_stackmap(bin_x86_64, &sm_x86_64, &num_sm_x86_64)) != SUCCESS)
    die("could not read stackmaps (x86-64)", ret);

  check_stackmaps(sm_aarch64, num_sm_aarch64, sm_x86_64, num_sm_x86_64);

  free_stackmaps(sm_aarch64, num_sm_aarch64);
  free_stackmaps(sm_x86_64, num_sm_x86_64);
  free_elf_bin(bin_aarch64);
  free_elf_bin(bin_x86_64);

  return 0;
}

