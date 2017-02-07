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
 * @param b a binary descriptor
 * @param sm stack map data
 * @param num_sm number of stack maps pointed to by sm
 * @param start_id beginning ID number for site tags
 * @param num_sites number of call sites copied to cs
 * @param cs call site meta-data
 * @param num_live number of location records copied to live
 * @param live live variable location information
 * @param num_consts number of arch-specific constant records copied to consts
 * @param consts architecture-specific value information
 * @param unwind_addr function unwinding information address ranges
 * @param num_addrs number of function unwinding information entries
 * @return true if the data was created, false otherwise (*cs & *locs will be
 *         NULL if creation failed)
 */
static bool
create_call_site_metadata(bin *b,
                          stack_map *sm,
                          size_t num_sm,
                          uint64_t start_id,
                          size_t *num_sites, call_site **cs,
                          size_t *num_live, call_site_value **live,
                          size_t *num_consts, arch_const_value **consts,
                          size_t num_addrs, const unwind_addr *addrs);

/**
 * Comparison function to sort unwind address ranges by address.  Called by
 * qsort().
 * @param a first unwind address range
 * @param b second unwind address range
 * @return -1 if a < b, 0 if a == b or 1 if a > b
 */
static int sort_unwind_addr(const void *a, const void *b);

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

ret_t update_function_addr(bin *b, const char *sec)
{
  size_t num_records, i, cur_offset = 0, record_size;
  Elf_Scn *scn;
  Elf64_Shdr *shdr;
  unwind_addr *ua;

  if(!(scn = get_section_by_name(b->e, sec))) return FIND_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
  if(!shdr->sh_size || !shdr->sh_entsize) return INVALID_METADATA;
  if(!(ua = get_section_data(scn))) return READ_ELF_FAILED;

  num_records = shdr->sh_size / shdr->sh_entsize;
  record_size = shdr->sh_entsize;
  if(verbose) printf("Found %lu records in the unwind address range section\n",
                     num_records);

  /*
   * Update the offsets in the records because the records from all object
   * files have been smushed together by the linker
   */
  for(i = 0; i < num_records; i++)
  {
    ua[i].unwind_offset = cur_offset;
    cur_offset += ua[i].num_unwind;
  }

  /* Sort by address & update the section */
  qsort(ua, num_records, record_size, sort_unwind_addr);
  return update_section(b->e, scn, num_records, record_size, ua);
}

ret_t add_sections(bin *b,
                   stack_map *sm,
                   size_t num_sm,
                   const char *sec,
                   uint64_t start_id,
                   const char *unwind_sec)
{
  size_t num_shdr, i, added = 0, cur_offset, num_sites,
         num_live, num_const, num_unwind;
  char sec_name[BUF_SIZE];
  call_site *id_sites, *addr_sites;
  call_site_value *live_vals;
  arch_const_value *archconsts;
  Elf64_Ehdr *ehdr;
  Elf64_Shdr *shdr;
  Elf_Scn *scn;
  const unwind_addr *unwind;
  ret_t ret;

  /* Generate the section's data (not yet sorted by anything) */
  if(!(scn = get_section_by_name(b->e, unwind_sec))) return FIND_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
  if(!shdr->sh_size || !shdr->sh_entsize) return INVALID_METADATA;
  num_unwind = shdr->sh_size / shdr->sh_entsize;
  if(!(unwind = get_section_data(scn))) return READ_ELF_FAILED;
  if(!create_call_site_metadata(b,
                                sm,
                                num_sm,
                                start_id,
                                &num_sites, &id_sites,
                                &num_live, &live_vals,
                                &num_const, &archconsts,
                                num_unwind, unwind))
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
    ret = update_section(b->e, scn, num_live, sizeof(call_site_value), live_vals);
  else
    ret = add_section(b->e, sec_name, num_live, sizeof(call_site_value), live_vals);
  if(ret) return ret;
  added++;

  /* Add architecture-specific location section. */
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_ARCH);
  if((scn = get_section_by_name(b->e, sec_name)))
    ret = update_section(b->e, scn, num_const, sizeof(arch_const_value), archconsts);
  else
    ret = add_section(b->e, sec_name, num_const, sizeof(arch_const_value), archconsts);
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
create_call_site_metadata(bin *b,
                          stack_map *sm,
                          size_t num_sm,
                          uint64_t start_id,
                          size_t *num_sites, call_site **cs,
                          size_t *num_live, call_site_value **live,
                          size_t *num_consts, arch_const_value **consts,
                          size_t num_addrs, const unwind_addr *addrs)
{
  size_t i, j, sites_num = 0, loc_num = 0, const_num = 0, cur;
  const stack_size_record *ssr;
  GElf_Sym mainthr, thread;
  call_site *sites;
  call_site_value *locs;
  const unwind_addr *ua;
  arch_const_value *archconsts;

  if(!num_sites || !cs || !num_live || !live) return false;

  mainthr = get_sym_by_name(b->e, START_MAIN);
  if(!mainthr.st_size) return false;
  thread = get_sym_by_name(b->e, START_THREAD);

  /* Calculate number of stackmap & location records */
  for(i = 0; i < num_sm; i++)
  {
    sites_num += sm[i].num_records;
    for(j = 0; j < sm[i].num_records; j++)
    {
      loc_num += sm[i].stack_map_records[j].num_locations;
      const_num += sm[i].stack_map_records[j].num_arch_consts;
    }
  }
  sites_num += (thread.st_size ? 2 : 1);
  sites = malloc(sizeof(call_site) * sites_num);
  locs = malloc(sizeof(call_site_value) * loc_num);
  archconsts = malloc(sizeof(arch_const_value) * const_num);

  if(verbose)
    printf("Creating metadata for %lu call site & %lu location records\n",
           sites_num, loc_num);

  /* Populate call site & location record data. */
  for(i = 0, loc_num = 0, const_num = 0, cur = 0; i < num_sm; i++)
  {
    for(j = 0; j < sm[i].num_records; j++, cur++)
    {
      ssr = &sm[i].stack_size_records[sm[i].stack_map_records[j].func_idx];

      /* Populate call site record */
      sites[cur].id = start_id++;
      sites[cur].addr = ssr->func_addr + sm[i].stack_map_records[j].offset;
      sites[cur].fbp_offset = ssr->stack_size - fp_offset(b->arch);
      sites[cur].num_unwind = ssr->num_unwind;
      sites[cur].num_live = sm[i].stack_map_records[j].num_locations;
      sites[cur].num_arch_const = sm[i].stack_map_records[j].num_arch_consts;
      sites[cur].live_offset = loc_num;
      sites[cur].arch_const_offset = const_num;

      /* Find unwinding information offset */
      ua = get_func_unwind_data(sites[cur].addr, num_addrs, addrs);
      if(!ua) return false;
      sites[cur].unwind_offset = ua->unwind_offset;

      /* Copy live value location records to new section */
      memcpy(&locs[loc_num], sm[i].stack_map_records[j].locations,
             sizeof(call_site_value) * sites[cur].num_live);
      loc_num += sites[cur].num_live;

      /* Copy arch-specific constant records to new section */
      memcpy(&archconsts[const_num], sm[i].stack_map_records[j].arch_consts,
             sizeof(arch_const_value) * sites[cur].num_arch_const);
      const_num += sites[cur].num_arch_const;
    }
  }

  /*
   * Add entries for main thread's __libc_start_main & thread's start.  We
   * denote stack beginning call site records with an ID Of UINT64_MAX for
   * the main thread & UINT64_MAX-1 for spawned threads.
   */
  sites[cur].id = UINT64_MAX;
  sites[cur].addr = mainthr.st_value + main_start_offset(b->arch);
  sites[cur].fbp_offset = 0;
  sites[cur].num_unwind = 0;
  sites[cur].unwind_offset = UINT32_MAX;
  sites[cur].num_live = 0;
  sites[cur].live_offset = loc_num;
  sites[cur].num_arch_const = 0;
  sites[cur].arch_const_offset = const_num;
  cur++;

  if(thread.st_size) // May not exist if application doesn't use pthreads
  {
    sites[cur].id = UINT64_MAX - 1;
    sites[cur].addr = thread.st_value + thread_start_offset(b->arch);
    sites[cur].fbp_offset = 0;
    sites[cur].num_unwind = 0;
    sites[cur].unwind_offset = UINT32_MAX;
    sites[cur].num_live = 0;
    sites[cur].live_offset = loc_num;
    sites[cur].num_arch_const = 0;
    sites[cur].arch_const_offset = const_num;
  }

  *num_sites = sites_num;
  *cs = sites;
  *num_live = loc_num;
  *live = locs;
  *num_consts = const_num;
  *consts = archconsts;

  return true;
}

static int sort_unwind_addr(const void *a, const void *b)
{
  const unwind_addr *ua_a = (const unwind_addr*)a;
  const unwind_addr *ua_b = (const unwind_addr*)b;

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

