/*
 * Implements the main rewriting logic for stack transformation.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/12/2015
 */

#include "stack_transform.h"
#include "data.h"
#include "unwind.h"
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

// TODO needed?
/*
 * Re-write the outer-most frame, which doesn't need to copy over local
 * variables.
 */
//static void rewrite_frame_outer(rewrite_context src, rewrite_context dest);

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
  dest = init_dest_context(handle_dest, regset_dest, sp_base_dest,
                           REGOPS(src)->pc(ACT(src).regs));

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

  TIMER_START(rewrite_stack);

  /* Rewrite outer-most frame. */
  ST_INFO("--> Rewriting outermost frame <--\n");

  // TODO do we need to rewrite the outer frame?  Arguments to migration shim
  // are stored in the pthreads library
  //rewrite_frame_outer(src, dest);
  set_return_address_funcentry(dest, (void*)NEXT_ACT(dest).site.addr);
  pop_frame_funcentry(dest);
  fbp = REGOPS(dest)->sp(ACT(dest).regs) + ACT(dest).site.fbp_offset;
  ASSERT(fbp, "invalid frame pointer\n");
  REGOPS(dest)->set_fbp(ACT(dest).regs, fbp);
  REGOPS(dest)->set_fbp(PREV_ACT(dest).regs, fbp);
  setup_frame_info(dest);
  ST_INFO("Set FP=%p for outer-most frame\n", fbp);

  /* Rewrite rest of frames. */
  // Note: no need to rewrite libc start function, no state to maintain there
  for(src->act = 1; src->act < src->num_acts - 1; src->act++)
  {
    ST_INFO("--> Rewriting frame %d <--\n", src->act);

    rewrite_frame(src, dest);
    set_return_address(dest, (void*)NEXT_ACT(dest).site.addr);
    saved_fbp = get_savedfbp_loc(dest);
    ASSERT(saved_fbp, "invalid saved frame pointer location\n");
    pop_frame(dest);
    fbp = REGOPS(dest)->sp(ACT(dest).regs) + ACT(dest).site.fbp_offset;
    ASSERT(fbp, "invalid frame pointer\n");
    REGOPS(dest)->set_fbp(ACT(dest).regs, fbp);
    *saved_fbp = (uint64_t)fbp;
    setup_frame_info(dest);
    ST_INFO("Saved old FP=%p to %p\n", fbp, saved_fbp);
  }

  TIMER_STOP(rewrite_stack);

  /* Copy out register state for destination & clean up. */
  REGOPS(dest)->regset_copyout(dest->acts[0].regs, dest->regs);
  free_context(dest);
  free_context(src);

  ST_INFO("Finished rewrite!\n");

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
  unwind_addr meta;

  TIMER_START(init_src_context);

#if _TLS_IMPL == COMPILER_TLS
  ctx = &src_ctx;
#else
  ctx = (rewrite_context)malloc(sizeof(struct rewrite_context));
#endif
  ctx->handle = handle;
  ctx->num_acts = 0;
  ctx->act = 0;
  list_init(fixup, &ctx->stack_pointers);
  ACT(ctx).regs = REGOPS(ctx)->regset_init(regset);
  ctx->regs = regset;
  ctx->stack_base = sp_base;
  ctx->stack = REGOPS(ctx)->sp(ACT(ctx).regs);
  init_data_pools(ctx, REGOPS(ctx)->num_regs);
  setup_frame_info(ctx);

  if(!get_site_by_addr(handle, REGOPS(ctx)->pc(ACT(ctx).regs), &ACT(ctx).site))
  {
    ACT(ctx).site = EMPTY_CALL_SITE;
    ST_INFO("No source call site information @ %p, searching for function\n",
           REGOPS(ctx)->pc(ACT(ctx).regs));

    if(!get_unwind_offset_by_addr(handle,
                                  REGOPS(ctx)->pc(ACT(ctx).regs),
                                  &meta))
      ST_ERR(1, "unable to find unwinding information for outermost frame\n");

    ACT(ctx).site.num_unwind = meta.num_unwind;
    ACT(ctx).site.unwind_offset = meta.unwind_offset;
  }

  ASSERT(ctx->stack, "invalid stack pointer\n");

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
  ctx->handle = handle;
  ctx->num_acts = 0;
  ctx->act = 0;
  list_init(fixup, &ctx->stack_pointers);
  ACT(ctx).regs = REGOPS(ctx)->regset_default();
  REGOPS(ctx)->set_pc(ACT(ctx).regs, pc);
  ACT(ctx).site = EMPTY_CALL_SITE;

  ctx->regs = regset;
  ctx->stack_base = sp_base;
  init_data_pools(ctx, ACT(ctx).regs->num_regs);
  // Note: cannot setup frame information because CFA will be invalid (need to
  // set up SP first)

  TIMER_STOP(init_dest_context);
  return ctx;
}

/*
 * Initialize the context's data pools.
 */
static void init_data_pools(rewrite_context ctx, size_t num_regs)
{
  ctx->callee_saved_pool = malloc(bitmap_size(num_regs) * MAX_FRAMES);
  ASSERT(ctx->callee_saved_pool, "could not initialize data pools\n");
}

/*
 * Free an architecture-specific context.
 */
static void free_context(rewrite_context ctx)
{
  int i;
  node_t(fixup)* node;

  TIMER_START(free_context);

  node = list_begin(fixup, &ctx->stack_pointers);
  while(node)
  {
    ST_WARN("could not find stack pointer fixup for %p (in activation %d)\n",
            node->data.src_addr, node->data.dest_loc.act);
    node = list_remove(fixup, &ctx->stack_pointers, node);
  }

  for(i = 0; i < ctx->num_acts; i++)
    free_activation(ctx->handle, &ctx->acts[i]);
  free_data_pools(ctx);
#if _TLS_IMPL != COMPILER_TLS
  free(ctx);
#endif

  TIMER_STOP(free_context);
}

/*
 * Free a rewrite context's data pools.
 */
static void free_data_pools(rewrite_context ctx)
{
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

  TIMER_START(unwind_and_size);

  do
  {
    pop_frame(src);
    setup_frame_info(src);
    src->num_acts++;
    dest->num_acts++;
    dest->act++;

    /*
     * Call site meta-data will be used to get return addresses & frame-base
     * pointer locations.
     */
    if(!get_site_by_addr(src->handle, REGOPS(src)->pc(ACT(src).regs), &ACT(src).site))
      ST_ERR(1, "could not get source call site information (address=%p)\n",
             REGOPS(src)->pc(ACT(src).regs));
    if(!get_site_by_id(dest->handle, ACT(src).site.id, &ACT(dest).site))
      ST_ERR(1, "could not get destination call site information (address=%p, ID=%ld)\n",
             REGOPS(src)->pc(ACT(src).regs), ACT(src).site.id);

    /* Update stack size with newly discovered stack frame's size */
    // Note: this might overestimate the size for frames without a base pointer
    // but that shouldn't be a problem.
    stack_size += ACT(dest).site.fbp_offset +
                  (2 * dest->handle->ptr_size); // old FBP & RA
  }
  while(!first_frame(ACT(src).site.id));

  /* Do one more iteration for starting function */
  pop_frame(src);
  setup_frame_info(src);
  src->num_acts++;
  dest->num_acts++;
  dest->act++;
  if(!get_site_by_addr(src->handle, REGOPS(src)->pc(ACT(src).regs), &ACT(src).site))
    ST_ERR(1, "could not get source call site information (address=%p)\n",
           REGOPS(src)->pc(ACT(src).regs));
  if(!get_site_by_id(dest->handle, ACT(src).site.id, &ACT(dest).site))
    ST_ERR(1, "could not get destination call site information (address=%p, ID=%ld)\n",
           REGOPS(src)->pc(ACT(src).regs), ACT(src).site.id);
  stack_size += ACT(dest).site.fbp_offset + (2 * dest->handle->ptr_size);

  ASSERT(stack_size < MAX_STACK_SIZE / 2, "invalid stack size\n");

  ST_INFO("Number of live activations: %d\n", src->num_acts);
  ST_INFO("Destination stack size: %lu\n", stack_size);

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

  /* Set up outermost activation for destination since we have a SP. */
  setup_frame_info_funcentry(dest);

  /*
   * The compiler may specify arguments are located at an offset from the frame
   * pointer at all function PCs, including the ones where the frame hasn't
   * been set up.  Hard-code outer frame's FBP for this situation.
   */
  // TODO this is wrong, probably...
  /*REGOPS(dest)->set_fbp(ACT(dest).regs,
                        ACT(dest).cfa - fp_offset(dest->handle->arch));*/

  TIMER_STOP(unwind_and_size);
}

/*
 * Rewrite an individual variable from the source to destination call frame.
 */
static bool rewrite_var(rewrite_context src, const variable* var_src,
                        rewrite_context dest, const variable* var_dest)
{
  value val_src, val_dest, fixup_val;
  bool skip = false, needs_fixup = false, needs_local_fixup = false;

  ASSERT(var_src && var_dest, "invalid variables\n");

  // TODO hack -- va_list is implemented as a different type for aarch64 &
  // x86-64, and thus has a different size.  Need to handle more gracefully.
  if(var_src->is_alloca && var_src->pointed_size == 24 &&
     var_dest->is_alloca && var_dest->pointed_size == 32)
    skip = true;
  else if(var_src->is_alloca && var_src->pointed_size == 32 &&
          var_dest->is_alloca && var_dest->pointed_size == 24)
    skip = true;
  if(skip)
  {
    ST_INFO("Skipping va_list (different size for aarch64/x86-64)\n");
    return false;
  }

  ASSERT(VAR_SIZE(var_src) == VAR_SIZE(var_dest),
        "variable has different size (%llu vs. %llu)\n",
        (long long unsigned)VAR_SIZE(var_src),
        (long long unsigned)VAR_SIZE(var_dest));
  ASSERT(!(var_src->is_ptr ^ var_dest->is_ptr),
        "variable does not have same type (%s vs. %s)\n",
        (var_src->is_ptr ? "pointer" : "non-pointer"),
        (var_dest->is_ptr ? "pointer" : "non-pointer"));
  ASSERT(!(var_src->is_alloca ^ var_dest->is_alloca),
        "variable does not have same type (%s vs. %s)\n",
        (var_src->is_alloca ? "alloca" : "non-alloca"),
        (var_dest->is_alloca ? "alloca" : "non-alloca"));

  /* Read variable's source value & perform appropriate action. */
  val_src = get_var_val(src, var_src);
  val_dest = get_var_val(dest, var_dest);
  if(val_src.is_valid && val_dest.is_valid)
  {
    fixup fixup_data;
    node_t(fixup)* fixup_node;
    void* stack_addr;

    /*
     * If variable is a pointer to the stack, record a fixup.  Otherwise, copy
     * the variable into the destination frame.
     */
    if(!var_src->is_alloca && var_src->is_ptr)
    {
      /* Read the pointer's value */
      switch(val_src.type)
      {
      case ADDRESS: stack_addr = *(void**)val_src.addr; break;
      case REGISTER:
        // Note: we assume that we're doing offsets from 64-bit registers 
        ASSERT(REGOPS(src)->reg_size(val_src.reg) == 8,
               "invalid register size for pointer\n");
        stack_addr = *(void**)REGOPS(src)->reg(ACT(src).regs, val_src.reg);
        break;
      case CONSTANT: stack_addr = (void*)val_src.cnst; break;
      }

      /* Check if it points to a value on the stack */
      if(src->stack_base > stack_addr && stack_addr >= src->stack)
      {
        if(src->act > 0 && stack_addr <= PREV_ACT(src).cfa)
        {
          // Note: it is an error for a pointer to point to frames down the
          // call chain.  This is probably something like uninitialized data,
          // so we'll let it slide.
          ST_WARN("pointing to variable in called function (%p)\n", stack_addr);
          skip = true;
        }
        else
        {
          ST_INFO("Adding fixup for stack pointer %p\n", stack_addr);
          needs_fixup = true;
          fixup_data.src_addr = stack_addr;
          fixup_data.dest_loc = val_dest;
          list_add(fixup, &dest->stack_pointers, fixup_data);

          /* Are we pointing to a variable within the same frame? */
          if(stack_addr < ACT(src).cfa) needs_local_fixup = true;
        }
      }
    }

    /* If we don't point to the stack, do the copy. */
    if(!skip && !needs_fixup)
      put_val(src, val_src, dest, val_dest, VAR_SIZE(var_src));

    /* Check if variable is pointed to by other variables & fix up if so. */
    // Note: can only be pointed to if value is in memory, so optimize by
    // filtering out illegal types
    if(val_src.type == ADDRESS && val_dest.type == ADDRESS)
    {
      fixup_node = list_begin(fixup, &dest->stack_pointers);
      while(fixup_node)
      {
        if(val_src.addr <= fixup_node->data.src_addr &&
           fixup_node->data.src_addr < val_src.addr + VAR_SIZE(var_src))
        {
          ST_INFO("Found fixup for %p (in frame %d)\n",
                  fixup_node->data.src_addr, fixup_node->data.dest_loc.act);

          fixup_val.is_valid = true;
          fixup_val.type = CONSTANT;
          fixup_val.cnst = (uint64_t)(val_dest.addr +
                           (fixup_node->data.src_addr - val_src.addr));
          put_val(src,
                  fixup_val,
                  dest,
                  fixup_node->data.dest_loc,
                  dest->handle->ptr_size);
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
  size_t i, j;
  const variable* var_src, *var_dest;
  size_t src_offset, dest_offset;
  bool needs_local_fixup = false;
  list_t(varval) var_list;
  node_t(varval)* varval_node;
  node_t(fixup)* fixup_node;
  varval varval_data;
  value val_src, val_dest, fixup_val;

  TIMER_FG_START(rewrite_frame);
  ST_INFO("Rewriting frame (CFA: %p -> %p)\n", ACT(src).cfa, ACT(dest).cfa);

  /* Copy live values */
  src_offset = ACT(src).site.live_offset;
  dest_offset = ACT(dest).site.live_offset;
  for(i = 0, j = 0;
      i < ACT(src).site.num_live && j < ACT(dest).site.num_live;
      i++, j++)
  {
    ASSERT(i < src->handle->live_vals_count,
          "out-of-bounds live value record access in source handle\n");
    ASSERT(j < dest->handle->live_vals_count,
          "out-of-bounds live value record access in destination handle\n");

    var_src = &src->handle->live_vals[i + src_offset];
    var_dest = &dest->handle->live_vals[j + dest_offset];

    ASSERT(!var_src->is_duplicate, "invalid backing location record\n");
    ASSERT(!var_dest->is_duplicate, "invalid backing location record\n");

    /* Apply to first location record */
    needs_local_fixup |= rewrite_var(src, var_src, dest, var_dest);

    /* Apply to all duplicate/backing location records */
    while(dest->handle->live_vals[j + 1 + dest_offset].is_duplicate)
    {
      j++;
      var_dest = &dest->handle->live_vals[j + dest_offset];
      ASSERT(!var_dest->is_alloca, "invalid backing location record\n");
      ST_INFO("Applying to duplicate location record\n");
      needs_local_fixup |= rewrite_var(src, var_src, dest, var_dest);
    }

    /* Advance source variable past duplicates/backing location records */
    while(src->handle->live_vals[i + 1 + src_offset].is_duplicate) i++;
  }
  ASSERT(i == ACT(src).site.num_live && j == ACT(dest).site.num_live,
        "did not handle all live values\n");

  /*
   * Fix up pointers to arguments or local variables. This is assumed to *not*
   * be the common case, so we don't save the rewriting metadata from above &
   * must regenerate it here.
   */
  if(needs_local_fixup)
  {
    ST_INFO("Resolving local fix-ups.\n");

    /* Re-generate list of argument & local variable locations. */
    list_init(varval, &var_list);
    src_offset = ACT(src).site.live_offset;
    dest_offset = ACT(dest).site.live_offset;
    for(i = 0, j = 0;
        i < ACT(src).site.num_live && j < ACT(dest).site.num_live;
        i++, j++)
    {
      var_src = &src->handle->live_vals[i + src_offset];
      var_dest = &dest->handle->live_vals[j + dest_offset];

      ASSERT(!var_src->is_duplicate, "invalid backing location record\n");
      ASSERT(!var_dest->is_duplicate, "invalid backing location record\n");

      /*
       * Advance past duplicate/backing location records, which can never be
       * pointed-to (these are spilled values, not stack allocations).
       */
      while(src->handle->live_vals[i + 1 + src_offset].is_duplicate) i++;
      while(dest->handle->live_vals[j + 1 + dest_offset].is_duplicate) j++;

      /* Can only have stack pointers to allocas */
      if(!var_src->is_alloca || !var_dest->is_alloca) continue;

      val_src = get_var_val(src, var_src);
      val_dest = get_var_val(dest, var_dest);
      if(val_src.type == ADDRESS && val_dest.type == ADDRESS)
      {
        varval_data.var = var_src;
        varval_data.val_src = val_src;
        varval_data.val_dest = val_dest;
        list_add(varval, &var_list, varval_data);
      }
    }
    ASSERT(i == ACT(src).site.num_live && j == ACT(dest).site.num_live,
          "did not handle all live values\n");

    /* Traverse list to resolve fixups. */
    fixup_node = list_begin(fixup, &dest->stack_pointers);
    while(fixup_node)
    {
      if(fixup_node->data.src_addr <= ACT(src).cfa) // Is fixup in this frame?
      {
        // Note: we should have resolved all fixups for this frame from frames
        // down the call chain by this point.  If not, the fixup may be
        // pointing to garbage data (e.g. uninitialized local variables)
        if(fixup_node->data.dest_loc.act != src->act)
        {
          ST_WARN("unresolved fixup for '%p' (frame %d)\n",
                  fixup_node->data.src_addr, fixup_node->data.dest_loc.act);
          fixup_node = list_next(fixup, fixup_node);
          continue;
        }

        varval_node = list_begin(varval, &var_list);
        while(varval_node)
        {
          if(varval_node->data.val_src.addr <= fixup_node->data.src_addr &&
             fixup_node->data.src_addr <
               (varval_node->data.val_src.addr + VAR_SIZE(varval_node->data.var)))
            break;
          varval_node = list_next(varval, varval_node);
        }
        ASSERT(varval_node, "could not resolve same-frame/local fixup (%p in %d)\n",
              fixup_node->data.src_addr, fixup_node->data.dest_loc.act);

        ST_INFO("Found local fixup for %p\n", fixup_node->data.src_addr);

        fixup_val.is_valid = true;
        fixup_val.type = CONSTANT;
        fixup_val.cnst = (uint64_t)(varval_node->data.val_dest.addr +
                         (fixup_node->data.src_addr - varval_node->data.val_src.addr));
        put_val(src,
                fixup_val,
                dest,
                fixup_node->data.dest_loc,
                dest->handle->ptr_size);

        fixup_node = list_remove(fixup, &dest->stack_pointers, fixup_node);
      }
      else fixup_node = list_next(fixup, fixup_node);
    }

    list_clear(varval, &var_list);
  }

  TIMER_FG_STOP(rewrite_frame);
}

// TODO needed?
/*
 * Transform the outer-most frame from the source to destination stack.
 */
// Note: we don't copy local variables both as an optimization and as a
// correctness criterion.  The compiler *may* mark local variables as valid for
// all PCs (e.g. if its location doesn't change within a function) but stack
// space hasn't been allocated yet when entering a function.
/*static void rewrite_frame_outer(rewrite_context src, rewrite_context dest)
{
  size_t i;
  const variable* arg_src, *arg_dest;
  size_t src_offset, dest_offset;
  bool needs_local_fixup = false;

  TIMER_FG_START(rewrite_frame);
  ST_INFO("Rewriting frame (CFA: %p -> %p)\n", ACT(src).cfa, ACT(dest).cfa);

  ASSERT(ACT(src).site.num_live == ACT(dest).site.num_live,
        "call sites have different numbers of live values (%lu vs. %lu)\n",
        (long unsigned)ACT(src).site.num_live,
        (long unsigned)ACT(dest).site.num_live);*/

  /* Copy live values */
/*  src_offset = ACT(src).site.live_offset;
  dest_offset = ACT(dest).site.live_offset;
  for(i = 0; i < ACT(src).site.num_live; i++)
  {
    ASSERT(i < src->handle->live_vals_count,
          "out-of-bounds live value record access\n");

    arg_src = &src->handle->live_vals[i + src_offset];
    arg_dest = &dest->handle->live_vals[i + dest_offset];
    needs_local_fixup |= rewrite_var(src, arg_src, dest, arg_dest);
  }

  ASSERT(!needs_local_fixup, "argument cannot point to another argument\n");

  TIMER_FG_STOP(rewrite_frame);
}*/

