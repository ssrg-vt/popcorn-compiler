/*
 * Implementation of utility functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/11/2015
 */

#include <stdbool.h>
#include <libdwarf.h>

#include "stack_transform.h"
#include "definitions.h"
#include "arch_regs.h"
#include "util.h"

///////////////////////////////////////////////////////////////////////////////
// (Public) Utility functions
///////////////////////////////////////////////////////////////////////////////

/*
 * Free strings returned by information functions.
 */
void st_free_str(st_handle handle, char* str)
{
  if(!handle) ST_WARN("invalid arguments\n");
  else dwarf_dealloc(handle->dbg, str, DW_DLA_STRING);
}

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
  case EM_X86_64: return ROSTR("x86-64");
  case EM_AARCH64: return ROSTR("aarch64");
  default: return ROSTR("unknown/unsupported architecture");
  }
}

/*
 * Get architecture-specific register operations.
 */
regset_t get_regops(uint16_t arch)
{
  switch(arch)
  {
  case EM_AARCH64: return &regs_aarch64;
  case EM_X86_64: return &regs_x86_64;
  default:
    ST_WARN("unsupported architecture\n");
    return NULL;
  }
}

/*
 * Get architecture-specific register operations.
 */
properties_t get_properties(uint16_t arch)
{
  switch(arch)
  {
  case EM_AARCH64: return &properties_aarch64;
  case EM_X86_64: return &properties_x86_64;
  default:
    ST_WARN("unsupported architecture\n");
    return NULL;
  }
}


/*
 * Print the DIE entry's name, if available.
 */
void print_die_name(st_handle handle, Dwarf_Die die)
{
  char* name;
  Dwarf_Bool has_attr;
  Dwarf_Error err;

  DWARF_OK(dwarf_hasattr(die, DW_AT_name, &has_attr, &err), "dwarf_hasattr");
  if(has_attr)
  {
    DWARF_OK(dwarf_diename(die, &name, &err), "dwarf_diename");
    ST_INFO("Name: %s\n", name);
    dwarf_dealloc(handle->dbg, name, DW_DLA_STRING);
  }
  else
    ST_INFO("Name: n/a\n");
}

/*
 * Print the DIE's type (a.k.a. its tag).
 */
void print_die_type(Dwarf_Die die)
{
  const char* tag_name;
  Dwarf_Half tag;
  Dwarf_Error err;

  DWARF_OK(dwarf_tag(die, &tag, &err), "dwarf_tag");
  dwarf_get_TAG_name(tag, &tag_name);
  ST_INFO("Type: %s\n", tag_name);
}

/*
 * Print all attributes for the DIE.
 */
void print_die_attrs(st_handle handle, Dwarf_Die die)
{
  int ret;
  const char* attr_name;
  Dwarf_Attribute* attrs;
  Dwarf_Signed i, num_attrs;
  Dwarf_Half attr;
  Dwarf_Error err;

  ret = DWARF_CHK(dwarf_attrlist(die, &attrs, &num_attrs, &err), "dwarf_attrlist");
  if(ret != DW_DLV_NO_ENTRY)
  {
    DWARF_OK(dwarf_whatattr(attrs[0], &attr, &err), "dwarf_whatattr");
    dwarf_get_AT_name(attr, &attr_name);
    ST_INFO("Attributes: %s", attr_name);
    dwarf_dealloc(handle->dbg, attrs[0], DW_DLA_ATTR);
    for(i = 1; i < num_attrs; i++)
    {
      DWARF_OK(dwarf_whatattr(attrs[i], &attr, &err), "dwarf_whatattr");
      dwarf_get_AT_name(attr, &attr_name);
      ST_RAW_INFO(", %s", attr_name);
      dwarf_dealloc(handle->dbg, attrs[i], DW_DLA_ATTR);
    }
    ST_RAW_INFO("\n");
    dwarf_dealloc(handle->dbg, attrs, DW_DLA_LIST);
  }
  else
    ST_INFO("Attributes: n/a\n");
}

/*
 * Decode an unsigned little-endian base-128 (LEB128) value.  Adapted from LLVM
 * (LEB128.h).
 */
Dwarf_Unsigned decode_leb128u(Dwarf_Unsigned raw)
{
  Dwarf_Unsigned value = 0, shift = 0;
  uint8_t* p = (uint8_t*)&raw;

  do {
    value += (Dwarf_Unsigned)((*p & 0x7f) << shift);
    shift += 7;
  }
  while(*p++ >= 128);

  ST_INFO("Decoded %llx -> %llu\n", raw, value);
  return value;
}

/*
 * Decode a signed little-endian base-128 (LEB128) value.  Adapted from LLVM
 * (LEB128.h).
 */
Dwarf_Signed decode_leb128s(Dwarf_Unsigned raw)
{
  Dwarf_Signed value = 0;
  Dwarf_Unsigned shift = 0;
  uint8_t byte;
  uint8_t* p = (uint8_t*)&raw;

  do {
    byte = *p++;
    value |= ((byte & 0x7f) << shift);
    shift += 7;
  }
  while(byte >= 128);

  if(byte & 0x40)
    value |= (-1ULL) << shift;

  ST_INFO("Decoded %llx -> %lld\n", raw, value);
  return value;
}

