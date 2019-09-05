/**
 * Utility functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/8/2016
 */

#ifndef _UTIL_H
#define _UTIL_H

#include "definitions.h"
#include "bin.h"
#include "call_site.h"

/**
 * Print warning message.
 * @param msg warning message
 */
static inline void warn(const char *msg)
{
  fprintf(stderr, "WARNING: %s\n", msg);
}

/**
 * Print error message & exit.
 * @param msg error message
 * @param retcode error code
 */
static inline void die(const char *msg, ret_t retcode)
{
  fprintf(stderr, "ERROR: %s - %s!\n", msg, ret_t_str[retcode]);
  exit(1);
}

/**
 * Check if the ELF object is supported.
 * @param e an ELF object
 * @return true if supported, false otherwise
 */
bool check_elf_ehdr(Elf *e);

/**
 * Return the kind of the ELF object.
 * @param e an ELF object
 * @return the kind of the object
 */
static inline const char *elf_kind_name(Elf *e)
{
  switch(elf_kind(e)) {
  case ELF_K_NONE: return "none";
  case ELF_K_AR: return "archive";
  case ELF_K_COFF: return "COFF object";
  case ELF_K_ELF: return "ELF object";
  default: return "n/a";
  };
}

/**
 * Return the class of the ELF object.
 * @param e an ELF object
 * @return the class of the object
 */
static inline const char *elf_class_name(Elf *e)
{
  switch(gelf_getclass(e)) {
  case ELFCLASSNONE: return "none";
  case ELFCLASS32: return "32-bit";
  case ELFCLASS64: return "64-bit";
  default: return "n/a";
  }
}

/**
 * Return the data storage format of the ELF object.
 * @param e an ELF object
 * @return the data storage format of the object
 */
static inline const char *elf_data_name(Elf *e)
{
  const char *id = elf_getident(e, NULL);
  if(!id) return "n/a";

  switch(id[EI_DATA]) {
  case ELFDATANONE: return "none";
  case ELFDATA2LSB: return "2's complement little-endian";
  case ELFDATA2MSB: return "2's complement big-endian";
  default: return "n/a";
  }
}

/**
 * Return the ABI of the ELF object.
 * @param e an ELF object
 * @return the ABI of the object
 */
static inline const char *elf_abi_name(Elf *e)
{
  const char *id = elf_getident(e, NULL);
  if(!id) return "n/a";

  switch(id[EI_OSABI]) {
  case ELFOSABI_SYSV: return "SysV"; // same as ELFOSABI_NONE
  case ELFOSABI_HPUX: return "HP UX";
  case ELFOSABI_NETBSD: return "NetBSD";
  case ELFOSABI_LINUX: return "GNU/Linux"; // same as ELFOSABI_GNU
  case ELFOSABI_SOLARIS: return "Solaris";
  case ELFOSABI_AIX: return "AIX";
  case ELFOSABI_IRIX: return "IRIX";
  case ELFOSABI_FREEBSD: return "FreeBSD";
  case ELFOSABI_TRU64: return "Tru64";
  case ELFOSABI_MODESTO: return "Modesto";
  case ELFOSABI_OPENBSD: return "OpenBSD";
  case ELFOSABI_ARM: return "ARM";
  //case ELFOSABI_STANDALONE: return "standalone"; TODO compiler complains
  default: return "n/a";
  }
}

/**
 * Return the architecture of the ELF object.
 * @param e an ELF object
 * @return the architecture of the object
 */
static inline const char *elf_arch_name(Elf *e)
{
  uint16_t arch = elf64_getehdr(e)->e_machine;

  switch(arch) {
  case EM_AARCH64: return "aarch64";
  case EM_PPC64: return "powerpc64";
  case EM_X86_64: return "x86_64";
  case EM_RISCV: return "riscv64";
  default: return "unsupported";
  }
}

/**
 * Get an ELF section by name.
 * @param e an ELF object
 * @param name name of the section
 * @return the associated ELF section, or NULL otherwise
 */
Elf_Scn *get_section_by_name(Elf *e, const char *name);

/**
 * Get an ELF section by offset.
 * @param e an ELF object
 * @param offset offset of section within ELF object
 * @return the associated ELF section, or NULL otherwise
 */
Elf_Scn *get_section_by_offset(Elf *e, size_t offset);

/**
 * Get the number of data blocks (i.e. the number of available Elf_Data
 * descriptors) in the ELF section.
 * @param s an ELF section object
 * @return the number of data blocks in the section
 */
size_t get_num_data_blocks(Elf_Scn *s);

/**
 * Get the ELF's symbol table.
 * @param e an ELF object
 * @return the symbol table, or NULL otherwise
 */
Elf_Scn *get_sym_tab(Elf *e);

/**
 * Get a symbol from the symbol table by name.
 * @param e an ELF object
 * @param name name of the symbol
 * @return the symbol if found, or an empty symbol otherwise
 */
GElf_Sym get_sym_by_name(Elf *e, const char *name);

/**
 * Get a symbol from the symbol table by address.
 * @param e an ELF object
 * @param addr the address of the symbol
 * @param type the type of the symbol (or UINT8_MAX if any type)
 * @return the symbol if found, or an empty symbol otherwise
 */
GElf_Sym get_sym_by_addr(Elf *e, uint64_t addr, uint8_t type);

/**
 * Get a symbol's name.
 * @param e an ELF object
 * @param sym a symbol
 * @return the name of the symbol, or a NULL pointer if it doesn't exist
 */
const char *get_sym_name(Elf *e, GElf_Sym sym);

/**
 * Get function unwinding metadata for an instruction address.
 *
 * @param addr an instruction address
 * @param num number of function unwinding metadata records
 * @param addrs function unwinding metadata records
 * @return a pointer to the corresponding function's metadata, or NULL if not
 *         found
 */
const unwind_addr *get_func_unwind_data(uint64_t addr,
                                        size_t num,
                                        const unwind_addr *addrs);

/**
 * Add new section name to section header string table.
 * @param e an ELF object
 * @param name section name
 * @return offset of new section name in section header string table
 */
uint64_t add_section_name(Elf *e, const char *name);

/**
 * Add a new section to the ELF.
 * @param e an ELF object
 * @param name the section name
 * @param num_entries number of entries in the section
 * @param entry_size size of each entry, in bytes
 * @param buf data comprising the section
 * @return 0 if it was added, an error code otherwise
 */
ret_t add_section(Elf *e,
                  const char *name,
                  size_t num_entries,
                  size_t entry_size,
                  void *buf);

/**
 * Update a section in the ELF.
 * @param e an ELF object
 * @param scn an ELF section
 * @param num_entries number of entries in the section
 * @param entry_size size of each entry, in bytes
 * @param buf data comprising the section
 * @return 0 if it was updated, an error code otherwise
 */
ret_t update_section(Elf *e,
                     Elf_Scn *scn,
                     size_t num_entries,
                     size_t entry_size,
                     void *buf);

/**
 * Get an ELF section's data.
 * @param e an ELF object
 * @param name the section name
 * @return a pointer to the section, or NULL if there was an error
 */
void *get_section_data(Elf_Scn *scn);

#endif /* _UTIL_H */

