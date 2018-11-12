/*
 * Implementation of utility functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/11/2015
 */

#include <libelf/gelf.h>

#include "definitions.h"
#include "arch_regs.h"
#include "util.h"

///////////////////////////////////////////////////////////////////////////////
// (Private) Utility functions
///////////////////////////////////////////////////////////////////////////////

/*
 * Return the name of ARCH.
 */
const char* arch_name(uint16_t arch)
{
  switch(arch)
  {
  case EM_AARCH64: return "aarch64";
  case EM_PPC64: return "powerpc64";
  case EM_X86_64: return "x86-64";
  default: return "unknown/unsupported architecture";
  }
}

/*
 * Get architecture-specific register operations.
 */
regops_t get_regops(uint16_t arch)
{
  switch(arch)
  {
  case EM_AARCH64: return &regs_aarch64;
  case EM_PPC64: return &regs_powerpc64;
  case EM_X86_64: return &regs_x86_64;
  default:
    ST_WARN("unsupported architecture\n");
    return NULL;
  }
}

/*
 * Get architecture-specific properties.
 */
properties_t get_properties(uint16_t arch)
{
  switch(arch)
  {
  case EM_AARCH64: return &properties_aarch64;
  case EM_PPC64: return &properties_powerpc64;
  case EM_X86_64: return &properties_x86_64;
  default:
    ST_WARN("unsupported architecture\n");
    return NULL;
  }
}

/*
 * Search for and return ELF section named SEC.
 */
Elf_Scn* get_section(Elf* e, const char* sec)
{
  size_t shdrstrndx = 0;
  const char* cur_sec;
  Elf_Scn* scn = NULL;
  GElf_Shdr shdr;

  ASSERT(sec, "invalid arguments to get_section()\n");

  if(elf_getshdrstrndx(e, &shdrstrndx)) return NULL;
  while((scn = elf_nextscn(e, scn)))
  {
    if(gelf_getshdr(scn, &shdr) != &shdr) return NULL;
    if((cur_sec = elf_strptr(e, shdrstrndx, shdr.sh_name)))
      if(!strcmp(sec, cur_sec)) break;
  }
  return scn; // NULL if not found
}

/*
 * Get the number of entries in section SEC.
 */
int64_t get_num_entries(Elf* e, const char* sec)
{
  Elf_Scn* scn;
  GElf_Shdr shdr;

  if(!(scn = get_section(e, sec))) return -1;
  if(gelf_getshdr(scn, &shdr) != &shdr) return -1;
  return shdr.sh_entsize ? (shdr.sh_size / shdr.sh_entsize) : -1;
}

/*
 * Return the start of the section SEC in Elf data E.
 */
const void* get_section_data(Elf* e, const char* sec)
{
  Elf_Scn* scn;
  Elf_Data* data = NULL;

  if(!(scn = get_section(e, sec))) return NULL;
  if(!(data = elf_getdata(scn, data))) return NULL;
  return data->d_buf;
}

/*
 * Search through call site entries for the specified return address.
 */
bool get_site_by_addr(st_handle handle, void* ret_addr, call_site* cs)
{
  bool found = false;
  long min = 0;
  long max = (handle->sites_count - 1);
  long mid;
  uint64_t retaddr = (uint64_t)ret_addr;

  TIMER_FG_START(get_site_by_addr);
  ASSERT(cs, "invalid arguments to get_site_by_addr()\n");

  while(max >= min)
  {
    mid = (max + min) / 2;
    if(handle->sites_addr[mid].addr == retaddr) {
      *cs = handle->sites_addr[mid];
      found = true;
      break;
    }
    else if(retaddr > handle->sites_addr[mid].addr)
      min = mid + 1;
    else
      max = mid - 1;
  }

  TIMER_FG_STOP(get_site_by_addr);
  return found;
}

/*
 * Search through call site entries for the specified ID.
 */
bool get_site_by_id(st_handle handle, uint64_t csid, call_site* cs)
{
  bool found = false;
  long min = 0;
  long max = (handle->sites_count - 1);
  long mid;

  TIMER_FG_START(get_site_by_id);
  ASSERT(cs, "invalid arguments to get_site_by_id()\n");

  while(max >= min)
  {
    mid = (max + min) / 2;
    if(handle->sites_id[mid].id == csid) {
      *cs = handle->sites_id[mid];
      found = true;
      break;
    }
    else if(csid > handle->sites_id[mid].id)
      min = mid + 1;
    else
      max = mid - 1;
  }

  TIMER_FG_STOP(get_site_by_id);
  return found;
}

/* Check if an address is within the range of a function unwinding record */
#define IN_RANGE( idx, _addr ) \
  (handle->funcs[idx].addr <= _addr && \
   _addr < (handle->funcs[idx].addr + handle->funcs[idx].code_size))

/*
 * Search through function records for the specified address.
 */
const function_record *get_function_by_addr(st_handle handle, void* addr)
{
  bool found = false;
  long min = 0;
  long max = (handle->func_count - 1);
  long mid;
  uint64_t addr_int = (uint64_t)addr;
  const function_record *fr = NULL;

  TIMER_FG_START(get_function_by_addr);
  ASSERT(handle && addr, "invalid arguments to get_unwind_offset_by_addr()\n");

  while(max >= min)
  {
    mid = (max + min) / 2;

    if(IN_RANGE(mid, addr_int))
    {
      fr = &handle->funcs[mid];
      found = true;
      break;
    }
    else if(addr_int > handle->funcs[mid].addr)
      min = mid + 1;
    else
      max = mid - 1;
  }

  if(found)
    ST_INFO("Address of enclosing function: 0x%lx\n", fr->addr);

  TIMER_FG_STOP(get_function_by_addr);
  return fr;
}

/*
 * Return the address of the function encapsulating PC.
 */
void* get_function_address(st_handle handle, void* pc)
{
  const function_record *fr = get_function_by_addr(handle, pc);
  if(fr) return (void*)fr->addr;
  else return NULL;
}

