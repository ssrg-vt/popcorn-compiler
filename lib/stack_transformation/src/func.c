/*
 * Functions for reading, querying, and freeing function-specific information.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/11/2015
 */

#include "func.h"
#include "query.h"

///////////////////////////////////////////////////////////////////////////////
// File-local API
///////////////////////////////////////////////////////////////////////////////

/*
 * A function information descriptor.  Used to query information about how to
 * find argument and variable locations when executing a given function.
 */
struct func_info
{
  /* General information */
  Dwarf_Die cu_die;
  Dwarf_Die die;
#ifdef _DEBUG
  char* name;
#endif
  Dwarf_Addr cu_start_addr;
  Dwarf_Addr start_addr;
  Dwarf_Addr end_addr;

  // Note: with LLVM's stackmap intrinsic, live values are associated with call
  // sites, not functions
#if _LIVE_VALS == DWARF_LIVE_VALS
  /* Frame base location description (should one be 1) */
  Dwarf_Locdesc* fb_desc;

  /* Argument & local variable information */
  size_t num_args;
  variable* args;
  size_t num_vars;
  variable* vars;
#endif
};

/* Common function information initialization code. */
static void init_func_info(st_handle, func_info);

#if _LIVE_VALS == DWARF_LIVE_VALS

/* Extract relevant information from DIEs. */
static inline void die_to_variable(st_handle, Dwarf_Addr, variable*);

/*
 * Free argument & variable information.  Does NOT free the struct pointer
 * itself, but only the data contained therein.
 */
static inline void free_variable(st_handle, variable*);

#endif /* DWARF_LIVE_VALS */

///////////////////////////////////////////////////////////////////////////////
// Function handling
///////////////////////////////////////////////////////////////////////////////

/*
 * Allocate and read in information about a function's arguments & local
 * variables.  The returned handle can be used to query information about the
 * function, including where arguments & variables are located.
 */
func_info get_func_by_pc(st_handle handle, void* pc)
{
  func_info new_info;

  TIMER_FG_START(get_func_by_pc);
  ST_INFO("Getting function for PC=%p\n", pc);

  new_info = (func_info)malloc(sizeof(struct func_info));
  if(get_func_die(handle, pc, &new_info->cu_die, &new_info->die))
  {
    init_func_info(handle, new_info);
    ST_INFO("Function: '%s' (start=0x%llx)\n", new_info->name, new_info->start_addr);
  }
  else
  {
    ST_WARN("no matching function\n");
    free(new_info);
    new_info = NULL;
  }

  TIMER_FG_STOP(get_func_by_pc);
  return new_info;
}

/*
 * Allocate and read in information about a function's arguments & local
 * variables.  The returned handle can be used to query information about the
 * function, including where arguments & variables are located.
 */
func_info get_func_by_name(st_handle handle,
                                   const char* cu,
                                   const char* func)
{
  func_info new_info;

  TIMER_FG_START(get_func_by_name);
  ST_INFO("Getting function '%s'\n", func);

  new_info = (func_info)malloc(sizeof(struct func_info));
  if(get_named_func_die(handle, cu, func, &new_info->cu_die, &new_info->die))
  {
    init_func_info(handle, new_info);
    ST_INFO("Starting PC=0x%llx\n", new_info->start_addr);
  }
  else
  {
    ST_WARN("no matching function\n");
    free(new_info);
    new_info = NULL;
  }

  TIMER_FG_STOP(get_func_by_name);
  return new_info;
}

/*
 * Free a function information descriptor.
 */
void free_func_info(st_handle handle, func_info info)
{
  TIMER_FG_START(free_func_info);

#if _LIVE_VALS == DWARF_LIVE_VALS
  size_t i;

  if(info->fb_desc)
  {
    dwarf_dealloc(handle->dbg, info->fb_desc->ld_s, DW_DLA_LOC_BLOCK);
    dwarf_dealloc(handle->dbg, info->fb_desc, DW_DLA_LOCDESC);
  }

  if(info->num_args)
  {
    for(i = 0; i < info->num_args; i++)
      free_variable(handle, &info->args[i]);
    free(info->args);
  }

  if(info->num_vars)
  {
    for(i = 0; i < info->num_vars; i++)
      free_variable(handle, &info->vars[i]);
    free(info->vars);
  }
#endif

#ifdef _DEBUG
  dwarf_dealloc(handle->dbg, info->name, DW_DLA_STRING);
#endif
  dwarf_dealloc(handle->dbg, info->cu_die, DW_DLA_DIE);
  dwarf_dealloc(handle->dbg, info->die, DW_DLA_DIE);
  free(info);

  TIMER_FG_STOP(free_func_info);
}

bool is_func(func_info handle, void* pc)
{
  if(!handle) return false;
  else if(handle->start_addr <= (uint64_t)pc &&
          (uint64_t)pc <= handle->end_addr)
    return true;
  else return false;
}

/*
 * Return the function's name.
 */
const char* get_func_name(func_info handle)
{
#ifdef _DEBUG
  return handle->name;
#else
  ASSERT(false, "function names are only available in debug mode\n");
  return NULL;
#endif
}

/*
 * Return the function's starting address.
 */
void* get_func_start_addr(func_info handle)
{
  return (void*)handle->start_addr;
}

#if _LIVE_VALS == DWARF_LIVE_VALS

/*
 * Return the function's frame base location description.
 */
const Dwarf_Locdesc* get_func_fb(func_info handle)
{
  return handle->fb_desc;
}

/*
 * Return the number of formal arguments for the specified function.
 */
size_t num_args(func_info handle)
{
  return handle->num_args;
}

/*
 * Search for a formal argument by name.
 */
const variable* get_arg_by_name(func_info handle, const char* name)
{
#ifdef _DEBUG
  size_t i;

  for(i = 0; i < handle->num_args; i++)
    if(!strcmp(handle->args[i].name, name))
      return &handle->args[i];

  return NULL;
#else
  ASSERT(false, "get_arg_by_name only allowed in debug mode\n");
  return NULL;
#endif
}

/*
 * Return a formal argument by position in the argument list.
 */
const variable* get_arg_by_pos(func_info handle, size_t pos)
{
  if(pos < handle->num_args) return &handle->args[pos];
  else return NULL;
}

/*
 * Return the number of local variables for the specified function.
 */
size_t num_vars(func_info handle)
{
  return handle->num_vars;
}

/*
 * Search for a local variable by name.
 */
const variable* get_var_by_name(func_info handle, const char* name)
{
#ifdef _DEBUG
  size_t i;

  for(i = 0; i < handle->num_vars; i++)
    if(!strcmp(handle->vars[i].name, name))
      return &handle->vars[i];

  return NULL;
#else
  ASSERT(false, "get_var_by_name only allowed in debug mode\n");
  return NULL;
#endif
}

/*
 * Return a formal argument by position in the argument list.
 */
const variable* get_var_by_pos(func_info handle, size_t pos)
{
  if(pos < handle->num_vars) return &handle->vars[pos];
  else return NULL;
}

#endif /* DWARF_LIVE_VALS */

///////////////////////////////////////////////////////////////////////////////
// File-local API
///////////////////////////////////////////////////////////////////////////////

static void init_func_info(st_handle handle, func_info new_info)
{
  Dwarf_Bool has_attr;
  Dwarf_Half form;
  enum Dwarf_Form_Class class;
  Dwarf_Error err;

  TIMER_FG_START(init_func_info);

#ifdef _DEBUG
  DWARF_OK(dwarf_diename(new_info->die, &new_info->name, &err), "dwarf_diename");
#endif

  /* Get CU starting address (apply to location description offsets) */
  DWARF_OK(dwarf_hasattr(new_info->cu_die, DW_AT_low_pc, &has_attr, &err), "dwarf_hasattr");
  if(has_attr)
    DWARF_OK(dwarf_lowpc(new_info->cu_die, &new_info->cu_start_addr, &err), "dwarf_lowpc");
  else new_info->cu_start_addr = 0;

  /* Get function address range */
  DWARF_OK(dwarf_hasattr(new_info->die, DW_AT_low_pc, &has_attr, &err), "dwarf_hasattr");
  if(has_attr)
  {
    DWARF_OK(dwarf_lowpc(new_info->die, &new_info->start_addr, &err), "dwarf_lowpc");
    DWARF_OK(dwarf_highpc_b(new_info->die, &new_info->end_addr, &form, &class, &err), "dwarf_highpc");
    if(class == DW_FORM_CLASS_CONSTANT) new_info->end_addr += new_info->start_addr;
  }
  else
  {
    new_info->start_addr = 0;
    new_info->end_addr = 0;
  }

#if _LIVE_VALS == DWARF_LIVE_VALS
  size_t i;
  Dwarf_Signed num_fb_desc;
  Dwarf_Unsigned exprlen;
  Dwarf_Ptr exprblock;
  Dwarf_Attribute attr;

  /* Get frame base location description (there should only be 1!). */
  DWARF_OK(dwarf_hasattr(new_info->die, DW_AT_frame_base, &has_attr, &err), "dwarf_hasattr");
  if(has_attr)
  {
    DWARF_OK(dwarf_attr(new_info->die, DW_AT_frame_base, &attr, &err), "dwarf_attr");
    DWARF_OK(dwarf_formexprloc(attr, &exprlen, &exprblock, &err), "dwarf_formexprloc");
    dwarf_dealloc(handle->dbg, attr, DW_DLA_ATTR);
    DWARF_OK(dwarf_loclist_from_expr_b(handle->dbg,
                                       exprblock,
                                       exprlen,
                                       sizeof(Dwarf_Addr), /* Assumed to be 8 bytes */
                                       sizeof(Dwarf_Off), /* Assumed to be 8 bytes */
                                       4, /* CU version = 4 per DWARF4 standard */
                                       &new_info->fb_desc,
                                       &num_fb_desc, /* Should always be set to 1 */
                                       &err), "dwarf_loclist_from_expr_b");
    ST_INFO("Found frame base location description\n");
  }

  /* Read in argument & local variable information. */
  // Note: it's okay if there are zero variables and/or arguments.
  TIMER_FG_START(var_lookup);
#ifdef _FUNC_QUERY_OPT
  get_args_locals(handle,
                  new_info->die,
                  &new_info->num_args,
                  &new_info->args,
                  &new_info->num_vars,
                  &new_info->vars);

  TIMER_FG_STOP(var_lookup);

  TIMER_FG_START(var_prep);

  for(i = 0; i < new_info->num_args; i++)
    die_to_variable(handle, new_info->cu_start_addr, &new_info->args[i]);
  for(i = 0; i < new_info->num_vars; i++)
    die_to_variable(handle, new_info->cu_start_addr, &new_info->vars[i]);
#else
  Dwarf_Die* arg_dies, *var_dies;
  new_info->num_args = get_children(handle,
                                    new_info->die,
                                    DW_TAG_formal_parameter,
                                    &arg_dies);
  new_info->num_vars = get_children(handle,
                                    new_info->die,
                                    DW_TAG_variable,
                                    &var_dies);

  TIMER_FG_STOP(var_lookup);

  TIMER_FG_START(var_prep);

  if(new_info->num_args > 0)
  {
    new_info->args = (variable*)malloc(sizeof(variable) * new_info->num_args);
    for(i = 0; i < new_info->num_args; i++)
    {
      new_info->args[i].die = arg_dies[i];
      die_to_variable(handle, new_info->cu_start_addr, &new_info->args[i]);
    }
    free(arg_dies);
  }

  if(new_info->num_vars > 0)
  {
    new_info->vars = (variable*)malloc(sizeof(variable) * new_info->num_vars);
    for(i = 0; i < new_info->num_vars; i++)
    {
      new_info->vars[i].die = var_dies[i];
      die_to_variable(handle, new_info->cu_start_addr, &new_info->vars[i]);
    }
    free(var_dies);
  }
#endif /* _FUNC_QUERY_OPT */
  TIMER_FG_STOP(var_prep);

#endif /* DWARF_LIVE_VALS */
  TIMER_FG_STOP(init_func_info);
}

#if _LIVE_VALS == DWARF_LIVE_VALS

static void die_to_variable(st_handle handle,
                            Dwarf_Addr start_addr,
                            variable* var)
{
  int i;
  Dwarf_Bool has_attr;
  Dwarf_Attribute attr;
  Dwarf_Error err;

  /* Get name & size */
#ifdef _DEBUG
  DWARF_OK(dwarf_diename(var->die, &var->name, &err), "dwarf_diename");
#endif
  TIMER_FG_START(datum_size);
  var->size = get_datum_size(handle, var->die, &var->is_ptr);
  TIMER_FG_STOP(datum_size);
  ST_INFO("%s (%llu bytes, is pointer? %d)\n",
          var->name, var->size, var->is_ptr);

  /* Get location descriptions */
  TIMER_FG_START(datum_location);
  DWARF_OK(dwarf_hasattr(var->die, DW_AT_location, &has_attr, &err), "dwarf_hasattr");
  if(has_attr)
  {
    DWARF_OK(dwarf_attr(var->die, DW_AT_location, &attr, &err), "dwarf_attr");
    DWARF_OK(dwarf_loclist_n(attr, &var->locs, &var->num_locs, &err), "dwarf_loclist_n");
    dwarf_dealloc(handle->dbg, attr, DW_DLA_ATTR);

    // TODO better way to detect if correction is needed?
    if(!(var->locs[0]->ld_lopc == 0 && var->locs[0]->ld_hipc == UINT64_MAX) &&
        var->locs[0]->ld_lopc < start_addr) {
      for(i = 0; i < var->num_locs; i++) {
        var->locs[i]->ld_lopc += start_addr;
        var->locs[i]->ld_hipc += start_addr;
      }
    }
  }
  else
  {
    var->num_locs = 0;
    var->locs = NULL;
  }
  TIMER_FG_STOP(datum_location);
}

static void free_variable(st_handle handle, variable* var)
{
  size_t i;

  dwarf_dealloc(handle->dbg, var->die, DW_DLA_DIE);
#ifdef _DEBUG
  dwarf_dealloc(handle->dbg, var->name, DW_DLA_STRING);
#endif
  for(i = 0; i < var->num_locs; i++)
  {
    dwarf_dealloc(handle->dbg, var->locs[i]->ld_s, DW_DLA_LOC_BLOCK);
    dwarf_dealloc(handle->dbg, var->locs[i], DW_DLA_LOCDESC);
  }
  dwarf_dealloc(handle->dbg, var->locs, DW_DLA_LIST);
}

#endif /* DWARF_LIVE_VALS */

