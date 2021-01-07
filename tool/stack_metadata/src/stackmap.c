/**
 * Stack map section parsing & cleanup.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#include <stddef.h>

#include "definitions.h"
#include "stackmap.h"
#include "util.h"

#define LLVM_STACKMAP_SECTION ".llvm_pcn_stackmaps"

static uint64_t stackmap_records_size(void *raw_sm, unsigned num_records)
{
  size_t i;
  void *orig_raw = raw_sm;
  uint16_t *num;

  for(i = 0; i < num_records; i++)
  {
    raw_sm += offsetof(call_site_record, num_locations);
    num = raw_sm; // Number of locations
    raw_sm += 4 + sizeof(live_value) * (*num);
    num = raw_sm; // Number of live outs
    raw_sm += 4 + sizeof(live_out_record) * (*num);
    num = raw_sm; // Number of architecture-specific locations
    raw_sm += 2 + sizeof(arch_live_value) * (*num);
    if((uint64_t)raw_sm % 8)
      raw_sm += 8 - ((uint64_t)raw_sm % 8);
  }

  return raw_sm - orig_raw;
}

static uint64_t read_stackmap_records(void *raw_sm, stack_map_section *sm)
{
  size_t i, off;
  void* orig_raw = raw_sm;

  sm->call_sites = malloc(sizeof(call_site_record) * sm->num_records);
  for(i = 0; i < sm->num_records; i++)
  {
    /* id, func_idx, offset, reserved, num_locations */
    off = offsetof(call_site_record, num_locations);
    memcpy(&sm->call_sites[i], raw_sm, off);
    raw_sm += off;

    /* num_locations, locations, padding */
    memcpy(&sm->call_sites[i].num_locations, raw_sm, 2);
    raw_sm += 2;
    sm->call_sites[i].locations = raw_sm;
    raw_sm += sizeof(live_value) * sm->call_sites[i].num_locations;
    raw_sm += 2; // padding

    /* num_live_outs, live_outs, padding2 */
    memcpy(&sm->call_sites[i].num_live_outs, raw_sm, 2);
    raw_sm += 2;
    sm->call_sites[i].live_outs = raw_sm;
    raw_sm += sizeof(live_out_record) * sm->call_sites[i].num_live_outs;
    raw_sm += 2; // padding2

    /* num_arch_live, arch_live, alignment padding */
    memcpy(&sm->call_sites[i].num_arch_live, raw_sm, 2);
    raw_sm += 2;
    sm->call_sites[i].arch_live = raw_sm;
    raw_sm += sizeof(arch_live_value) * sm->call_sites[i].num_arch_live;
    if((uint64_t)raw_sm % 8)
      raw_sm += 8 - ((uint64_t)raw_sm % 8);
  }

  return raw_sm - orig_raw;
}

ret_t init_stackmap(bin *b, stack_map_section **sm_ptr, size_t *num_sm)
{
  ret_t ret = SUCCESS;
  uint64_t offset, i, j;
  stack_map_section *sm, tmp_sm;
  Elf_Scn *scn;
  GElf_Shdr shdr;
  Elf_Data *data = NULL;

  if(!b || !sm_ptr || !num_sm) return INVALID_ARGUMENT;
  *sm_ptr = NULL;
  *num_sm = 0;

  if(!(scn = get_section_by_name(b->e, LLVM_STACKMAP_SECTION)))
    return FIND_SECTION_FAILED;
  if(gelf_getshdr(scn, &shdr) != &shdr) return READ_ELF_FAILED;
  if(get_num_data_blocks(scn) != 1) return READ_ELF_FAILED;
  if(!(data = elf_getdata(scn, data)))
    return READ_ELF_FAILED;

  if(verbose)
    printf("ELF section '%s': %lu bytes\n", LLVM_STACKMAP_SECTION,
           shdr.sh_size);

  /* Calculate number of stack map records */
  offset = 0;
  while(offset < shdr.sh_size)
  {
    (*num_sm)++;
    memcpy(&tmp_sm, data->d_buf + offset,
           offsetof(stack_map_section, function_records));
    offset += offsetof(stack_map_section, function_records);
    offset += sizeof(function_record) * tmp_sm.num_functions;
    offset += sizeof(uint64_t) * tmp_sm.num_constants;
    offset += stackmap_records_size(data->d_buf + offset, tmp_sm.num_records);
  }

  // Note: we should expect LLVM to generate a stackmap section for each
  // instrumented module
  if(verbose) printf("Found %lu stackmap section(s)\n", *num_sm);

  /* Populate stackmap records. */
  offset = 0;
  sm = malloc(sizeof(stack_map_section) * (*num_sm));
  for(i = 0; i < *num_sm; i++)
  {
    /* Read header & record counts */
    memcpy(&sm[i], data->d_buf + offset,
           offsetof(stack_map_section, function_records));
    offset += offsetof(stack_map_section, function_records);

    if(verbose)
      printf("  Stackmap v%d, %u function(s), %u constant(s), %u record(s)\n",
             sm[i].version, sm[i].num_functions, sm[i].num_constants,
             sm[i].num_records);

    /* Read function_records */
    sm[i].function_records = data->d_buf + offset;
    offset += sizeof(function_record) * sm[i].num_functions;

    if(verbose)
      for(j = 0; j < sm[i].num_functions; j++)
        printf("    Function %lu: %p, stack frame size = %lu byte(s), "
               "%u unwinding records\n", j,
               (void*)sm[i].function_records[j].func_addr,
               sm[i].function_records[j].stack_size,
               sm[i].function_records[j].num_unwind);

    /* Read constants */
    sm[i].constants = data->d_buf + offset;
    offset += sizeof(uint64_t) * sm[i].num_constants;

    if(verbose)
      for(j = 0; j < sm[i].num_constants; j++)
        printf("    Constant %lu: %lu\n", j, sm[i].constants[j]);

    /* Read stack map records */
    offset += read_stackmap_records(data->d_buf + offset, &sm[i]);

    if(verbose)
      for(j = 0; j < sm[i].num_records; j++)
        printf("    Stack map %lu: %lu "
               "(function %u), "
               "function offset = %u byte(s), "
               "%u location(s), "
               "%u live-out(s), "
               "%u arch-specific live value(s)\n",
               j, sm[i].call_sites[j].id,
               sm[i].call_sites[j].func_idx,
               sm[i].call_sites[j].offset,
               sm[i].call_sites[j].num_locations,
               sm[i].call_sites[j].num_live_outs,
               sm[i].call_sites[j].num_arch_live);
  }

  *sm_ptr = sm;
  return ret;
}

ret_t free_stackmaps(stack_map_section *sm, size_t num_sm)
{
  size_t i;

  if(!sm) return INVALID_ARGUMENT;

  for(i = 0; i < num_sm; i++)
    free(sm[i].call_sites);
  free(sm);

  return SUCCESS;
}

