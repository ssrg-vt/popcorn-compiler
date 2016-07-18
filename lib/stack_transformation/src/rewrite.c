/*
 * Implements the main rewriting logic for stack transformation.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/12/2015
 */

#include "stack_transform.h"
#include "func.h"
#include "data.h"
#include "unwind.h"
#include "query.h"
#include "properties.h"
#include "util.h"

///////////////////////////////////////////////////////////////////////////////
// File-local API & definitions
///////////////////////////////////////////////////////////////////////////////

#if _TLS_IMPL == COMPILER_TLS

/*
 * Per-thread rewriting context.  We can declare these at compile-time,
 * because we know each thread will only ever use a pair of these at a time.
 */
static __thread struct rewrite_context src_ctx, dest_ctx;

#endif

/*
 * Initialize an architecture-specific (source) context using previously
 * initialized REGSET and HANDLE.
 */
static rewrite_context init_src_context(st_handle handle,
                                        void* regset,
                                        void* sp_base);

/*
 * Initialize an architecture-specific (destination) context using destination
 * stack SP_BASE and program location PC.  Store destination REGSET pointer
 * to be filled with destination thread's resultant register state.
 */
static rewrite_context init_dest_context(st_handle handle,
                                         void* regset,
                                         void* sp_base,
                                         void* pc);

/*
 * Initialize data pools for constant-time allocation.
 */
static void init_data_pools(rewrite_context ctx, size_t num_regs);

/*
 * Free previously-allocated context information.
 */
static void free_context(rewrite_context ctx);

/*
 * Free a context's data pools.
 */
static void free_data_pools(rewrite_context ctx);

/*
 * Unwind the source stack to find all live stack frames & determine
 * destination stack size.
 */
static void unwind_and_size(rewrite_context src,
                            rewrite_context dest);

/*
 * Rewrite an individual variable from the source to destination call frame.
 * Returns true if there's a fixup needed within this stack frame.
 */
static bool rewrite_var(rewrite_context src, const variable* var_src,
                        rewrite_context dest, const variable* var_dest);

/*
 * Re-write an individual frame from the source to destination stack.
 */
static void rewrite_frame(rewrite_context src, rewrite_context dest);

/*
 * Re-write the outer-most frame, which doesn't need to copy over local
 * variables.
 */
static void rewrite_frame_outer(rewrite_context src, rewrite_context dest);

///////////////////////////////////////////////////////////////////////////////
// Perform stack transformation
///////////////////////////////////////////////////////////////////////////////

/*
 * Perform stack transformation in its entirety, from source to destination.
 */
int st_rewrite_stack(st_handle handle_src,
                     void* regset_src,
                     void* sp_base_src,
                     st_handle handle_dest,
                     void* regset_dest,
                     void* sp_base_dest)
{
  rewrite_context src, dest;
  uint64_t* saved_fbp, *fbp;

  if(!handle_src || !regset_src || !sp_base_src ||
     !handle_dest || !regset_dest || !sp_base_dest)
  {
    ST_WARN("invalid arguments\n");
    return 1;
  }

  TIMER_START(st_rewrite_stack);
  ST_INFO("--> Initializing rewrite (%s -> %s) <--\n",
          arch_name(handle_src->arch), arch_name(handle_dest->arch));

  /* Initialize rewriting contexts. */
  // Note: functions are aligned & we're only transforming starting at the
  // beginning of functions, so source pc == destination pc.
  src = init_src_context(handle_src, regset_src, sp_base_src);
  dest = init_dest_context(handle_dest,
                           regset_dest,
                           sp_base_dest,
                           ACT(src).regs->pc(ACT(src).regs));
  if(!src || !dest)
  {
    if(src) free_context(src);
    if(dest) free_context(dest);
    return 1;
  }

  ST_INFO("--> Unwinding source stack to find live activations <--\n");

  /* Unwind source stack to determine destination stack size. */
  unwind_and_size(src, dest);

  // Note: the following code is brittle -- it has to happen in this *exact*
  // order because of the way the stack is unwound and information in the
  // current & surrounding frames is accessed.  Modify with care!

  ST_INFO("--> Rewriting from source to destination stack <--\n");

  /* Rewrite outer-most frame. */
  ST_INFO("--> Rewriting outermost frame <--\n");

  rewrite_frame_outer(src, dest);
  set_return_address(dest, (void*)NEXT_ACT(dest).site.addr);
  pop_frame(dest);
  ACT(dest).function = get_func_by_pc(dest->handle, ACT(dest).regs->pc(ACT(dest).regs));
  ASSERT(ACT(dest).function, "could not get function information\n");
#if _LIVE_VALS == DWARF_LIVE_VALS
  if(ACT(dest).site.has_fbp)
  {
    fbp = ACT(dest).regs->sp(ACT(dest).regs) + ACT(dest).site.fbp_offset;
    ASSERT(fbp, "invalid frame pointer\n");
    ACT(dest).regs->set_fbp(ACT(dest).regs, fbp);
    PREV_ACT(dest).regs->set_fbp(PREV_ACT(dest).regs, fbp);

    ST_INFO("Set FP=%p for outer-most frame\n", fbp);
  }
#else /* STACKMAP_LIVE_VALS */
  // Note: the LLVM stackmap intrinsic disables -fomit-frame-pointer
  // optimization, so we *must* have a frame pointer.
  fbp = ACT(dest).regs->sp(ACT(dest).regs) + ACT(dest).site.fbp_offset;
  ASSERT(fbp, "invalid frame pointer\n");
  ACT(dest).regs->set_fbp(ACT(dest).regs, fbp);
  PREV_ACT(dest).regs->set_fbp(PREV_ACT(dest).regs, fbp);

  ST_INFO("Set FP=%p for outer-most frame\n", fbp);
#endif
  read_unwind_rules(dest);

  /* Rewrite rest of frames. */
  // Note: no need to rewrite libc start function, no state to maintain there
  for(src->act = 1; src->act < src->num_acts - 1; src->act++)
  {
    ST_INFO("--> Rewriting frame %d <--\n", src->act);

    rewrite_frame(src, dest);
    set_return_address(dest, (void*)NEXT_ACT(dest).site.addr);
#if _LIVE_VALS == DWARF_LIVE_VALS
    if(NEXT_ACT(dest).site.has_fbp)
    {
      saved_fbp = get_savedfbp_loc(dest);
      ASSERT(saved_fbp, "invalid saved frame pointer location\n");
      pop_frame(dest);
      ACT(dest).function = get_func_by_pc(dest->handle, ACT(dest).regs->pc(ACT(dest).regs));
      fbp = ACT(dest).regs->sp(ACT(dest).regs) + ACT(dest).site.fbp_offset;
      ASSERT(fbp, "invalid frame pointer\n");
      ACT(dest).regs->set_fbp(ACT(dest).regs, fbp);
      *saved_fbp = (uint64_t)fbp;

      ST_INFO("Saved old FP=%p to %p\n", fbp, saved_fbp);
    }
    else
    {
      pop_frame(dest);
      ACT(dest).function = get_func_by_pc(dest->handle, ACT(dest).regs->pc(ACT(dest).regs));
    }
#else /* STACKMAP_LIVE_VALS */
    saved_fbp = get_savedfbp_loc(dest);
    ASSERT(saved_fbp, "invalid saved frame pointer location\n");
    pop_frame(dest);
    ACT(dest).function = get_func_by_pc(dest->handle, ACT(dest).regs->pc(ACT(dest).regs));
    fbp = ACT(dest).regs->sp(ACT(dest).regs) + ACT(dest).site.fbp_offset;
    ASSERT(fbp, "invalid frame pointer\n");
    ACT(dest).regs->set_fbp(ACT(dest).regs, fbp);
    *saved_fbp = (uint64_t)fbp;

    ST_INFO("Saved old FP=%p to %p\n", fbp, saved_fbp);
#endif
    ASSERT(ACT(dest).function, "could not get function information\n");
    read_unwind_rules(dest);
  }

  /* Copy out register state for destination & clean up. */
  dest->acts[0].regs->regset_copyout(dest->acts[0].regs, dest->regs);
  free_context(dest);
  free_context(src);

  ST_INFO("Finished rewrite\n");

  TIMER_STOP(st_rewrite_stack);
  TIMER_PRINT;

#ifdef _LOG
  fflush(__log);
#endif

  return 0;
}

/*
 * Perform stack transformation for the top frame.  Replace return address so
 * that we can intercept and transform frames on demand.
 */
int st_rewrite_ondemand(st_handle handle_src,
                        void* regset_src,
                        void* sp_base_src,
                        st_handle handle_dest,
                        void* regset_dest,
                        void* sp_base_dest)
{
  ASSERT(false, "on-demand rewriting not yet supported\n");

  // Note: don't clean up, as we'll need the contexts when the thread needs to
  // re-write the next frame
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// File-local API implementation
///////////////////////////////////////////////////////////////////////////////

/*
 * Initialize an architecture-specific (source) context using previously
 * initialized REGSET and HANDLE.
 */
static rewrite_context init_src_context(st_handle handle,
                                        void* regset,
                                        void* sp_base)
{
  rewrite_context ctx;

  TIMER_START(init_src_context);

#if _TLS_IMPL == COMPILER_TLS
  ctx = &src_ctx;
#else
  ctx = (rewrite_context)malloc(sizeof(struct rewrite_context));
#endif
  ctx->num_acts = 0;
  ctx->act = 0;
  list_init(fixup, &ctx->stack_pointers);
  ACT(ctx).regs = handle->regops->regset_init(regset);
  ACT(ctx).function = get_func_by_pc(handle, ACT(ctx).regs->pc(ACT(ctx).regs));
  ctx->regs = regset;
  ctx->stack_base = sp_base;
  ctx->stack = ACT(ctx).regs->sp(ACT(ctx).regs);
  ctx->handle = handle;
  init_data_pools(ctx, ACT(ctx).regs->num_regs);
  read_unwind_rules(ctx);

  /* Correct PC for destination context. */
  // Note: must come after read_unwind_rules() to unwind the current frame with
  // correct register info
  ACT(ctx).regs->set_pc(ACT(ctx).regs, get_func_start_addr(ACT(ctx).function));
  if(!get_site_by_addr(handle, ACT(ctx).regs->pc(ACT(ctx).regs), &ACT(ctx).site))
    ASSERT(false, "could not get function argument information\n");

  ASSERT(ctx->stack, "invalid stack pointer\n");
  ASSERT(ACT(ctx).function, "could not get starting function information\n");

  TIMER_STOP(init_src_context);
  return ctx;
}

/*
 * Initialize an architecture-specific (destination) context using destination
 * stack SP_BASE and program location PC.  Store destination REGSET pointer
 * to be filled with destination thread's resultant register state.
 */
static rewrite_context init_dest_context(st_handle handle,
                                         void* regset,
                                         void* sp_base,
                                         void* pc)
{
  rewrite_context ctx;

  TIMER_START(init_dest_context);

#if _TLS_IMPL == COMPILER_TLS
  ctx = &dest_ctx;
#else
  ctx = (rewrite_context)malloc(sizeof(struct rewrite_context));
#endif
  ctx->num_acts = 0;
  ctx->act = 0;
  list_init(fixup, &ctx->stack_pointers);
  ACT(ctx).regs = handle->regops->regset_default();
  ACT(ctx).regs->set_pc(ACT(ctx).regs, pc);
  ACT(ctx).function = get_func_by_pc(handle, pc);
  if(!get_site_by_addr(handle, pc, &ACT(ctx).site))
    ASSERT(false, "could not get function argument information\n");
  ctx->regs = regset;
  ctx->stack_base = sp_base;
  ctx->handle = handle;
  init_data_pools(ctx, ACT(ctx).regs->num_regs);
  // Note: cannot read unwind rules yet because CFA will be invalid (no SP)

  ASSERT(ACT(ctx).function, "could not get starting function information\n");
  TIMER_STOP(init_dest_context);
  return ctx;
}

/*
 * Initialize the context's data pools.
 */
static void init_data_pools(rewrite_context ctx, size_t num_regs)
{
  ctx->regtable_pool =
    malloc(sizeof(Dwarf_Regtable_Entry3) * num_regs * MAX_FRAMES);
  ctx->callee_saved_pool = malloc(bitmap_size(num_regs) * MAX_FRAMES);
  ASSERT(ctx->regtable_pool && ctx->callee_saved_pool,
        "could not initialize data pools\n");
}

/*
 * Free an architecture-specific context.
 */
static void free_context(rewrite_context ctx)
{
  int i;
  node_t(fixup)* node;

  TIMER_FG_START(free_context);

  node = list_begin(fixup, &ctx->stack_pointers);
  while(node)
  {
    ST_WARN("could not find stack pointer fixup for %p (in activation %d)\n",
            node->data.src_addr, node->act);
    node = list_remove(fixup, &ctx->stack_pointers, node);
  }

  for(i = 0; i < ctx->num_acts; i++)
    free_activation(ctx->handle, &ctx->acts[i]);
  free_data_pools(ctx);
#if _TLS_IMPL != COMPILER_TLS
  free(ctx);
#endif

  TIMER_FG_STOP(free_context);
}

/*
 * Free a rewrite context's data pools.
 */
static void free_data_pools(rewrite_context ctx)
{
  free(ctx->regtable_pool);
  free(ctx->callee_saved_pool);
}

/*
 * Unwind source stack to find live frames & size destination stack.
 * Simultaneously caches function & call-site information.
 */
static void unwind_and_size(rewrite_context src,
                            rewrite_context dest)
{
  size_t stack_size = 0;

  TIMER_FG_START(unwind_and_size);

  /*
   * Unwind the source stack in order to calculate destination stack size.
   */
  pop_frame(src);
  src->num_acts++;
  dest->num_acts++;
  dest->act++;
  ACT(src).function = first_frame(src->handle, ACT(src).regs->pc(ACT(src).regs));

  while(!ACT(src).function)
  {
    ACT(src).function = get_func_by_pc(src->handle, ACT(src).regs->pc(ACT(src).regs));
    ASSERT(ACT(src).function, "could not get function information\n");
    read_unwind_rules(src);

    /*
     * Call site meta-data will be used to get return addresses & frame-base
     * pointer locations.
     */
    if(!get_site_by_addr(src->handle, ACT(src).regs->pc(ACT(src).regs), &ACT(src).site))
      ASSERT(false, "could not get source call site information (address=%p)\n",
                    ACT(src).regs->pc(ACT(src).regs));
    if(!get_site_by_id(dest->handle, ACT(src).site.id, &ACT(dest).site))
      ASSERT(false, "could not get destination call site information (address=%p, ID=%ld)\n",
                    ACT(src).regs->pc(ACT(src).regs), ACT(src).site.id);

    // Note: this might overestimate the size for frames without a base pointer
    // but that shouldn't be a problem.
    stack_size += ACT(dest).site.fbp_offset + (2 * dest->handle->ptr_size); // old FBP & RA

    pop_frame(src);
    src->num_acts++;
    dest->num_acts++;
    dest->act++;
    ACT(src).function = first_frame(src->handle, ACT(src).regs->pc(ACT(src).regs));
  }

  /* Get frame information for starting function */
  ASSERT(ACT(src).function, "could not get starting function information\n");
  if(!get_site_by_addr(src->handle, ACT(src).regs->pc(ACT(src).regs), &ACT(src).site))
    ASSERT(false, "could not get source call site information (address=%p)\n",
                  ACT(src).regs->pc(ACT(src).regs));
  if(!get_site_by_id(dest->handle, ACT(src).site.id, &ACT(dest).site))
    ASSERT(false, "could not get destination call site information (address=%p, ID=%ld)\n",
                  ACT(src).regs->pc(ACT(src).regs), ACT(src).site.id);
  stack_size += ACT(dest).site.fbp_offset + (2 * dest->handle->ptr_size); // old FBP & RA
  src->num_acts++;
  dest->num_acts++;
  ASSERT(stack_size < MAX_STACK_SIZE, "invalid stack size\n");

  ST_INFO("Stack initial function: '%s'\n", get_func_name(ACT(src).function));
  ST_INFO("Number of live activations: %d\n", src->num_acts);
  ST_INFO("Destination stack size: %lu\n", stack_size);

  // TODO this could be cleaner
  if(ACT(src).function == src->handle->start_main)
    ACT(dest).function = dest->handle->start_main;
  else ACT(dest).function = dest->handle->start_thread;
  ASSERT(ACT(src).function && ACT(dest).function, "invalid start function\n");

  /* Reset to outer-most frame. */
  src->act = 0;
  dest->act = 0;

  /* Set destination stack pointer (align if necessary). */
  dest->stack = dest->stack_base - stack_size;
  if(dest->handle->props->sp_needs_align)
    dest->stack = dest->handle->props->align_sp(dest->stack);
  ACT(dest).regs->set_sp(ACT(dest).regs, dest->stack);

  ST_INFO("Top of new stack: %p\n", dest->stack);

  /* Clear the callee-saved bitmaps for all destination frames. */
  memset(dest->callee_saved_pool, 0, sizeof(STORAGE_TYPE) *
                                     bitmap_size(ACT(dest).regs->num_regs) *
                                     dest->num_acts);

  /* Read unwind rules & calculate CFA for destination since we have a SP. */
  read_unwind_rules(dest);

  /*
   * The compiler may specify arguments are located at an offset from the frame
   * pointer at all function PCs, including the ones where the frame hasn't
   * been set up.  Hard-code outer frame's FBP for this situation.
   */
  ACT(dest).regs->set_fbp(ACT(dest).regs, ACT(dest).cfa - 0x10);

  TIMER_FG_STOP(unwind_and_size);
}

/*
 * Rewrite an individual variable from the source to destination call frame.
 */
static bool rewrite_var(rewrite_context src, const variable* var_src,
                        rewrite_context dest, const variable* var_dest)
{
  value val_src, fixup_val;
  value_loc val_dest;
  void* stack_addr;
  bool needs_local_fixup = false;
  fixup fixup_data;
  node_t(fixup)* fixup_node;

  ASSERT(var_src && var_dest, "invalid variables\n");

  // TODO hack -- LLVM puts debug information for regs_aarch64 & regs_x86_64 in
  // a different order for the two binaries. We *know* these don't need to be
  // copied, hence we'll ignore them.  Note that this problem goes away if we
  // use -finstrument-functions rather than wrapping individual functions.
  if((var_src->size == 784 && var_dest->size == 624) ||
     (var_src->size == 624 && var_dest->size == 784))
  {
    ST_INFO("Skipping regset_aarch64/regset_x86_64\n");
    return false;
  }

  // TODO hack -- va_list is implemented as a different type for aarch64 &
  // x86-64, and thus has a different size.  Need to handle more gracefully.
  if((var_src->size == 24 && var_dest->size == 32) ||
     (var_src->size == 32 && var_dest->size == 24))
  {
    ST_INFO("Skipping va_list (different size for aarch64/x86-64\n");
    return false;
  }

  ASSERT(var_src->size == var_dest->size,
        "variable has different size (%llu vs. %llu)\n",
        (long long unsigned)var_src->size, (long long unsigned)var_dest->size);
  ASSERT(!(var_src->is_ptr ^ var_dest->is_ptr),
        "variable does not have same type (%d vs. %d)\n",
        var_src->is_ptr, var_dest->is_ptr);

  /* Read variable's source value & perform appropriate action. */
  val_src = get_var_val(src, var_src);
  if(val_src.is_valid)
  {
    /*
     * If variable is a pointer to the stack, record a fixup.  Otherwise, copy
     * the variable into the destination frame.
     */
#if _LIVE_VALS == DWARF_LIVE_VALS
    if(var_src->is_ptr)
#else /* STACKMAP_LIVE_VALS */
    if(!var_src->is_alloca && var_src->is_ptr)
#endif
    {
      if(val_src.is_addr) stack_addr = *(void**)val_src.addr;
      else stack_addr = (void*)val_src.val;

      if(src->stack_base > stack_addr && stack_addr >= src->stack)
      {
        if(src->act > 0 && stack_addr <= PREV_ACT(src).cfa)
          ST_WARN("pointing to variable in called function (%p)\n", stack_addr);
        else
        {
          val_dest = get_var_loc(dest, var_dest);
          ASSERT(val_dest.is_valid, "invalid stack pointer\n");
          fixup_data.src_addr = stack_addr;
          fixup_data.dest_loc = val_dest;
          list_add(fixup, &dest->stack_pointers, dest->act, fixup_data);

          ST_INFO("Adding fixup for stack pointer %p\n", stack_addr);

          /* Are we pointing to a variable within the same frame? */
          if(stack_addr < ACT(src).cfa) needs_local_fixup = true;
        }
      }
      else val_dest = put_var_val(dest, var_dest, val_src);
    }
    else val_dest = put_var_val(dest, var_dest, val_src);

    /* Check if variable is pointed to by other variables & fix up if so. */
    if(val_src.is_addr && val_dest.type == ADDRESS)
    {
      fixup_node = list_begin(fixup, &dest->stack_pointers);
      while(fixup_node)
      {
        if(val_src.addr <= fixup_node->data.src_addr &&
           fixup_node->data.src_addr < val_src.addr + var_src->size)
        {
          ST_INFO("Found fixup for %p (in frame %d)\n",
                  fixup_node->data.src_addr, fixup_node->act);

          fixup_val.is_valid = true;
          fixup_val.is_addr = false;
          fixup_val.val = (uint64_t)(val_dest.addr +
                          (fixup_node->data.src_addr - val_src.addr));
          put_val_loc(dest,
                      fixup_val,
                      dest->handle->ptr_size,
                      fixup_node->data.dest_loc,
                      fixup_node->act);
          fixup_node = list_remove(fixup, &dest->stack_pointers, fixup_node);
        }
        else fixup_node = list_next(fixup, fixup_node);
      }
    }
  }

  return needs_local_fixup;
}

/*
 * Transform an individual frame from the source to destination stack.
 */
static void rewrite_frame(rewrite_context src, rewrite_context dest)
{
  size_t i;
#if _LIVE_VALS == DWARF_LIVE_VALS
  const variable* arg_src, *arg_dest;
#else /* STACKMAP_LIVE_VALS */
  size_t src_offset, dest_offset;
#endif
  const variable* var_src, *var_dest;
  bool needs_local_fixup = false;
  list_t(varval) var_list;
  node_t(varval)* varval_node;
  node_t(fixup)* fixup_node;
  varval varval_data;
  value val_src, val_dest, fixup_val;

  TIMER_START(rewrite_frame);
  ST_INFO("Rewriting frame (CFA: %p -> %p)\n", ACT(src).cfa, ACT(dest).cfa);

#if _LIVE_VALS == DWARF_LIVE_VALS
  ASSERT(num_args(ACT(src).function) == num_args(ACT(dest).function),
        "functions have different numbers of arguments (%lu vs. %lu)\n",
        num_args(ACT(src).function), num_args(ACT(dest).function));
  ASSERT(num_vars(ACT(src).function) == num_vars(ACT(dest).function),
        "functions have different numbers of local variables (%lu vs. %lu)\n",
        num_vars(ACT(src).function), num_vars(ACT(dest).function));

  /* Copy arguments */
  for(i = 0; i < num_args(ACT(src).function); i++)
  {
    arg_src = get_arg_by_pos(ACT(src).function, i);
    arg_dest = get_arg_by_pos(ACT(dest).function, i);
    needs_local_fixup |= rewrite_var(src, arg_src, dest, arg_dest);
  }

  /* Copy variables */
  for(i = 0; i < num_vars(ACT(src).function); i++)
  {
    var_src = get_var_by_pos(ACT(src).function, i);
    var_dest = get_var_by_pos(ACT(dest).function, i);
    needs_local_fixup |= rewrite_var(src, var_src, dest, var_dest);
  }
#else
  ASSERT(ACT(src).site.num_live == ACT(dest).site.num_live,
        "call sites have different numbers of live values (%u vs. %u)\n",
        ACT(src).site.num_live, ACT(dest).site.num_live);

  /* Copy live values */
  src_offset = ACT(src).site.live_offset;
  dest_offset = ACT(dest).site.live_offset;
  for(i = 0; i < ACT(src).site.num_live; i++)
  {
    ASSERT(i < src->handle->live_vals_count,
          "out-of-bounds live value record access\n");

    var_src = &src->handle->live_vals[i + src_offset];
    var_dest = &dest->handle->live_vals[i + dest_offset];
    needs_local_fixup |= rewrite_var(src, var_src, dest, var_dest);
  }
#endif /* STACKMAP_LIVE_VALS */

  /*
   * Fixup pointers to arguments or local variables. This is assumed to *not*
   * be the common case, so we don't save the rewriting metadata from above &
   * must regenerate it here.
   */
  if(needs_local_fixup)
  {
    ST_INFO("Resolving local fix-ups.\n");

    /* Re-generate list of argument & local variable locations. */
    list_init(varval, &var_list);
#if _LIVE_VALS == DWARF_LIVE_VALS
    for(i = 0; i < num_args(ACT(src).function); i++)
    {
      arg_src = get_arg_by_pos(ACT(src).function, i);
      val_src = get_var_val(src, arg_src);
      val_dest = get_var_val(dest, get_arg_by_pos(ACT(dest).function, i));
      if(val_src.is_addr && val_dest.is_addr)
      {
        varval_data.var = arg_src;
        varval_data.val_src = val_src;
        varval_data.val_dest = val_dest;
        varval_node = list_add(varval, &var_list, src->act, varval_data);
      }
    }

    for(i = 0; i < num_vars(ACT(src).function); i++)
    {
      var_src = get_var_by_pos(ACT(src).function, i);
      val_src = get_var_val(src, var_src);
      val_dest = get_var_val(dest, get_var_by_pos(ACT(dest).function, i));
      if(val_src.is_addr && val_dest.is_addr)
      {
        varval_data.var = var_src;
        varval_data.val_src = val_src;
        varval_data.val_dest = val_dest;
        varval_node = list_add(varval, &var_list, src->act, varval_data);
      }
    }
#else /* STACKMAP_LIVE_VALS */
    src_offset = ACT(src).site.live_offset;
    dest_offset = ACT(dest).site.live_offset;
    for(i = 0; i < ACT(src).site.num_live; i++)
    {
      var_src = &src->handle->live_vals[i + src_offset];
      var_dest = &dest->handle->live_vals[i + dest_offset];
      val_src = get_var_val(src, var_src);
      val_dest = get_var_val(dest, var_dest);
      if(val_src.is_addr && val_dest.is_addr)
      {
        varval_data.var = var_src;
        varval_data.val_src = val_src;
        varval_data.val_dest = val_dest;
        varval_node = list_add(varval, &var_list, src->act, varval_data);
      }
    }
#endif /* STACKMAP_LIVE_VALS */

    /* Traverse list to resolve fixups. */
    fixup_node = list_begin(fixup, &dest->stack_pointers);
    while(fixup_node)
    {
      if(fixup_node->data.src_addr <= ACT(src).cfa) // Is fixup in this frame?
      {
        // Note: we should have resolved all fixups for this frame from frames
        // down the call chain by this point.  If not, the fixup may be
        // pointing to garbage data (e.g. uninitialized local variables)
        if(fixup_node->act != src->act)
        {
          ST_WARN("unresolved fixup for '%p' (frame %d)\n",
                  fixup_node->data.src_addr, fixup_node->act);
          continue;
        }

        varval_node = list_begin(varval, &var_list);
        while(varval_node)
        {
          if(varval_node->data.val_src.addr <= fixup_node->data.src_addr &&
             fixup_node->data.src_addr <
               (varval_node->data.val_src.addr + varval_node->data.var->size))
            break;
          varval_node = list_next(varval, varval_node);
        }
        ASSERT(varval_node, "could not resolve same-frame/local fixup (%p in %d)\n",
              fixup_node->data.src_addr, fixup_node->act);

        ST_INFO("Found local fixup for %p\n", fixup_node->data.src_addr);

        fixup_val.is_valid = true;
        fixup_val.is_addr = false;
        fixup_val.val = (uint64_t)(varval_node->data.val_dest.addr +
                        (fixup_node->data.src_addr - varval_node->data.val_src.addr));
        put_val_loc(dest,
                    fixup_val,
                    dest->handle->ptr_size,
                    fixup_node->data.dest_loc,
                    dest->act);
        fixup_node = list_remove(fixup, &dest->stack_pointers, fixup_node);
      }
      else fixup_node = list_next(fixup, fixup_node);
    }

    list_clear(varval, &var_list);
  }

  TIMER_STOP(rewrite_frame);
}

/*
 * Transform the outer-most frame from the source to destination stack.
 */
// Note: we don't copy local variables both as an optimization and as a
// correctness criterion.  The compiler *may* mark local variables as valid for
// all PCs (e.g. if its location doesn't change within a function) but stack
// space hasn't been allocated yet when entering a function.
static void rewrite_frame_outer(rewrite_context src, rewrite_context dest)
{
  size_t i;
#if _LIVE_VALS == STACKMAP_LIVE_VALS
  size_t src_offset, dest_offset;
#endif
  const variable* arg_src, *arg_dest;
  bool needs_local_fixup = false;

  TIMER_START(rewrite_frame);
  ST_INFO("Rewriting frame (CFA: %p -> %p)\n", ACT(src).cfa, ACT(dest).cfa);

#if _LIVE_VALS == DWARF_LIVE_VALS
  ASSERT(num_args(ACT(src).function) == num_args(ACT(dest).function),
        "functions have different numbers of arguments (%lu vs. %lu)\n",
        num_args(ACT(src).function), num_args(ACT(dest).function));

  /* Copy arguments */
  for(i = 0; i < num_args(ACT(src).function); i++)
  {
    arg_src = get_arg_by_pos(ACT(src).function, i);
    arg_dest = get_arg_by_pos(ACT(dest).function, i);
    needs_local_fixup |= rewrite_var(src, arg_src, dest, arg_dest);
  }
#else /* STACKMAP_LIVE_VALS */
  ASSERT(ACT(src).site.num_live == ACT(dest).site.num_live,
        "call sites have different numbers of live values (%lu vs. %lu)\n",
        (long unsigned)ACT(src).site.num_live,
        (long unsigned)ACT(dest).site.num_live);

  /* Copy live values */
  src_offset = ACT(src).site.live_offset;
  dest_offset = ACT(dest).site.live_offset;
  for(i = 0; i < ACT(src).site.num_live; i++)
  {
    ASSERT(i < src->handle->live_vals_count,
          "out-of-bounds live value record access\n");

    arg_src = &src->handle->live_vals[i + src_offset];
    arg_dest = &dest->handle->live_vals[i + dest_offset];
    needs_local_fixup |= rewrite_var(src, arg_src, dest, arg_dest);
  }
#endif

  ASSERT(!needs_local_fixup, "argument cannot point to another argument\n");

  TIMER_STOP(rewrite_frame);
}

