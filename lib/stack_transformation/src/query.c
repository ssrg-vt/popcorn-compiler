/*
 * Implementation of operations for querying individual DIEs & FDEs.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/10/2015
 */

#include "query.h"

#include <libelf/gelf.h>

///////////////////////////////////////////////////////////////////////////////
// Query operations
///////////////////////////////////////////////////////////////////////////////

/* Return the compilation unit DIE for a given program location. */
Dwarf_Die get_cu_die(st_handle handle, void* pc)
{
  int ret;
  Dwarf_Arange cu_arange;
  Dwarf_Off cu_die_offset;
  Dwarf_Die cu_die;
  Dwarf_Error err;

  // Note: dwarf_get_arange does a linear search, we can probably speed this
  // up by pre-sorting the aranges and then doing a binary search
  ret = DWARF_CHK(dwarf_get_arange(handle->aranges,
                                   handle->arange_count,
                                   (Dwarf_Addr)pc,
                                   &cu_arange,
                                   &err),
                  "dwarf_get_arange");
  if(ret == DW_DLV_NO_ENTRY)
  {
    ST_WARN("could not find address range for PC=%p\n", pc);
    return NULL;
  }

  DWARF_OK(dwarf_get_cu_die_offset(cu_arange, &cu_die_offset, &err),
           "dwarf_get_cu_die_offset");
  ret = DWARF_CHK(dwarf_offdie_b(handle->dbg, cu_die_offset, true, &cu_die, &err),
                  "dwarf_offdie");
  if(ret == DW_DLV_NO_ENTRY)
  {
    ST_WARN("could not find compilation unit for PC=%p\n", pc);
    return NULL;
  }

  return cu_die;
}

/* Return the function DIE for a given program location. */
bool get_func_die(st_handle handle, void* pc, Dwarf_Die* cu, Dwarf_Die* func)
{
  int ret;
  Dwarf_Die cu_die, sib_die, tmp_die;
  Dwarf_Half tag, form;
  Dwarf_Addr lowpc, highpc;
  Dwarf_Bool has_lowpc, has_highpc;
  enum Dwarf_Form_Class class;
  Dwarf_Error err;

  ASSERT(func, "invalid arguments to get_func_die()\n");

  /* Get the CU for the PC */
  cu_die = get_cu_die(handle, pc);
  ret = DWARF_CHK(dwarf_child(cu_die, &sib_die, &err), "dwarf_child");
  if(cu) *cu = cu_die;
  else dwarf_dealloc(handle->dbg, cu_die, DW_DLA_DIE);
  if(ret == DW_DLV_NO_ENTRY)
  {
    ST_WARN("no children in compilation unit\n");
    if(cu)
    {
      dwarf_dealloc(handle->dbg, cu_die, DW_DLA_DIE);
      *cu = NULL;
    }
    return false;
  }

  /* Search first child to see if it's the matching function */
  DWARF_OK(dwarf_tag(sib_die, &tag, &err), "dwarf_tag");
  if(tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine)
  {
    /* Ensure DIE has required attributes before checking PC range */
    DWARF_OK(dwarf_hasattr(sib_die, DW_AT_low_pc, &has_lowpc, &err), "dwarf_hasattr");
    DWARF_OK(dwarf_hasattr(sib_die, DW_AT_high_pc, &has_highpc, &err), "dwarf_hasattr");
    if(has_lowpc && has_highpc)
    {
      DWARF_OK(dwarf_lowpc(sib_die, &lowpc, &err), "dwarf_lowpc");
      DWARF_OK(dwarf_highpc_b(sib_die, &highpc, &form, &class, &err), "dwarf_highpc");
      if(class == DW_FORM_CLASS_CONSTANT) highpc += lowpc;

      if(lowpc <= (Dwarf_Addr)pc && (Dwarf_Addr)pc < highpc)
      {
        *func = sib_die;
        return true;
      }
    }
  }

  /* First child didn't match, search rest of children */
  while(DWARF_CHK(dwarf_siblingof_b(handle->dbg,
                                    sib_die,
                                    true,
                                    &tmp_die,
                                    &err),
                  "dwarf_siblingof_b") != DW_DLV_NO_ENTRY)
  {
    dwarf_dealloc(handle->dbg, sib_die, DW_DLA_DIE);
    sib_die = tmp_die;

    DWARF_OK(dwarf_tag(sib_die, &tag, &err), "dwarf_tag");
    if(tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine)
    {
      DWARF_OK(dwarf_hasattr(sib_die, DW_AT_low_pc, &has_lowpc, &err), "dwarf_hasattr");
      DWARF_OK(dwarf_hasattr(sib_die, DW_AT_high_pc, &has_highpc, &err), "dwarf_hasattr");
      if(has_lowpc && has_highpc)
      {
        DWARF_OK(dwarf_lowpc(sib_die, &lowpc, &err), "dwarf_lowpc");
        DWARF_OK(dwarf_highpc_b(sib_die, &highpc, &form, &class, &err), "dwarf_highpc");
        if(class == DW_FORM_CLASS_CONSTANT) highpc = lowpc + highpc;

        if(lowpc <= (Dwarf_Addr)pc && (Dwarf_Addr)pc < highpc)
        {
          *func = sib_die;
          return true;
        }
      }
    }
  }

  /* Didn't find function... */
  dwarf_dealloc(handle->dbg, sib_die, DW_DLA_DIE);
  ST_WARN("could not find function for PC=%p\n", pc);
  *func = NULL;
  if(cu)
  {
    dwarf_dealloc(handle->dbg, cu_die, DW_DLA_DIE);
    *cu = NULL;
  }
  return false;
}

/*
 * Return the function DIE for function FUNC in compilation unit CU.
 */
// Note: this is significantly slower than searching by PC!
bool get_named_func_die(st_handle handle,
                        const char* cu,
                        const char* func,
                        Dwarf_Die* cu_die,
                        Dwarf_Die* func_die)
{
  int ret;
  bool found = false;
  char* cu_name, *func_name;
  Dwarf_Unsigned cu_offset;
  Dwarf_Half cu_type, tag;
  Dwarf_Bool has_name;
  Dwarf_Die _cu_die, sib_die, tmp_die;
  Dwarf_Error err;

  ASSERT(cu && func && func_die, "invalid arguments to get_named_func_die()\n");

  // Note: we have to keep looping until DW_DLV_NO_ENTRY is returned because of
  // the way dwarf_next_cu_header delivers CU die offsets.
  while(DWARF_CHK(dwarf_next_cu_header_d(handle->dbg, true,
                                         NULL, NULL, NULL, NULL,
                                         NULL, NULL, NULL, NULL,
                                         &cu_offset, &cu_type, &err),
                  "dwarf_next_cu_header_d") != DW_DLV_NO_ENTRY)
  {
    if(found) continue;

    /* Get the CU die */
    DWARF_OK(dwarf_siblingof_b(handle->dbg, NULL, true, &_cu_die, &err), "dwarf_siblingof_b");
    ret = DWARF_CHK(dwarf_child(_cu_die, &sib_die, &err), "dwarf_child");
    if(cu_die) *cu_die = _cu_die;
    else dwarf_dealloc(handle->dbg, _cu_die, DW_DLA_DIE);
    if(ret == DW_DLV_NO_ENTRY)
    {
      if(cu_die) dwarf_dealloc(handle->dbg, _cu_die, DW_DLA_DIE);
      continue;
    }

    /* Check that we're examining the right compilation unit */
    DWARF_OK(dwarf_diename(_cu_die, &cu_name, &err), "dwarf_diename");
    if(strcmp(cu, cu_name))
    {
      if(cu_die) dwarf_dealloc(handle->dbg, _cu_die, DW_DLA_DIE);
      continue;
    }

    /* Search first child to see if it's the matching function */
    DWARF_OK(dwarf_tag(sib_die, &tag, &err), "dwarf_tag");
    if(tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine)
    {
      /* Ensure DIE has required attributes before checking PC range */
      DWARF_OK(dwarf_hasattr(sib_die, DW_AT_name, &has_name, &err), "dwarf_hasattr");
      if(has_name)
      {
        DWARF_OK(dwarf_diename(sib_die, &func_name, &err), "dwarf_diename");
        if(!strcmp(func, func_name))
        {
          found = true;
          dwarf_dealloc(handle->dbg, func_name, DW_DLA_STRING);
          continue;
        }
        dwarf_dealloc(handle->dbg, func_name, DW_DLA_STRING);
      }
    }

    /* First child didn't match, search rest of children */
    while(DWARF_CHK(dwarf_siblingof_b(handle->dbg,
                                      sib_die,
                                      true,
                                      &tmp_die,
                                      &err),
                    "dwarf_siblingof") != DW_DLV_NO_ENTRY)
    {
      dwarf_dealloc(handle->dbg, sib_die, DW_DLA_DIE);
      sib_die = tmp_die;

      DWARF_OK(dwarf_tag(sib_die, &tag, &err), "dwarf_tag");
      if(tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine)
      {
        DWARF_OK(dwarf_hasattr(sib_die, DW_AT_name, &has_name, &err), "dwarf_hasattr");
        if(has_name)
        {
          DWARF_OK(dwarf_diename(sib_die, &func_name, &err), "dwarf_diename");
          if(!strcmp(func, func_name))
          {
            found = true;
            dwarf_dealloc(handle->dbg, func_name, DW_DLA_STRING);
            break;
          }
          dwarf_dealloc(handle->dbg, func_name, DW_DLA_STRING);
        }
      }
    }
  }

  if(found)
  {
    *func_die = sib_die;
    return true;
  }
  else
  {
    ST_WARN("could not find function '%s'\n", func);
    dwarf_dealloc(handle->dbg, sib_die, DW_DLA_DIE);
    if(cu_die) dwarf_dealloc(handle->dbg, _cu_die, DW_DLA_DIE);
    *func_die = NULL;
    *cu_die = NULL;
    return false;
  }
}

/*
 * Return the frame description entry and common information entry for a given
 * program location.
 */
void get_fde_cie(st_handle handle, void* pc, Dwarf_Fde* fde, Dwarf_Cie* cie)
{
  int ret;
  Dwarf_Addr lowpc, highpc;
  Dwarf_Error err;
  Dwarf_Fde* first, *second;

  ASSERT(fde && cie, "invalid arguments to get_fde_cie()\n");

  /*
   * Some architectures dump frame information into .eh_frame while others
   * dump it into .debug_frame.  Pick the architecture-specific ordering.
   */
  if(handle->arch == EM_X86_64)
  {
    first = handle->fdes_eh;
    second = handle->fdes;
  }
  else
  {
    first = handle->fdes;
    second = handle->fdes_eh;
  }

  ret = DWARF_CHK(dwarf_get_fde_at_pc(first,
                                      (Dwarf_Addr)pc,
                                      fde,
                                      &lowpc,
                                      &highpc,
                                      &err),
                  "dwarf_get_fde_at_pc");
  if(ret == DW_DLV_NO_ENTRY)
  {
    ret = DWARF_CHK(dwarf_get_fde_at_pc(second,
                                        (Dwarf_Addr)pc,
                                        fde,
                                        &lowpc,
                                        &highpc,
                                        &err),
                    "dwarf_get_fde_at_pc");
    if(ret == DW_DLV_NO_ENTRY)
    {
      ST_WARN("could not find frame description entry for PC=%p\n", pc);
      *fde = NULL;
      *cie = NULL;
      return;
    }
  }

  DWARF_OK(dwarf_get_cie_of_fde(*fde, cie, &err), "dwarf_get_cie_of_fde");
}

/* Return the number of direct children of DIE of type TAG. */
size_t get_num_children(st_handle handle, Dwarf_Die die, Dwarf_Half tag)
{
  size_t num_dies = 0;
  int ret;
  Dwarf_Die sib_die, tmp_die;
  Dwarf_Half sib_tag;
  Dwarf_Error err;

  ret = DWARF_CHK(dwarf_child(die, &sib_die, &err), "dwarf_child");
  while(ret != DW_DLV_NO_ENTRY)
  {
    DWARF_OK(dwarf_tag(sib_die, &sib_tag, &err), "dwarf_tag");
    if(sib_tag == tag) num_dies++;

    ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, sib_die, true, &tmp_die, &err),
                    "dwarf_siblingof_b");
    dwarf_dealloc(handle->dbg, sib_die, DW_DLA_DIE);
    sib_die = tmp_die;
  }

  return num_dies;
}

/* Return the direct children of DIE of type TAG. */
size_t get_children(st_handle handle,
                    Dwarf_Die die,
                    Dwarf_Half tag,
                    Dwarf_Die** children)
{
  size_t num, cur_num = 0;
  const char* tag_name;
  int ret;
  Dwarf_Die sib_die, tmp_die;
  Dwarf_Die* ret_dies;
  Dwarf_Half sib_tag;
  Dwarf_Error err;

  ASSERT(children, "invalid arguments to get_children()\n");

  num = get_num_children(handle, die, tag);
  dwarf_get_TAG_name(tag, &tag_name);
  ST_INFO("Found %lu children of type %s\n", num, tag_name);
  if(!num)
  {
    *children = NULL;
    return num;
  }

  /* Search through children. */
  ret_dies = (Dwarf_Die*)malloc(sizeof(Dwarf_Die) * num);
  ret = DWARF_CHK(dwarf_child(die, &sib_die, &err), "dwarf_child");
  while(ret != DW_DLV_NO_ENTRY)
  {
    DWARF_OK(dwarf_tag(sib_die, &sib_tag, &err), "dwarf_tag");
    ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, sib_die, true, &tmp_die, &err),
                    "dwarf_siblingof_b");
    if(sib_tag == tag)
      ret_dies[cur_num++] = sib_die;
    else
      dwarf_dealloc(handle->dbg, sib_die, DW_DLA_DIE);
    sib_die = tmp_die;
  }

  ASSERT(cur_num == num, "could not get child DIEs\n");

  *children = ret_dies;
  return num;
}

#if _LIVE_VALS == DWARF_LIVE_VALS

/*
 * An optimized way to search for a function's argument & local variable
 * metadata.  This depends on the structural ordering of how this information
 * is layed out in the binary (valid for clang v3.7.1):
 *
 * 1. All arguments, if any, are in a single contiguous block
 * 2. All variables, if any, are in a single contiguous block
 * 3. If there are arguments, then they appear *directly before* variables (if
 *    there are variables)
 * 4. Conversely, if there are variables then they appear *directly after*
 *    arguments (if there are arguments)
 */
bool get_args_locals(st_handle handle,
                     Dwarf_Die func_die,
                     size_t* num_args,
                     variable** args,
                     size_t* num_vars,
                     variable** vars)
{
  int ret, i;
  Dwarf_Half tag = 0;
  Dwarf_Die marker, tmp, begin;
  Dwarf_Error err;

  ASSERT(num_args && args && num_vars && vars,
        "invalid arguments to get_args_locals()\n");

  *num_args = 0;
  *args = NULL;
  *num_vars = 0;
  *vars = NULL;

  /* Get to beginning of arguments or locals, if any. */
  ret = DWARF_CHK(dwarf_child(func_die, &marker, &err), "dwarf_child");
  while(ret != DW_DLV_NO_ENTRY)
  {
    DWARF_OK(dwarf_tag(marker, &tag, &err), "dwarf_tag");
    if(tag == DW_TAG_formal_parameter || tag == DW_TAG_variable)
      break;

    ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, marker, true, &tmp, &err),
                    "dwarf_siblingof_b");
    dwarf_dealloc(handle->dbg, marker, DW_DLA_DIE);
    marker = tmp;
  }

  /* Count & collect arguments (if we've got them) */
  if(tag == DW_TAG_formal_parameter)
  {
    begin = marker;
    (*num_args)++;

    /* Count the argument DIEs */
    ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, marker, true, &marker, &err),
                    "dwarf_siblingof_b");
    while(ret != DW_DLV_NO_ENTRY)
    {
      DWARF_OK(dwarf_tag(marker, &tag, &err), "dwarf_tag");
      if(tag != DW_TAG_formal_parameter) break;

      (*num_args)++;
      ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, marker, true, &tmp, &err),
                      "dwarf_siblingof_b");
      dwarf_dealloc(handle->dbg, marker, DW_DLA_DIE);
      marker = tmp;
    }

    /* Collect the argument DIEs */
    *args = (variable*)malloc(sizeof(variable) * (*num_args));
    for(i = 0; i < (*num_args); i++)
    {
      (*args)[i].die = begin;
      DWARF_CHK(dwarf_siblingof_b(handle->dbg, begin, true, &begin, &err),
                "dwarf_siblingof_b");
    }
  }

  /* Count & collect locals (if we've got them) */
  if(tag == DW_TAG_variable)
  {
    begin = marker;
    (*num_vars)++;

    /* Count the variable DIEs */
    ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, marker, true, &marker, &err),
                    "dwarf_siblingof_b");
    while(ret != DW_DLV_NO_ENTRY)
    {
      DWARF_OK(dwarf_tag(marker, &tag, &err), "dwarf_tag");
      if(tag != DW_TAG_variable) break;

      (*num_vars)++;
      ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, marker, true, &tmp, &err),
                      "dwarf_siblingof_b");
      dwarf_dealloc(handle->dbg, marker, DW_DLA_DIE);
      marker = tmp;
    }

    /* Collect the variable DIEs */
    *vars = (variable*)malloc(sizeof(variable) * (*num_vars));
    for(i = 0; i < (*num_vars); i++)
    {
      (*vars)[i].die = begin;
      DWARF_CHK(dwarf_siblingof_b(handle->dbg, begin, true, &begin, &err),
                "dwarf_siblingof_b");
    }
  }

  return true;
}

/*
 * Returns the size of the datum represented by DIE.
 */
Dwarf_Unsigned get_datum_size(st_handle handle, Dwarf_Die die, bool* is_ptr)
{
  int ret;
  const char* tag_name;
  Dwarf_Bool has_attr;
  Dwarf_Half tag;
  Dwarf_Unsigned size, elems = 1, lower, upper;
  Dwarf_Off off;
  Dwarf_Attribute attr;
  Dwarf_Die type_die, cld_die, tmp_die;
  Dwarf_Error err;

  ASSERT(is_ptr, "invalid arguments to get_datum_size()\n");

  *is_ptr = false;
  DWARF_OK(dwarf_attr(die, DW_AT_type, &attr, &err), "dwarf_attr");
  DWARF_OK(dwarf_global_formref(attr, &off, &err), "dwarf_formref");
  dwarf_dealloc(handle->dbg, attr, DW_DLA_ATTR);
  DWARF_OK(dwarf_offdie_b(handle->dbg, off, true, &type_die, &err), "dwarf_offdie_b");
  DWARF_OK(dwarf_tag(type_die, &tag, &err), "dwarf_tag");
  switch(tag)
  {
  case DW_TAG_base_type:
  case DW_TAG_structure_type:
  case DW_TAG_union_type:
  case DW_TAG_enumeration_type:
    DWARF_OK(dwarf_attr(type_die, DW_AT_byte_size, &attr, &err), "dwarf_attr");
    DWARF_OK(dwarf_formudata(attr, &size, &err), "dwarf_formudata");
    dwarf_dealloc(handle->dbg, attr, DW_DLA_ATTR);
    break;
  case DW_TAG_pointer_type:
    size = handle->ptr_size;
    *is_ptr = true;
    break;
  case DW_TAG_array_type:
    /* Get size of individual elements */
    size = get_datum_size(handle, type_die, is_ptr);

    /* Get array dimensions */
    ret = DWARF_OK(dwarf_child(type_die, &cld_die, &err), "dwarf_child");
    while(ret != DW_DLV_NO_ENTRY)
    {
      DWARF_OK(dwarf_tag(cld_die, &tag, &err), "dwarf_tag");
      if(tag == DW_TAG_subrange_type)
      {
        /* Either have DW_AT_count or DW_AT_lower_bound/upper_bound */
        DWARF_OK(dwarf_hasattr(cld_die, DW_AT_count, &has_attr, &err), "dwarf_hasattr");
        if(has_attr)
        {
          DWARF_OK(dwarf_attr(cld_die, DW_AT_count, &attr, &err), "dwarf_attr");
          DWARF_OK(dwarf_formudata(attr, &upper, &err), "dwarf_formudata");
          dwarf_dealloc(handle->dbg, attr, DW_DLA_ATTR);
          elems *= upper;
        }
        else
        {
          DWARF_OK(dwarf_hasattr(cld_die, DW_AT_lower_bound, &has_attr, &err), "dwarf_hasattr");
          if(has_attr)
          {
            DWARF_OK(dwarf_attr(cld_die, DW_AT_lower_bound, &attr, &err), "dwarf_attr");
            DWARF_OK(dwarf_formudata(attr, &lower, &err), "dwarf_formudata");
            dwarf_dealloc(handle->dbg, attr, DW_DLA_ATTR);
          }
          else lower = 0;

          DWARF_OK(dwarf_attr(cld_die, DW_AT_upper_bound, &attr, &err), "dwarf_attr");
          DWARF_OK(dwarf_formudata(attr, &upper, &err), "dwarf_formudata");
          dwarf_dealloc(handle->dbg, attr, DW_DLA_ATTR);
          upper++; // Upper bound is inclusive
          elems *= (upper - lower);
        }
      }

      ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, cld_die, true, &tmp_die, &err),
                      "dwarf_siblingof_b");
      dwarf_dealloc(handle->dbg, cld_die, DW_DLA_DIE);
      cld_die = tmp_die;
    }
    size *= elems;
    break;
  case DW_TAG_const_type:
  case DW_TAG_typedef:
  case DW_TAG_volatile_type:
    size = get_datum_size(handle, type_die, is_ptr);
    break;
  /*
   * TODO handle DW_TAG_* types for C++:
   *   class_type
   *   reference_type
   *   string_type
   */
  default:
    dwarf_get_TAG_name(tag, &tag_name);
    ST_ERR(1, "unhandled tag type '%s'\n", tag_name);
  }
  dwarf_dealloc(handle->dbg, type_die, DW_DLA_DIE);
  return size;
}

#endif /* DWARF_LIVE_VALS */

/*
 * Search through a list of location descriptions and return the one that
 * applies to PC.
 */
Dwarf_Locdesc* get_loc_desc(Dwarf_Signed num_locs,
                            Dwarf_Locdesc** locs,
                            void* pc)
{
  Dwarf_Signed i;

  ASSERT(!num_locs || (num_locs && locs), "invalid arguments to get_loc_desc()\n");

  for(i = 0; i < num_locs; i++) {
    if(locs[i]->ld_lopc <= (Dwarf_Addr)pc && (Dwarf_Addr)pc < locs[i]->ld_hipc)
      return locs[i];
  }

  return NULL;
}

/*
 * Search for and return ELF section named NAME.
 */
Elf_Scn* get_section(Elf* e, const char* name)
{
  size_t shdrstrndx = 0;
  const char* sec_name;
  Elf_Scn* scn = NULL;
  GElf_Shdr shdr;

  ASSERT(name, "invalid arguments to get_section()\n");

  if(elf_getshdrstrndx(e, &shdrstrndx)) return NULL;
  while((scn = elf_nextscn(e, scn)))
  {
    if(gelf_getshdr(scn, &shdr) != &shdr) return NULL;
    if((sec_name = elf_strptr(e, shdrstrndx, shdr.sh_name)))
      if(!strcmp(name, sec_name)) break;
  }
  return scn; // NULL if not found
}

/*
 * Get the number of entries in section SEC_NAME.
 */
int64_t get_num_entries(Elf* e, const char* sec_name)
{
  Elf_Scn* scn;
  GElf_Shdr shdr;

  if(!(scn = get_section(e, sec_name))) return -1;
  if(gelf_getshdr(scn, &shdr) != &shdr) return -1;
  return (shdr.sh_size / shdr.sh_entsize);
}

/*
 * Return the call site entries in SEC_NAME.
 */
const call_site* get_call_sites(Elf* e, const char* sec_name)
{
  Elf_Scn* scn;
  Elf_Data* data = NULL;

  if(!(scn = get_section(e, sec_name))) return NULL;
  if(!(data = elf_getdata(scn, data))) return NULL;
  return (const call_site*)data->d_buf;
}

/*
 * Return the live value entries in SEC_NAME.
 */
#if _LIVE_VALS == STACKMAP_LIVE_VALS
const call_site_value* get_call_site_values(Elf* e, const char* sec_name)
{
  Elf_Scn* scn;
  Elf_Data* data = NULL;

  if(!(scn = get_section(e, sec_name))) return NULL;
  if(!(data = elf_getdata(scn, data))) return NULL;
  return (const call_site_value*)data->d_buf;
}
#endif

/*
 * Search through call site entries for the specified return address.
 */
bool get_site_by_addr(st_handle handle, void* ret_addr, call_site* cs)
{
  bool found = false;
  size_t min = 0;
  size_t max = (handle->sites_count - 1);
  size_t mid;
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
  size_t min = 0;
  size_t max = (handle->sites_count - 1);
  size_t mid;

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

