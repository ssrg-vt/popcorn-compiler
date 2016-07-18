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
 * @return true if the data was created, false otherwise (*cs & *locs will be
 *         NULL if creation failed)
 */
static bool create_call_site_metadata(bin *b,
                                      stack_map *sm,
                                      size_t num_sm,
                                      uint64_t start_id,
                                      size_t *num_sites,
                                      call_site **cs,
                                      size_t *num_live,
                                      call_site_value **live);

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

ret_t add_sections(bin *b,
                   stack_map *sm,
                   size_t num_sm,
                   const char *sec,
                   uint64_t start_id)
{
  size_t num_shdr, i, cur_offset, num_sites, num_live;
  char sec_name[BUF_SIZE];
  call_site *id_sites, *addr_sites;
  call_site_value *live_vals;
  Elf64_Ehdr *ehdr;
  Elf64_Shdr *shdr;
  Elf_Scn *scn;
  ret_t ret;

  /* Generate the section's data (not yet sorted by anything) */
  if(!create_call_site_metadata(b,
                                sm,
                                num_sm,
                                start_id,
                                &num_sites,
                                &id_sites,
                                &num_live,
                                &live_vals))
    return CREATE_METADATA_FAILED;

  /* Add call site section sorted by ID */
  qsort(id_sites, num_sites, sizeof(call_site), sort_id);
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_ID);
  if((scn = get_section_by_name(b->e, sec_name)))
    ret = update_section(b->e, scn, num_sites, sizeof(call_site), id_sites);
  else
    ret = add_section(b->e, sec_name, num_sites, sizeof(call_site), id_sites);
  if(ret) return ret;

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

  /* Add live-value location section. */
  snprintf(sec_name, BUF_SIZE, "%s.%s", sec, SECTION_LIVE);
  if((scn = get_section_by_name(b->e, sec_name)))
    ret = update_section(b->e, scn, num_live, sizeof(call_site_value), live_vals);
  else
    ret = add_section(b->e, sec_name, num_live, sizeof(call_site_value), live_vals);
  if(ret) return ret;

  /* Calculate offset of last non-stack-transform section */
  if(elf_getshdrnum(b->e, &num_shdr) == -1) return READ_ELF_FAILED;
  if(!(scn = elf_getscn(b->e, num_shdr - 4))) return READ_ELF_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return READ_ELF_FAILED;
  cur_offset = shdr->sh_offset + shdr->sh_size;

  /* Update stack transformation section offsets */
  for(i = num_shdr - 3; i < num_shdr; i++)
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
  if(verbose) printf("Section table moved to %lx\n", cur_offset);

  /* Write changes back to disk.  LibELF now controls the data we malloc'd. */
  if(elf_update(b->e, ELF_C_WRITE) < 0) return WRITE_ELF_FAILED;
  return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// Private API
///////////////////////////////////////////////////////////////////////////////

static bool create_call_site_metadata(bin *b,
                                      stack_map *sm,
                                      size_t num_sm,
                                      uint64_t start_id,
                                      size_t *num_sites,
                                      call_site **cs,
                                      size_t *num_live,
                                      call_site_value **live)
{
  size_t i, j, sites_num = 0, loc_num = 0, cur;
  GElf_Sym mainthr, thread;
  call_site *sites;
  call_site_value *locs;

  if(!num_sites || !cs || !num_live || !live) return false;

  mainthr = get_sym_by_name(b->e, START_MAIN);
  if(!mainthr.st_size) return false;
  thread = get_sym_by_name(b->e, START_THREAD);

  /* Calculate number of stackmap & location records */
  for(i = 0; i < num_sm; i++)
  {
    sites_num += sm[i].num_records;
    for(j = 0; j < sm[i].num_records; j++)
      loc_num += sm[i].stack_maps[j].locations->num;
  }
  sites_num += (thread.st_size ? 2 : 1);
  sites = malloc(sizeof(call_site) * sites_num);
  locs = malloc(sizeof(call_site_value) * loc_num);

  if(verbose)
    printf("Creating metadata for %lu call site & %lu location records\n",
           sites_num, loc_num);

  /* Populate call site & location record data. */
  for(i = 0, loc_num = 0, cur = 0; i < num_sm; i++)
  {
    for(j = 0; j < sm[i].num_records; j++, cur++)
    {
      sites[cur].id = start_id++;
      if(sm[i].stack_maps[j].id == 0) // Function entry stack map
      {
        sites[cur].addr = sm[i].stack_sizes[sm[i].stack_maps[j].func_idx].func_addr;
        sites[cur].fbp_offset = 0; // TODO fp_offset(b->arch);
      }
      else // Call site
      {
        sites[cur].addr = sm[i].stack_sizes[sm[i].stack_maps[j].func_idx].func_addr +
                          sm[i].stack_maps[j].offset;
        sites[cur].fbp_offset =
          sm[i].stack_sizes[sm[i].stack_maps[j].func_idx].stack_size -
          fp_offset(b->arch);
      }
      sites[cur].num_live = sm[i].stack_maps[j].locations->num;
      sites[cur].live_offset = loc_num;
      memcpy(&locs[loc_num], &sm[i].stack_maps[j].locations->record,
             sizeof(call_site_value) * sites[cur].num_live);
      loc_num += sm[i].stack_maps[j].locations->num;
    }
  }

  /* Add entries for main thread's __libc_start_main & thread's start */
  sites[cur].id = start_id++;
  sites[cur].addr = mainthr.st_value + main_start_offset(b->arch);
  sites[cur].fbp_offset = 0;
  sites[cur].num_live = 0;
  sites[cur].live_offset = loc_num;
  cur++;

  if(thread.st_size) // May not exist if application doesn't use pthreads
  {
    sites[cur].id = start_id++;
    sites[cur].addr = thread.st_value + thread_start_offset(b->arch);
    sites[cur].fbp_offset = 0;
    sites[cur].num_live = 0;
    sites[cur].live_offset = loc_num;
  }

  *num_sites = sites_num;
  *cs = sites;
  *num_live = loc_num;
  *live = locs;

  return true;
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

