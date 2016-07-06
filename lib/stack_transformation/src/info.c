/*
 * Functions for querying information about DWARF & call frames.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/10/2015
 */

#include "stack_transform.h"
#include "func.h"
#include "query.h"

///////////////////////////////////////////////////////////////////////////////
// Internal API
///////////////////////////////////////////////////////////////////////////////

static void print_func_info(st_handle handle, Dwarf_Die die, int level);

///////////////////////////////////////////////////////////////////////////////
// Debugging information
///////////////////////////////////////////////////////////////////////////////

/*
 * Search compilation units (i.e. .c files) in .debug_info to see which file
 * the specified instruction pointer references.  Return the compilation unit's
 * name.
 */
char* st_get_cu_name(st_handle handle, void* pc)
{
  char* cu_name = NULL;
  Dwarf_Die cu_die;
  Dwarf_Error err;

  if(!handle || !pc)
  {
    ST_WARN("invalid arguments\n");
    return NULL;
  }

  if((cu_die = get_cu_die(handle, pc)))
  {
    DWARF_OK(dwarf_diename(cu_die, &cu_name, &err), "dwarf_diename");
    dwarf_dealloc(handle->dbg, cu_die, DW_DLA_DIE);
  }
  else
    ST_WARN("could not find compilation unit for PC=%p\n", pc);

  return cu_name;
}

/*
 * Search functions in .debug_info to see which function the specified
 * instruction pointer references.  Return the function's name.
 */
char* st_get_func_name(st_handle handle, void* pc)
{
  char* func_name = NULL;
  Dwarf_Die func_die;
  Dwarf_Error err;

  if(!handle || !pc)
  {
    ST_WARN("invalid arguments\n");
    return NULL;
  }

  if(get_func_die(handle, pc, NULL, &func_die))
  {
    DWARF_OK(dwarf_diename(func_die, &func_name, &err), "dwarf_diename");
    dwarf_dealloc(handle->dbg, func_die, DW_DLA_DIE);
  }
  else
    ST_WARN("could not find function for PC=%p\n", pc);

  return func_name;
}

/* 
 * Search functions in .debug_info to see which function the specified
 * instruction pointer references.  If found, print information about all
 * children DIEs, or print a warning otherwise.
 */
void st_print_func_info(st_handle handle, void* pc)
{
  Dwarf_Die func_die;

  if(get_func_die(handle, pc, NULL, &func_die))
  {
    print_func_info(handle, func_die, 0);
    dwarf_dealloc(handle->dbg, func_die, DW_DLA_DIE);
  }
  else
    ST_WARN("could not find function for PC=%p\n", pc);
}

/*
 * Print detailed information about the function's arguments and local
 * variables.  In particular, print the steps required to calculate the
 * variable's location.
 */
void st_print_func_loc_desc(st_handle handle, void* pc)
{
  func_info func;

  if(!handle || !pc)
  {
    ST_WARN("invalid arguments\n");
    return;
  }

  if((func = get_func_by_pc(handle, pc)))
  {
    printf("Function: %s\n", get_func_name(func));

#if _LIVE_VALS == DWARF_LIVE_VALS
    size_t i, j, k;
    const variable* arg, *var;
    const char* op_name;

    /* Print information about arguments */
    printf("  Number of arguments: %lu\n", num_args(func));
    for(i = 0; i < num_args(func); i++)
    {
      arg = get_arg_by_pos(func, i);
      ASSERT(arg, "invalid argument\n");
#ifdef _DEBUG
      printf("  Argument: %s (%llu bytes)\n", arg->name, arg->size);
#else
      printf("  Argument: %llu bytes\n", arg->size);
#endif
      for(j = 0; j < arg->num_locs; j++)
      {
        printf("    0x%llx - 0x%llx:\n", arg->locs[j]->ld_lopc, arg->locs[j]->ld_hipc);
        for(k = 0; k < arg->locs[j]->ld_cents; k++)
        {
          dwarf_get_OP_name(arg->locs[j]->ld_s[k].lr_atom, &op_name);
          printf("      [%lu] %s\n", k, op_name);
        }
      }
    }

    /* Print information about variables */
    printf("\n  Number of variables: %lu\n", num_vars(func));
    for(i = 0; i < num_vars(func); i++)
    {
      var = get_var_by_pos(func, i);
      ASSERT(arg, "invalid variable\n");
#ifdef _DEBUG
      printf("  Variable: %s (%llu bytes)\n", var->name, var->size);
#else
      printf("  Variable: %llu bytes\n", var->size);
#endif
      for(j = 0; j < var->num_locs; j++)
      {
        printf("    0x%llx - 0x%llx:\n", var->locs[j]->ld_lopc, var->locs[j]->ld_hipc);
        for(k = 0; k < var->locs[j]->ld_cents; k++)
        {
          dwarf_get_OP_name(var->locs[j]->ld_s[k].lr_atom, &op_name);
          printf("      [%lu] %s\n", k, op_name);
        }
      }
    }
#else /* STACKMAP_LIVE_VALS */
    printf(" (no argument/variable information)\n");
#endif

    free_func_info(handle, func);
  }
  else
    ST_WARN("could not find function for PC=%p\n", pc);
}

///////////////////////////////////////////////////////////////////////////////
// Internal API implementation
///////////////////////////////////////////////////////////////////////////////

static void print_func_info(st_handle handle, Dwarf_Die die, int level)
{
  int ret, i;
  char* name = NULL;
  const char* tag_name = NULL;
  Dwarf_Die sib_die, tmp_die;
  Dwarf_Attribute* atlist;
  Dwarf_Signed atcnt;
  Dwarf_Half tag;
  Dwarf_Bool has_name;
  Dwarf_Error err;

  /* Print this DIE's information */
  DWARF_OK(dwarf_tag(die, &tag, &err), "dwarf_tag");
  if(tag == DW_TAG_subprogram ||
     tag == DW_TAG_formal_parameter ||
     tag == DW_TAG_variable)
  {
    DWARF_OK(dwarf_get_TAG_name(tag, &tag_name), "dwarf_get_TAG_name");
    DWARF_OK(dwarf_attrlist(die, &atlist, &atcnt, &err), "dwarf_attrlist");
    DWARF_OK(dwarf_hasattr(die, DW_AT_name, &has_name, &err), "dwarf_hasattr");

    /* Print name (if available) & tag type */
    for(i = 0; i < level; i++) ST_RAW_INFO(" ");
    if(has_name)
    {
      DWARF_OK(dwarf_diename(die, &name, &err), "dwarf_diename");
      ST_RAW_INFO("%s (%s)\n", name, tag_name);
      dwarf_dealloc(handle->dbg, name, DW_DLA_STRING);
    }
    else
      ST_RAW_INFO("(no name) (%s)\n", tag_name);
  }
  else
    return;

  /* Traverse this DIE's children */
  ret = DWARF_CHK(dwarf_child(die, &sib_die, &err), "dwarf_child");
  if(ret != DW_DLV_NO_ENTRY)
  {
    for(i = 0; i < level; i++) ST_INFO(" ");
    ST_RAW_INFO("\\\n");
  }

  while(ret != DW_DLV_NO_ENTRY)
  {
    print_func_info(handle, sib_die, level + 1);
    ret = DWARF_CHK(dwarf_siblingof_b(handle->dbg, sib_die, true, &tmp_die, &err),
                    "dwarf_siblingof_b");
    dwarf_dealloc(handle->dbg, sib_die, DW_DLA_DIE);
    sib_die = tmp_die;
  }
}

