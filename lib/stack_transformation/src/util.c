/*
 * Implementation of utility functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/11/2015
 */

#include <libelf/gelf.h>

#include "stack_transform.h"
#include "definitions.h"
#include "arch_regs.h"
#include "util.h"

#include <my_private.h>
#include <my_strptr.h>
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

GElf_Shdr*
my_gelf_getshdr(Elf_Scn *scn, GElf_Shdr *dst) {
    GElf_Shdr buf;

    if (!scn) {
	return NULL;
    }
    if (!dst) {
	dst = &buf;
    }
    if (scn->s_elf->e_class == ELFCLASS64)
    	*dst = scn->s_shdr64;
    else 
	return NULL;

    if (dst == &buf) {
	dst = (GElf_Shdr*)malloc(sizeof(GElf_Shdr));
	if (!dst) {
	    //seterr(ERROR_MEM_SHDR);
	    return NULL;
	}
	*dst = buf;
    }
    return dst;
}

Elf_Scn* my_find_index(Elf*e, Elf_Scn* scn, int index){
 
  Elf64_Ehdr e_hdr;
  Elf64_Shdr s_hdr;

  if(lseek(e->e_fd, 0, SEEK_SET) < 0){
                 printf("lseek failed\n");
                 close(e->e_fd);
                 return NULL;
         }

        if(read(e->e_fd, (void*)&e_hdr, sizeof(Elf64_Ehdr)) < 0){
                printf("Read Failed\n");
                close(e->e_fd);
                return NULL;
        }

        if(e_hdr.e_shoff == 0)
                return NULL;

        if(lseek(e->e_fd, e_hdr.e_shoff, SEEK_SET) < 0){
                printf("lseek failed\n");
                close(e->e_fd);
                return NULL;
        }

        for(int i=0; i<e_hdr.e_shnum; i++)
        {
                if(read(e->e_fd, (void*)&s_hdr, sizeof(Elf64_Ehdr)) < 0){
                        printf("Read Failed\n");
                        close(e->e_fd);
                        return NULL;
                }

                if(s_hdr.sh_name == index)
                {
                        if(lseek(e->e_fd, s_hdr.sh_offset, SEEK_SET) < 0){
                                printf("lseek failed\n");
                                close(e->e_fd);
                                return NULL;
                        }

                        if(s_hdr.sh_type != SHT_NOBITS){
                                scn = (Elf_Scn*)malloc(sizeof(Elf_Scn));
                                if(read(e->e_fd, (void*)scn, sizeof(s_hdr.sh_size)) < 0){
                                        printf("Read Failed\n");
                                        close(e->e_fd);
                                        return NULL;
                                }
                        }
                        scn->s_index = index;
                        return scn;
                }

                if(lseek(e->e_fd, i*e_hdr.e_shentsize+e_hdr.e_shoff, SEEK_SET) < 0){
                        printf("lseek failed\n");
                        close(e->e_fd);
                        return NULL;
                }
        }
        return NULL;
}

Elf_Scn* my_next_scn(Elf*e, Elf_Scn* scn)
{
  int i=1;

  if(!scn){
	if(!(scn = my_find_index(e, scn, 1)))
		return NULL;
  }
  else
 {
//	for(i=1; i<=29; i++)
	if(!(scn = my_find_index(e, scn, i+scn->s_index)))
		return NULL;
  }
  
  printf("Index found = %d\n", scn->s_index);
  return scn;
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
  Elf64_Ehdr e_hdr;
  int count = 0;

  ASSERT(sec, "invalid arguments to get_section()\n");
  
 // if(elf_getshdrstrndx(e, &shdrstrndx)) return NULL;
  if(lseek(e->e_fd, 0, SEEK_SET) < 0){
        printf("lseek failed\n");
        close(e->e_fd);
    	return NULL;
  }  

  if(read(e->e_fd, (void*)&e_hdr, sizeof(Elf64_Ehdr)) < 0){
        printf("Read Failed\n");
        close(e->e_fd);
	return NULL;
  }
  shdrstrndx = e_hdr.e_shstrndx;
/*printf("Number of sec headers %d\n", e_hdr.e_shnum);
  if(!(scn=my_next_scn(e, scn)))
	printf("Index not found");
  if(!(scn=my_next_scn(e, scn)))
	printf("Index not found");
 scn = NULL; */
  while((scn = elf_nextscn(e, scn)))
  {
    if(my_gelf_getshdr(scn, &shdr) != &shdr) return NULL;
    if((cur_sec = elf_strptr(e, shdrstrndx, shdr.sh_name)))
      if(!strcmp(sec, cur_sec)) break;
  }

  return scn; // NULL if not found

/*
for (scn = e->e_scn_1; scn; scn = scn->s_link) {//loop until index 1 found
  	if (scn->s_index == 1) {
		do{
   			 if(my_gelf_getshdr(scn, &shdr) != &shdr) return NULL;//memory leack
   			 if((cur_sec = my_elf_strptr(e, shdrstrndx, shdr.sh_name)))
     			 if(!strcmp(sec, cur_sec)) break;
			 scn = scn->s_link;
 		 }while(scn->s_elf == e);//work with next item until it's from same Elf
  		printf("First index found\n");
		while(1);
		return scn; // NULL if not found
	}
   }
*/
return NULL;

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
  (handle->unwind_addrs[idx].addr <= _addr && \
   _addr < handle->unwind_addrs[idx + 1].addr)

/*
 * Search through unwinding information addresses for the specified address.
 */
bool get_unwind_offset_by_addr(st_handle handle, void* addr, unwind_addr* meta)
{
  bool found = false;
  long min = 0;
  long max = (handle->unwind_addr_count - 1);
  long mid;
  uint64_t addr_int = (uint64_t)addr;

  TIMER_FG_START(get_unwind_offset_by_addr);
  ASSERT(meta, "invalid arguments to get_unwind_offset_by_addr()\n");

  while(max >= min)
  {
    mid = (max + min) / 2;

    // Corner case: mid == last record, this is always a stopping condition
    if(mid == handle->unwind_addr_count - 1)
    {
      if(handle->unwind_addrs[mid].addr <= addr_int)
      {
        ST_WARN("cannot check range of last record (0x%lx = record %ld?)\n",
                addr_int, mid);
        *meta = handle->unwind_addrs[mid];
        found = true;
      }
      break;
    }

    if(IN_RANGE(mid, addr_int))
    {
      *meta = handle->unwind_addrs[mid];
      found = true;
      break;
    }
    else if(addr_int > handle->unwind_addrs[mid].addr)
      min = mid + 1;
    else
      max = mid - 1;
  }

  if(found)
    ST_INFO("Address of enclosing function: 0x%lx\n", meta->addr);

  TIMER_FG_STOP(get_unwind_offset_by_addr);
  return found;
}

/*
 * Return the address of the function encapsulating PC.
 */
void* get_function_address(st_handle handle, void* pc)
{
  unwind_addr entry;

  if(!get_unwind_offset_by_addr(handle, pc, &entry)) return NULL;
  else return (void*)entry.addr;
}

