/**
 * Check LLVM stackmaps to ensure that the same number of stackmaps and live
 * variable locations for each stackmap were generated for all binaries.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 7/7/2016
 */

#include <stdio.h>
#include <unistd.h>

#include "definitions.h"
#include "bin.h"
#include "stackmap.h"
#include "util.h"
#include "het_bin.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

///////////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////////

static const char *args = "ha:x:p:r:";
static const char *help =
"check-stackmaps - check LLVM stackmap sections for matching metadata across "
"binaries\n\n\
\
Usage: ./check-stackmaps [ OPTIONS ]\n\
Options:\n\
\t-h      : print help & exit\n\
\t-a file : name of AArch64 executable\n\
\t-x file : name of x86-64 executable\n\n\
\t-p file : name of PowerPC64 executable\n\n\
\t-r file : name of RISCV64 executable\n\n\
\
Note: this tool assumes binaries have been through the alignment tool, as it \
checks stackmaps based on function addresses";

static const char *bin_aarch64_fn = NULL;
static const char *bin_powerpc64_fn = NULL;
static const char *bin_riscv64_fn = NULL;
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
  int arg, count = 0;

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
    case 'p':
      bin_powerpc64_fn = optarg;
      break;
    case 'r':
      bin_riscv64_fn = optarg;
      break;
    default:
      fprintf(stderr, "Unknown argument '%c'\n", arg);
      break;
    }
  }

  count += (bin_aarch64_fn ? 1 : 0);
  count += (bin_powerpc64_fn ? 1 : 0);
  count += (bin_riscv64_fn ? 1 : 0);
  count += (bin_x86_64_fn ? 1 : 0);
  if(count < 2)
    die("please specify at least 2 binaries "
        "(run with -h for more information)",
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
ret_t check_stackmaps(bin *a, stack_map_section *sm_a, size_t num_sm_a,
                      bin *b, stack_map_section *sm_b, size_t num_sm_b)
{
  ret_t ret = SUCCESS;
  char buf[BUF_SIZE];
  size_t i, j, k, l, num_sm, num_records;
  uint64_t func_a, func_b;
  uint8_t flag_a, flag_b;
  uint32_t size_a, size_b;
  GElf_Sym sym_a, sym_b;
  const char *sym_a_name, *sym_b_name;

  /*
   * Errors here indicate there's probably an object file that didn't have the
   * stackmap-insertion pass run over its IR, i.e., there's no stackmap section
   * for one of the object files included in the binary.
   */
  if(num_sm_a != num_sm_b)
  {
    snprintf(buf, BUF_SIZE, "number of stackmaps doesn't match (%lu vs. %lu)",
             num_sm_a, num_sm_b);
    warn(buf);
    ret = INVALID_METADATA;
  }

  /* Iterate over all accumulated stackmap sections (one per .o file) */
  num_sm = MIN(num_sm_a, num_sm_b);
  for(i = 0; i < num_sm; i++)
  {
    /*
     * Errors here indicate different numbers of stackmap intrinsics inserted
     * into the IR.
     */
    if(sm_a[i].num_records != sm_b[i].num_records)
    {
      snprintf(buf, BUF_SIZE,
               "number of records for stackmap section %lu doesn't match "
               "(%u vs. %u)",
               i, sm_a[i].num_records, sm_b[i].num_records);
      warn(buf);
      ret = INVALID_METADATA;
    }

    /*
     * Iterate over all records in the stackmap, i.e., all
     * llvm.experimental.stackmap intrinsics
     */
    num_records = MIN(sm_a[i].num_records, sm_b[i].num_records);
    for(j = 0; j < num_records; j++)
    {
      const function_record *fr;

      fr = &sm_a[i].function_records[sm_a[i].call_sites[j].func_idx];
      func_a = fr->func_addr;
      sym_a = get_sym_by_addr(a->e, func_a, STT_FUNC);
      sym_a_name = get_sym_name(a->e, sym_a);

      fr = &sm_b[i].function_records[sm_b[i].call_sites[j].func_idx];
      func_b = fr->func_addr;
      sym_b = get_sym_by_addr(b->e, func_b, STT_FUNC);
      sym_b_name = get_sym_name(b->e, sym_b);

      /*
       * Errors here indicate stackmaps inside of different functions, or
       * function misalignments.
       */
      if(func_a != func_b)
      {
        snprintf(buf, BUF_SIZE,
                 "stackmap %lu corresponds to different functions: "
                 "%s/%lx vs. %s/%lx", sm_a[i].call_sites[j].id,
                 sym_a_name, func_a, sym_b_name, func_b);
        warn(buf);
        ret = INVALID_METADATA;
      }
      else
      {
        /*
         * The raw location record count may be different, so count
         * non-duplicated records, i.e., ignore backing stack slot locations.
         */
        unsigned num_a = 0, num_b = 0;
        for(k = 0; k < sm_a[i].call_sites[j].num_locations; k++)
          if(!sm_a[i].call_sites[j].locations[k].is_duplicate)
            num_a++;

        for(k = 0; k < sm_b[i].call_sites[j].num_locations; k++)
          if(!sm_b[i].call_sites[j].locations[k].is_duplicate)
            num_b++;

        /*
         * Errors here indicate different numbers of live values at the stackmap
         * intrinsic call site.
         */
        if(num_a != num_b)
        {
          snprintf(buf, BUF_SIZE,
                   "%s: stackmap %lu has different numbers of location records "
                   "(%u vs. %u)",
                   sym_a_name, sm_a[i].call_sites[j].id, num_a, num_b);
          warn(buf);
          ret = INVALID_METADATA;
        }

        /*
         * Iterate over all location records (i.e., all live values) at a call
         * site.  Errors point to different live values/different orderings of
         * live values at the site.
         */
        for(k = 0, l = 0; k < num_a && l < num_b; k++, l++)
        {
          flag_a = sm_a[i].call_sites[j].locations[k].size;
          flag_b = sm_b[i].call_sites[j].locations[l].size;
          if(flag_a != flag_b)
          {
            snprintf(buf, BUF_SIZE, "%s: stackmap %lu, location %lu/%lu has "
                                    "different size (%u vs. %u)",
                     sym_a_name, sm_a[i].call_sites[j].id, k, l, flag_a,
                     flag_b);
            warn(buf);
            ret = INVALID_METADATA;
          }

          flag_a = sm_a[i].call_sites[j].locations[k].is_ptr;
          flag_b = sm_b[i].call_sites[j].locations[l].is_ptr;
          if(flag_a != flag_b)
          {
            snprintf(buf, BUF_SIZE, "%s: stackmap %lu, location %lu/%lu has "
                                    "mismatched pointer flag (%u vs. %u)",
                     sym_a_name, sm_a[i].call_sites[j].id, k, l, flag_a,
                     flag_b);
            warn(buf);
            ret = INVALID_METADATA;
          }

          flag_a = sm_a[i].call_sites[j].locations[k].is_alloca;
          flag_b = sm_b[i].call_sites[j].locations[l].is_alloca;
          if(flag_a != flag_b)
          {
            snprintf(buf, BUF_SIZE, "%s: stackmap %lu, location %lu/%lu has "
                                    "mismatched alloca flag (%u vs. %u)",
                     sym_a_name, sm_a[i].call_sites[j].id, k, l, flag_a,
                     flag_b);
            warn(buf);
            ret = INVALID_METADATA;
          }

          if(flag_a && flag_b)
          {
            size_a = sm_a[i].call_sites[j].locations[k].alloca_size;
            size_b = sm_b[i].call_sites[j].locations[l].alloca_size;
            if(size_a != size_b)
            {
              snprintf(buf, BUF_SIZE, "%s: stackmap %lu, location %lu/%lu has "
                                      "different size (%u vs. %u)",
                       sym_a_name, sm_a[i].call_sites[j].id, k, l,
                       size_a, size_b);
              warn(buf);
              ret = INVALID_METADATA;
            }
          }

          /* Skip backing stack slot records */
          while(sm_a[i].call_sites[j].locations[k+1].is_duplicate) k++;
          while(sm_b[i].call_sites[j].locations[l+1].is_duplicate) l++;
        }

        // Note: don't check architecture-specific live values because they're
        // by nature going to be different
      }
    }
  }

  return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Driver
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
  ret_t ret;
  bin *bin_aarch64 = NULL, *bin_powerpc64 = NULL, *bin_riscv64 = NULL, *bin_x86_64 = NULL;
  stack_map_section *sm_aarch64 = NULL, *sm_powerpc64 = NULL, *sm_riscv64 = NULL, *sm_x86_64 = NULL;
  size_t num_sm_aarch64 = 0, num_sm_powerpc64 = 0, num_sm_riscv64 = 0, num_sm_x86_64 = 0, i, j, num;
  char buf[1024];

  parse_args(argc, argv);

  if(elf_version(EV_CURRENT) == EV_NONE)
    die("could not initialize libELF", INVALID_ELF_VERSION);

  if(bin_aarch64_fn)
  {
    ret = init_elf_bin(bin_aarch64_fn, &bin_aarch64);
    if(ret != SUCCESS) die("could not initialize the binary (aarch64)", ret);
    ret = init_stackmap(bin_aarch64, &sm_aarch64, &num_sm_aarch64);
    if(ret != SUCCESS) die("could not read stackmaps (aarch64)", ret);
  }

  if(bin_powerpc64_fn)
  {
    ret = init_elf_bin(bin_powerpc64_fn, &bin_powerpc64);
    if(ret != SUCCESS) die("could not initialize the binary (powerpc64)", ret);
    ret = init_stackmap(bin_powerpc64, &sm_powerpc64, &num_sm_powerpc64);
    if(ret != SUCCESS) die("could not read stackmaps (powerpc64)", ret);
  }

  if(bin_riscv64_fn)
  {
    ret = init_elf_bin(bin_riscv64_fn, &bin_riscv64);
    if(ret != SUCCESS) die("could not initialize the binary (riscv64)", ret);
    ret = init_stackmap(bin_riscv64, &sm_riscv64, &num_sm_riscv64);
    if(ret != SUCCESS) die("could not read stackmaps (riscv64)", ret);
  }

  if(bin_x86_64_fn)
  {
    ret = init_elf_bin(bin_x86_64_fn, &bin_x86_64);
    if(ret != SUCCESS) die("could not initialize the binary (x86-64)", ret);
    ret = init_stackmap(bin_x86_64, &sm_x86_64, &num_sm_x86_64);
    if(ret != SUCCESS) die("could not read stackmaps (x86-64)", ret);
  }

  bin *binaries[] = { bin_aarch64, bin_powerpc64, bin_riscv64, bin_x86_64 };
  stack_map_section *stackmaps[] = { sm_aarch64, sm_powerpc64, sm_riscv64, sm_x86_64 };
  size_t num_stackmaps[] = { num_sm_aarch64, num_sm_powerpc64, num_sm_riscv64, num_sm_x86_64 };
  num = sizeof(binaries) / sizeof(bin *);

  for(i = 0; i < num; i++)
  {
    for(j = i + 1; j < num; j++)
    {
      if(binaries[i] && binaries[j])
      {
        ret = check_stackmaps(binaries[i], stackmaps[i], num_stackmaps[i],
                              binaries[j], stackmaps[j], num_stackmaps[j]);
        if(ret != SUCCESS)
        {
          snprintf(buf, sizeof(buf), "stackmaps in '%s' & '%s' differ",
                   binaries[i]->name, binaries[j]->name);
          die(buf, ret);
        }
      }
    }
  }

  if(bin_aarch64_fn)
  {
    free_stackmaps(sm_aarch64, num_sm_aarch64);
    free_elf_bin(bin_aarch64);
  }

  if(bin_powerpc64_fn)
  {
    free_stackmaps(sm_powerpc64, num_sm_powerpc64);
    free_elf_bin(bin_powerpc64);
  }

  if(bin_riscv64_fn)
  {
    free_stackmaps(sm_riscv64, num_sm_riscv64);
    free_elf_bin(bin_riscv64);
  }

  if(bin_x86_64_fn)
  {
    free_stackmaps(sm_x86_64, num_sm_x86_64);
    free_elf_bin(bin_x86_64);
  }

  return 0;
}

