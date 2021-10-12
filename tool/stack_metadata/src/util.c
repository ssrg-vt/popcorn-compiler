/**
 * Utilities for handling ELF files.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/8/2016
 */

#include "definitions.h"
#include "util.h"

bool check_elf_ehdr(Elf *e)
{
  const char *id;
  Elf64_Ehdr *ehdr;

  /* Identity bytes w/ APIs */
  if(elf_kind(e) != ELF_K_ELF) return false; // is it an ELF object?
  if(gelf_getclass(e) != ELFCLASS64) return false; // is it 64-bit?

  /* Identity bytes which must be checked manually */
  if(!(id = elf_getident(e, NULL))) return false;
  if(id[EI_DATA] != ELFDATA2LSB) return false; // is it 2's complement, little-endian?
  // TODO clang produces SysV binaries for AArch64 but GNU for x86-64
  //if(id[EI_OSABI] != ELFOSABI_SYSV) return false; // is it UNIX - System V ABI?

  /* Architecture checks */
  if(!(ehdr = elf64_getehdr(e))) return false;
  if(ehdr->e_machine != EM_AARCH64 &&
     ehdr->e_machine != EM_PPC64 &&
     ehdr->e_machine != EM_RISCV &&
     ehdr->e_machine != EM_X86_64)
    return false;

  return true;
}

Elf_Scn *get_section_by_name(Elf *e, const char *name)
{
  size_t shdrstrndx = 0;
  const char *sec_name;
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr;

  if(elf_getshdrstrndx(e, &shdrstrndx)) return NULL;
  while((scn = elf_nextscn(e, scn)))
  {
    if(gelf_getshdr(scn, &shdr) != &shdr) return NULL;
    if((sec_name = elf_strptr(e, shdrstrndx, shdr.sh_name)))
      if(!strcmp(name, sec_name)) break;
  }
  return scn; // NULL if not found
}

Elf_Scn *get_section_by_offset(Elf *e, size_t offset)
{
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr;

  while((scn = elf_nextscn(e, scn)))
  {
    if(gelf_getshdr(scn, &shdr) != &shdr) return NULL;
    if(shdr.sh_offset == offset) break;
  }
  return scn; // NULL if not found
}

size_t get_num_data_blocks(Elf_Scn *s)
{
  size_t num_blocks = 0;
  Elf_Data *data = NULL;
  while((data = elf_getdata(s, data))) num_blocks++;
  return num_blocks;
}

Elf_Scn *get_sym_tab(Elf *e)
{
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr;

  while((scn = elf_nextscn(e, scn)))
  {
    if(gelf_getshdr(scn, &shdr) != &shdr) return false;
    if(shdr.sh_type == SHT_SYMTAB) break; // According to ELF standard only
  }                                        // one section can be SHT_SYMTAB
  return scn; // NULL if not found
}

GElf_Sym get_sym_by_name(Elf *e, const char *name)
{
  size_t num_syms, i;
  const char *sym_name;
  GElf_Sym sym, ret = {
    .st_name = 0,
    .st_info = 0,
    .st_other = 0,
    .st_shndx = 0,
    .st_value = 0,
    .st_size = 0
  };
  GElf_Shdr shdr;
  Elf_Scn *symtab = NULL;
  Elf_Data *data = NULL;

  if(!(symtab = get_sym_tab(e))) return ret;
  if(gelf_getshdr(symtab, &shdr) != &shdr) return ret;
  if(!(data = elf_getdata(symtab, data))) return ret;

  num_syms = shdr.sh_size / shdr.sh_entsize;
  for(i = 0; i < num_syms; i++)
  {
    if(gelf_getsym(data, i, &sym) != &sym) return ret;
    if(!(sym_name = elf_strptr(e, shdr.sh_link, sym.st_name))) continue;
    if(!strncmp(name, sym_name, BUF_SIZE))
    {
      ret = sym;
      break;
    }
  }

  return ret;
}

GElf_Sym get_sym_by_addr(Elf *e, uint64_t addr, uint8_t type)
{
  size_t num_syms, i;
  GElf_Sym sym, ret = {
    .st_name = 0,
    .st_info = 0,
    .st_other = 0,
    .st_shndx = 0,
    .st_value = 0,
    .st_size = 0
  };
  GElf_Shdr shdr;
  Elf_Scn *symtab = NULL;
  Elf_Data *data = NULL;

  if(!(symtab = get_sym_tab(e))) return ret;
  if(gelf_getshdr(symtab, &shdr) != &shdr) return ret;
  if(!(data = elf_getdata(symtab, data))) return ret;

  num_syms = shdr.sh_size / shdr.sh_entsize;
  for(i = 0; i < num_syms; i++)
  {
    if(gelf_getsym(data, i, &sym) != &sym) return ret;
    if(sym.st_value == addr)
    {
      if(type == UINT8_MAX || GELF_ST_TYPE(sym.st_info) == type)
      {
        ret = sym;
        break;
      }
    }
  }

  return ret;
}

const char *get_sym_name(Elf *e, GElf_Sym sym)
{
  const char *sym_name;
  GElf_Shdr shdr;
  Elf_Scn *symtab = NULL;

  if(!(symtab = get_sym_tab(e))) return NULL;
  if(gelf_getshdr(symtab, &shdr) != &shdr) return NULL;
  sym_name = elf_strptr(e, shdr.sh_link, sym.st_name);

  return sym_name;
}

uint64_t add_section_name(Elf *e, const char *name)
{
  size_t shdrstrndx, shstrtab_size, name_size;
  char *shstrtab;
  static char* old_shstrtab = NULL;
  Elf_Scn *scn;
  Elf64_Shdr *shdr;
  Elf_Data *data = NULL;

  /* Get section header string table data */
  if(elf_getshdrstrndx(e, &shdrstrndx) != 0) return 0;
  if(!(scn = elf_getscn(e, shdrstrndx))) return 0;
  if(!(shdr = elf64_getshdr(scn))) return 0;
  if(!(data = elf_getdata(scn, data))) return 0;

  /* Initialize new section header string table & and new name */
  name_size = strnlen(name, BUF_SIZE);
  shstrtab_size = data->d_size + name_size + 1;
  shstrtab = malloc(sizeof(char) * shstrtab_size);
  memcpy(shstrtab, data->d_buf, data->d_size);
  memcpy(&shstrtab[data->d_size], name, name_size);
  shstrtab[shstrtab_size - 1] = '\0';

  /* Free old table if we previously malloc'd it & save new table */
  if(data->d_buf == old_shstrtab)
    free(old_shstrtab);
  old_shstrtab = shstrtab;

  /* Save new section header string table */
  data->d_buf = shstrtab;
  data->d_size = shstrtab_size;
  shdr->sh_size = shstrtab_size;

  elf_flagdata(data, ELF_C_SET, ELF_F_DIRTY);
  elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);
  return shstrtab_size - name_size - 1;
}

#define IN_RANGE( idx, _addr ) \
  (addrs[idx].addr <= _addr && _addr < addrs[idx + 1].addr)

const unwind_addr *get_func_unwind_data(uint64_t addr,
                                        size_t num,
                                        const unwind_addr *addrs)
{
  long min = 0;
  long max = num - 1;
  long mid;

  while(max >= min)
  {
    mid = (max + min) / 2;

    // Corner case: mid == last record, this is always a stopping condition
    if(mid == num - 1)
    {
      if(addrs[mid].addr <= addr)
        return &addrs[mid];
      else break;
    }

    if(IN_RANGE(mid, addr)) return &addrs[mid];
    else if(addr > addrs[mid].addr) min = mid + 1;
    else max = mid - 1;
  }

  return NULL;
}

ret_t add_section(Elf *e,
                  const char *name,
                  size_t num_entries,
                  size_t entry_size,
                  void *buf)
{
  size_t name_off;
  Elf_Scn *scn;
  Elf64_Shdr *shdr;
  Elf_Data *data;

  if(!e || !name || !buf) return INVALID_ARGUMENT;
  if(verbose)
    printf("Adding section '%s': %lu entries, %lu bytes\n",
           name, num_entries, num_entries * entry_size);

  if((name_off = add_section_name(e, name)) == 0) return WRITE_ELF_FAILED;

  if(!(scn = elf_newscn(e))) return ADD_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return ADD_SECTION_FAILED;
  if(!(data = elf_newdata(scn))) return ADD_SECTION_FAILED;

  data->d_buf = buf;
  data->d_type = ELF_T_WORD;
  data->d_version = EV_CURRENT;
  data->d_size = entry_size * num_entries;
  data->d_off = 0LL; // Must be set by somebody else
  data->d_align = 8;

  shdr->sh_name = name_off;
  shdr->sh_type = SHT_PROGBITS;
  shdr->sh_flags = 0;
  shdr->sh_size = data->d_size;
  shdr->sh_entsize = entry_size;

  return SUCCESS;
}

ret_t update_section(Elf *e,
                     Elf_Scn *scn,
                     size_t num_entries,
                     size_t entry_size,
                     void *buf)
{
  size_t shdrstrndx;
  const char *name;
  Elf64_Shdr *shdr;
  Elf_Data *data = NULL;

  if(!e || !scn || !buf) return INVALID_ARGUMENT;

  if(!(data = elf_getdata(scn, data))) return UPDATE_SECTION_FAILED;
  if(!(shdr = elf64_getshdr(scn))) return UPDATE_SECTION_FAILED;

  if(verbose)
  {
    elf_getshdrstrndx(e, &shdrstrndx);
    name = elf_strptr(e, shdrstrndx, shdr->sh_name);
    printf("Updating section '%s': %lu entries, %lu bytes\n",
           name, num_entries, num_entries * entry_size);
  }

  data->d_buf = buf;
  data->d_size = entry_size * num_entries;
  shdr->sh_size = data->d_size;

  elf_flagdata(data, ELF_C_SET, ELF_F_DIRTY);
  elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);
  return SUCCESS;
}

void *get_section_data(Elf_Scn *scn)
{
  Elf_Data *data = NULL;

  // TODO gracefully handle multiple data sections?
  if(!scn) return NULL;
  if(get_num_data_blocks(scn) > 1) return NULL;
  if(!(data = elf_getdata(scn, data))) return NULL;

  return data->d_buf;
}

