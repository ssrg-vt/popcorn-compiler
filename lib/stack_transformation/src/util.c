/*
 * Implementation of utility functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/11/2015
 */

#include "stack_transform.h"
#include "definitions.h"
#include "arch_regs.h"
#include "util.h"

#include <my_private.h>

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

/*Returns 1 if it can find the desired string*/
int match_string(Elf* e, char* name, int *str_index)
{
	Elf64_Ehdr e_hdr;
	Elf64_Shdr s_hdr;
 	char *stringTable;
	char entryStr[100];

 	if(lseek(e->e_fd, 0, SEEK_SET) < 0){
        	printf("lseek failed\n");
        	close(e->e_fd);
        	return -1;
  	}

  	if(read(e->e_fd, (void*)&e_hdr, sizeof(Elf64_Ehdr)) < 0){
        	printf("Read Failed\n");
        	close(e->e_fd);
        	return -1;
  	}

	if(lseek(e->e_fd, e_hdr.e_shoff + e_hdr.e_shentsize*e_hdr.e_shstrndx, SEEK_SET) < 0){
		printf("lseek failed\n");
		close(e->e_fd);
		return -1;
	}

	if(read(e->e_fd, (void*)&s_hdr, sizeof(Elf64_Shdr)) < 0){
		printf("Read Failed\n");
		close(e->e_fd);
		return -1;
	}
	//got string header
	if(e_hdr.e_shstrndx==SHN_UNDEF)
		return -1;

        if(lseek(e->e_fd, s_hdr.sh_offset, SEEK_SET) < 0){
                printf("lseek failed\n");
                close(e->e_fd);
                return -1;
        }

	stringTable = (char*)malloc(s_hdr.sh_size);
        if(!stringTable)
	{
		printf("Couldn't allocate memory\n");
		return -1;
	}

        if(read(e->e_fd, (void*)stringTable, sizeof(char)*s_hdr.sh_size) < 0){
                printf("Read Failed\n");
                close(e->e_fd);
                return -1;
        }

	for(int i=0, j=0; i<s_hdr.sh_size; i++)
	{
		if(stringTable[i] == '\0')
		{
			entryStr[j] = '\0';
			if(!strcmp(entryStr, name))
			{
				*str_index = i-j;
//				free(stringTable);
				return 1;
			}
			j = 0;
		}
		else
		{
			entryStr[j] = stringTable[i];
			j++; 
		}
	}		
//	free(stringTable);
	return -1;
}

Elf64_Shdr* grep_e_header(Elf *e, int index)
{
	Elf64_Ehdr e_hdr;
	Elf64_Shdr *s_hdr = (Elf64_Shdr*)malloc(sizeof(Elf64_Shdr));

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
 for(int i=0; i<e_hdr.e_shnum; i++)
{	
	if(lseek(e->e_fd, e_hdr.e_shoff + e_hdr.e_shentsize*i, SEEK_SET) < 0){
                 printf("lseek failed\n");
                 close(e->e_fd);
                 return NULL;
         }

        if(read(e->e_fd, (void*)s_hdr, sizeof(Elf64_Ehdr)) < 0){
                printf("Read Failed\n");
                close(e->e_fd);
                return NULL;
        }
	if(s_hdr->sh_name == index)	
	{
		if(s_hdr->sh_type == SHT_NULL)
		{
			printf("SHT_NULL\n");
			return NULL;
		}
		return s_hdr;
	}
}
	printf("Index Not found\n");
	return NULL; 
}

/*
 * Search for and return ELF section named SEC.
 */
/*
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
*/
/*
 * Search for and return ELF section named SEC.
 */
Elf64_Shdr* my_get_section(Elf* e, const char* sec)
{
  size_t shdrstrndx = 0;
  Elf64_Ehdr e_hdr;
  Elf64_Shdr *s_hdr;
  int str_index = 0;

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
 
  if(match_string(e, (char*)sec, &str_index) < 0)
	return NULL;
  
  s_hdr = grep_e_header(e, str_index);
  return s_hdr; // NULL if not found

}

/*
 * Get the number of entries in section SEC.
 */
/*
int64_t get_num_entries(Elf* e, const char* sec)
{
  Elf_Scn* scn;
  GElf_Shdr shdr;
  
  if(!(scn = get_section(e, sec))) return -1;
  if(gelf_getshdr(scn, &shdr) != &shdr) return -1;
  return shdr.sh_entsize ? (shdr.sh_size / shdr.sh_entsize) : -1;
}
*/

int64_t my_get_num_entries(Elf* e, const char* sec)
{
  Elf64_Shdr *shdr;
  int64_t i;
  
  if(!(shdr = my_get_section(e, sec)))
	return -1;

  i = shdr->sh_entsize ? (shdr->sh_size / shdr->sh_entsize) : -1;
  free(shdr);
  return i;
}

/*
 * Return the start of the section SEC in Elf data E.
 */
/*
const void* get_section_data(Elf* e, const char* sec)
{
  Elf_Scn* scn;
  Elf_Data* data = NULL;
  GElf_Shdr shdr;

  if(!(scn = get_section(e, sec))) return NULL;
  if(gelf_getshdr(scn, &shdr) != &shdr) return NULL;
  if(!(data = elf_getdata(scn, data))) return NULL;
  
  return data->d_buf;
}
*/
const void* my_get_section_data(Elf* e, const char* sec)
{   
  Elf64_Shdr *shdr;
  Elf_Data *data = (Elf_Data*)malloc(sizeof(Elf_Data));  

  shdr = my_get_section(e, sec);
  data->d_buf = malloc(shdr->sh_size);
  for(int i=0; i<shdr->sh_size; i++)
  	*((int*)(data->d_buf + i)) = 0;
  
  if(lseek(e->e_fd, shdr->sh_offset, SEEK_SET) < 0){
        printf("lseek failed\n");
        close(e->e_fd);
        return NULL;
  }

  if(read(e->e_fd, (void*)data->d_buf, shdr->sh_size) < 0){
        printf("Read Failed\n");
        close(e->e_fd);
        return NULL;
  }

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

