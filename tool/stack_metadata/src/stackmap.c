/**
 * Stack map section parsing & cleanup.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#include "definitions.h"
#include "stackmap.h"
#include "util.h"

#define LLVM_STACKMAP_SECTION ".llvm_stackmaps"

static uint64_t stackmap_records_size(void *raw_sm, unsigned num_records)
{
  size_t i;
  void *orig_raw = raw_sm;
  sm_stack_map_record tmp;

  for(i = 0; i < num_records; i++)
  {
    raw_sm += offsetof(sm_stack_map_record, locations);
    tmp.locations = raw_sm;
    raw_sm += 2 + sizeof(call_site_value) * tmp.locations->num;
    raw_sm += 2;
    tmp.live_outs = raw_sm;
    raw_sm += 2 + sizeof(sm_live_out_record) * tmp.live_outs->num;
    if((uint64_t)raw_sm % 8)
      raw_sm += 8 - ((uint64_t)raw_sm % 8);
  }

  return raw_sm - orig_raw;
}

static uint64_t read_stackmap_records(void *raw_sm, stack_map *sm)
{
  size_t i;
  void* orig_raw = raw_sm;

  sm->stack_maps = malloc(sizeof(sm_stack_map_record) * sm->num_records);
  for(i = 0; i < sm->num_records; i++)
  {
    /* id, func_idx, offset, reserved */
    memcpy(&sm->stack_maps[i], raw_sm, offsetof(sm_stack_map_record, locations));
    raw_sm += offsetof(sm_stack_map_record, locations);

    /* locations */
    sm->stack_maps[i].locations = raw_sm;
    raw_sm += 2 + sizeof(call_site_value) * sm->stack_maps[i].locations->num;

    /* padding, live_outs, final padding */
    raw_sm += 2;
    sm->stack_maps[i].live_outs = raw_sm;
    raw_sm += 2 + sizeof(sm_live_out_record) * sm->stack_maps[i].live_outs->num;
    if((uint64_t)raw_sm % 8)
      raw_sm += 8 - ((uint64_t)raw_sm % 8);
  }

  return raw_sm - orig_raw;
}

ret_t init_stackmap(bin *b, stack_map **sm_ptr, size_t *num_sm)
{
  ret_t ret = SUCCESS;
  uint64_t offset, i, j;
  stack_map *sm, tmp_sm;
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
    printf("Section '%s': %lu bytes\n", LLVM_STACKMAP_SECTION, shdr.sh_size);

  /* Calculate number of stack map records */
  offset = 0;
  while(offset < shdr.sh_size)
  {
    (*num_sm)++;
    memcpy(&tmp_sm, data->d_buf + offset, offsetof(stack_map, stack_sizes));
    offset += offsetof(stack_map, stack_sizes);
    offset += sizeof(sm_stack_size_record) * tmp_sm.num_functions;
    offset += sizeof(uint64_t) * tmp_sm.num_constants;
    offset += stackmap_records_size(data->d_buf + offset, tmp_sm.num_records);
    printf("Current offset: %lx\n", offset);
  }

  if(verbose) printf("Found %lu stackmap section(s)\n", *num_sm);

  /* Populate stackmap records. */
  offset = 0;
  sm = malloc(sizeof(stack_map) * (*num_sm));
  for(i = 0; i < *num_sm; i++)
  {
    /* Read header & record counts */
    memcpy(&sm[i], data->d_buf + offset, offsetof(stack_map, stack_sizes));
    offset += offsetof(stack_map, stack_sizes);

    if(verbose)
      printf("  Stackmap v%d, %u function(s), %u constant(s), %u record(s)\n",
             sm[i].header.version, sm[i].num_functions, sm[i].num_constants,
             sm[i].num_records);

    /* Read stack_size_records */
    sm[i].stack_sizes = data->d_buf + offset;
    offset += sizeof(sm_stack_size_record) * sm[i].num_functions;

    if(verbose)
      for(j = 0; j < sm[i].num_functions; j++)
        printf("    Function %lu: %p, stack frame size = %lu byte(s)\n", j,
               (void*)sm[i].stack_sizes[j].func_addr,
               sm[i].stack_sizes[j].stack_size);

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
        printf("    Stack map %lu: %lu (function %u), "
               "function offset = %u byte(s), "
               "%u location(s), %u live-out(s)\n",
               j, sm[i].stack_maps[j].id, sm[i].stack_maps[j].func_idx,
               sm[i].stack_maps[j].offset, sm[i].stack_maps[j].locations->num,
               sm[i].stack_maps[j].live_outs->num);

    printf("Parsing current offset: %lx\n", offset);
  }

  *sm_ptr = sm;
  return ret;
}

ret_t free_stackmaps(stack_map *sm, size_t num_sm)
{
  size_t i;

  if(!sm) return INVALID_ARGUMENT;

  for(i = 0; i < num_sm; i++)
    free(sm[i].stack_maps);
  free(sm);

  return SUCCESS;
}

