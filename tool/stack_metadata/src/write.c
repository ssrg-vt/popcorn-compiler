/**
 * Encoding stack transformation meta-data into ELF binaries.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/7/2016
 */

#include "write.h"
#include "util.h"
#include "het_bin.h"

///////////////////////////////////////////////////////////////////////////////
// Private API
///////////////////////////////////////////////////////////////////////////////

/**
 * Generate the call-site metadata section.
 * @param start_id beginning ID number for site tags
 * @param sm stack map section
 * @param num_sm number of stack map sections pointed to by sm
 * @param num_sites number of call sites copied to cs
 * @param cs call site meta-data
 * @param num_live number of location records copied to live
 * @param live live variable location information
 * @param num_arch_live number of arch-specific live value records copied to arch_live
 * @param arch_live architecture-specific live value information
 * @param num_func number of function metadata entries
 * @param records function metadata
 * @return true if the data was created, false otherwise (*cs & *locs will be
 *         NULL if creation failed)
 */
static bool
create_call_site_metadata(uint64_t start_id,
                          stack_map_section *sm, size_t num_sm,
                          size_t *num_sites, call_site **cs,
                          size_t *num_live, live_value **live,
                          size_t *num_arch_live, arch_live_value **arch_live,
                          size_t num_func, const function_record *records);

/**
 * Comparison function to sort function metadata entries by address.  Called by
 * qsort().
 * @param a first function metadata entry
 * @param b second function metadata entry
 * @return -1 if a < b, 0 if a == b or 1 if a > b
 */
static int sort_func_by_addr(const void *a, const void *b);

/**
 * Comparison function to sort sites by ID.  Called by qsort().
 * @param a first call site record
 * @param b second call site record
 * @return -1 if a < b, 0 if a == b or 1 if a > b
 */
static int sort_id(const void *a, const void *b);

/**
 * Comparison function to sort sites by instruction address.  Called by qsort().
 * @param a first call site record
 * @param b second call site record
 * @return -1 if a < b, 0 if a == b or 1 if a > b
 */
static int sort_addr(const void *a, const void *b);

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

// Note: assumptions for updating function records
//
//  1. Within a single file, LLVM emits function records and their associated
//     metadata (unwind locations, stack slots) into their respective sections
//     in the same order.  In other words, we do *not* see function 0's unwind
//     records and stack slots after function 1's metadata.  Everything should
//     be in the same order in all the metadata sections.
//
//  2. When linking, the linker generates the combined metadata sections in
//     file order.  In other words, we do *not* see file 0's metadata sections
//     after file 1's metadata sections.

ret_t update_function_records(bin *b, const char *sec)
{
  size_t num_records, i, unwind_offset = 0, stack_slot_offset = 0, record_size;
  char sec_name[BUF_SIZE];
  Elf_Scn *scn;
  Elf64_Shdr *shdr;
  function_record *fr;

  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_FUNCTIONS);
  if(!(scn = get_section_by_name(b->e, sec_name))) return FIND_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
  if(!shdr->sh_size) return INVALID_METADATA;
  if(!(fr = get_section_data(scn))) return READ_ELF_FAILED;

  record_size = sizeof(function_record);
  num_records = shdr->sh_size / record_size;
  if(verbose) printf("Found %lu record(s) in the function metadata section\n",
                     num_records);

  /*
   * Update the offsets in the records because the records from all object
   * files have been smushed together by the linker
   */
  for(i = 0; i < num_records; i++)
  {
    // TODO if the function's address isn't resolved until runtime (dynamic
    // linker), we need to update relocation entries for the function records
    // (REL, function sym) so the linker updates the rewriting metadata with
    // the correct function addresses at runtime
    fr[i].frame_size = cfa_correction(b->arch, fr[i].frame_size);
    fr[i].unwind.offset = unwind_offset;
    unwind_offset += fr[i].unwind.num;
    fr[i].stack_slot.offset = stack_slot_offset;
    stack_slot_offset += fr[i].stack_slot.num;

    if(verbose)
      printf("  Function %lu: 0x%lx, code size=%u, frame size=%u, "
             "%u unwinding records, %u stack slots\n",
             i, fr[i].addr, fr[i].code_size, fr[i].frame_size,
             fr[i].unwind.num, fr[i].stack_slot.num);
  }

  /* Sort by address & update the section */
  qsort(fr, num_records, record_size, sort_func_by_addr);
  return update_section(b->e, scn, num_records, record_size, fr);
}

ret_t add_sections(bin *b,
                   stack_map_section *sm,
                   size_t num_sm,
                   const char *sec,
                   uint64_t start_id)
{
  size_t num_shdr, i, added = 0, cur_offset, num_sites,
         num_live, num_arch_live, num_funcs;
  char sec_name[BUF_SIZE];
  call_site *id_sites, *addr_sites;
  live_value *live_vals;
  arch_live_value *archlive;
  Elf64_Ehdr *ehdr;
  Elf64_Shdr *shdr;
  Elf_Scn *scn;
  const function_record *funcs;
  ret_t ret;

  /* Generate the section's data (not yet sorted by anything) */
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_FUNCTIONS);
  if(!(scn = get_section_by_name(b->e, sec_name))) return FIND_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
  if(!shdr->sh_size) return INVALID_METADATA;
  num_funcs = shdr->sh_size / sizeof(function_record);
  if(!(funcs = get_section_data(scn))) return READ_ELF_FAILED;
  if(!create_call_site_metadata(start_id, sm, num_sm,
                                &num_sites, &id_sites,
                                &num_live, &live_vals,
                                &num_arch_live, &archlive,
                                num_funcs, funcs))
    return CREATE_METADATA_FAILED;

  /* Add call site section sorted by ID */
  qsort(id_sites, num_sites, sizeof(call_site), sort_id);
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_ID);
  if((scn = get_section_by_name(b->e, sec_name)))
    ret = update_section(b->e, scn, num_sites, sizeof(call_site), id_sites);
  else
    ret = add_section(b->e, sec_name, num_sites, sizeof(call_site), id_sites);
  if(ret) return ret;
  added++;

  /* Add call site section sorted by address */
  addr_sites = malloc(sizeof(call_site) * num_sites);
  memcpy(addr_sites, id_sites, sizeof(call_site) * num_sites);
  qsort(addr_sites, num_sites, sizeof(call_site), sort_addr);
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_ADDR);
  if((scn = get_section_by_name(b->e, sec_name)))
    ret = update_section(b->e, scn, num_sites, sizeof(call_site), addr_sites);
  else
    ret = add_section(b->e, sec_name, num_sites, sizeof(call_site), addr_sites);
  if(ret) return ret;
  added++;

  /* Add live-value location section. */
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_LIVE);
  if((scn = get_section_by_name(b->e, sec_name)))
    ret = update_section(b->e, scn, num_live, sizeof(live_value), live_vals);
  else
    ret = add_section(b->e, sec_name, num_live, sizeof(live_value), live_vals);
  if(ret) return ret;
  added++;

  /* Add architecture-specific location section. */
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_ARCH);
  if((scn = get_section_by_name(b->e, sec_name)))
    ret = update_section(b->e, scn, num_arch_live, sizeof(arch_live_value), archlive);
  else
    ret = add_section(b->e, sec_name, num_arch_live, sizeof(arch_live_value), archlive);
  if(ret) return ret;
  added++;

  /* Calculate offset of last non-stack-transform section */
  if(elf_getshdrnum(b->e, &num_shdr) == -1) return READ_ELF_FAILED;
  if(!(scn = elf_getscn(b->e, num_shdr - (added + 1)))) return READ_ELF_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
  cur_offset = shdr->sh_offset + shdr->sh_size;

  /* Update stack transformation section offsets */
  for(i = num_shdr - added; i < num_shdr; i++)
  {
    if(!(scn = elf_getscn(b->e, i))) return READ_ELF_FAILED;
    if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
    shdr->sh_offset = cur_offset;
    cur_offset += shdr->sh_size;
  }

  /* Update section header table offset */
  if(!(ehdr = elf64_getehdr(b->e))) return READ_ELF_FAILED;
  ehdr->e_shoff = cur_offset;
  elf_flagehdr(b->e, ELF_C_SET, ELF_F_DIRTY);
  if(verbose) printf("Section table moved to 0x%lx\n", cur_offset);

  /* Write changes back to disk.  LibELF now controls the data we malloc'd. */
  if(elf_update(b->e, ELF_C_WRITE) < 0) return WRITE_ELF_FAILED;
  return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// Private API
///////////////////////////////////////////////////////////////////////////////

static bool
create_call_site_metadata(uint64_t start_id,
                          stack_map_section *sm, size_t num_sm,
                          size_t *num_sites, call_site **cs,
                          size_t *num_live, live_value **live,
                          size_t *num_arch_live, arch_live_value **arch_live,
                          size_t num_func, const function_record *records)
{
  size_t i, j, sites_num = 0, loc_num = 0, arch_num = 0, cur;
  const stackmap_function *sm_func;
  const function_record *full_fr;
  call_site_record *site_record;
  call_site *sites;
  live_value *locs;
  arch_live_value *archlive;

  if(!num_sites || !cs || !num_live || !live || !num_arch_live ||
     !arch_live || !records)
    return false;

  /* Calculate number of call site & location records */
  for(i = 0; i < num_sm; i++)
  {
    sites_num += sm[i].num_records;
    for(j = 0; j < sm[i].num_records; j++)
    {
      loc_num += sm[i].call_sites[j].num_locations;
      arch_num += sm[i].call_sites[j].num_arch_live;
    }
  }
  sites = malloc(sizeof(call_site) * sites_num);
  locs = malloc(sizeof(live_value) * loc_num);
  archlive = malloc(sizeof(arch_live_value) * arch_num);

  if(verbose)
    printf("Creating metadata for %lu call sites, %lu location records & %lu "
           "arch-specific locations\n",
           sites_num, loc_num, arch_num);

  /* Populate call site & location record data. */
  for(i = 0, loc_num = 0, arch_num = 0, cur = 0; i < num_sm; i++)
  {
    for(j = 0; j < sm[i].num_records; j++, cur++)
    {
      /*
       * Find full function metadata entry.  We won't use the stackmap function
       * records as they don't have all the information we need.
       */
      site_record = &sm[i].call_sites[j];
      sm_func = &sm[i].function_records[site_record->func_idx];
      full_fr = get_func_metadata(sm_func->addr, num_func, records);
      if(!full_fr) return false;

      /* Populate call site record */
      if(site_record->id == UINT64_MAX ||
         site_record->id == UINT64_MAX - 1 ||
         site_record->id == UINT64_MAX - 2)
        sites[cur].id = site_record->id;
      else
        sites[cur].id = start_id++;
      sites[cur].func = full_fr - records;
      sites[cur].unhandled = site_record->unhandled;
      // TODO if the function's address isn't resolved until runtime (dynamic
      // linker), we need to add a relocation entry for the call site (RELA,
      // function sym + offset) so the linker updates the rewriting metadata
      // with the correct information
      sites[cur].addr = full_fr->addr + site_record->offset;
      sites[cur].live.num = site_record->num_locations;
      sites[cur].live.offset = loc_num;
      sites[cur].arch_live.num = site_record->num_arch_live;
      sites[cur].arch_live.offset = arch_num;

      /* Copy live value location records to new section */
      memcpy(&locs[loc_num], site_record->locations,
             sizeof(live_value) * sites[cur].live.num);
      loc_num += sites[cur].live.num;

      /* Copy arch-specific live value records to new section */
      memcpy(&archlive[arch_num], site_record->arch_live,
             sizeof(arch_live_value) * sites[cur].arch_live.num);
      arch_num += sites[cur].arch_live.num;
    }
  }

  *num_sites = sites_num;
  *cs = sites;
  *num_live = loc_num;
  *live = locs;
  *num_arch_live = arch_num;
  *arch_live = archlive;

  return true;
}

static int sort_func_by_addr(const void *a, const void *b)
{
  const function_record *ua_a = (const function_record*)a;
  const function_record *ua_b = (const function_record*)b;

  if(ua_a->addr < ua_b->addr) return -1;
  else if(ua_a->addr == ua_b->addr) return 0;
  else return 1;
}

static int sort_id(const void *a, const void *b)
{
  const call_site *cs_a = (const call_site*)a;
  const call_site *cs_b = (const call_site*)b;

  if(cs_a->id < cs_b->id) return -1;
  else if(cs_a->id == cs_b->id) return 0;
  else return 1;
}

static int sort_addr(const void *a, const void *b)
{
  const call_site *cs_a = (const call_site*)a;
  const call_site *cs_b = (const call_site*)b;

  if(cs_a->addr < cs_b->addr) return -1;
  else if(cs_a->addr == cs_b->addr) return 0;
  else return 1;
}

