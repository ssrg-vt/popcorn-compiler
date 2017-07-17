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
#include "arch/powerpc64/util.h"

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
static void init_data_pools(rewrite_context ctx);

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
 * Rewrite an individual value from the source to destination call frame.
 * Returns true if there's a fixup needed within this stack frame.
 */
static bool rewrite_val(rewrite_context src, const live_value* val_src,
                        rewrite_context dest, const live_value* val_dest);

/*
 * Fix up pointers to same-frame data.
 */
static inline void
fixup_local_pointers(rewrite_context src, rewrite_context dest);

/*
 * Re-write an individual frame from the source to destination stack.
 */
static void rewrite_frame(rewrite_context src, rewrite_context dest);

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
  uint64_t* saved_fbp;
  void* fn;

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
  fn = get_function_address(handle_src, REGOPS(src)->pc(ACT(src).regs));
  ASSERT(fn, "Could not find function address of outermost frame\n");
  ST_INFO("Rewriting destination as if entering function @ %p\n", fn);
  dest = init_dest_context(handle_dest, regset_dest, sp_base_dest, fn);

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

  set_return_address_funcentry(dest, (void*)NEXT_ACT(dest).site.addr);
  pop_frame_funcentry(dest);

  /* Rewrite rest of frames. */
  // Note: no need to rewrite libc start function, no state to maintain there
  for(src->act = 1; src->act < src->num_acts - 1; src->act++)
  {
    ST_INFO("--> Rewriting frame %d <--\n", src->act);

    rewrite_frame(src, dest);

    ST_INFO("current frame:%p [st_rewrite_stack]\n", ACT(dest).cfa);
    ST_INFO("fbp: %p [st_rewrite_stack]\n",  REGOPS(dest)->fbp(ACT(dest).regs));
    ST_INFO("sp: %p [st_rewrite_stack]\n",  REGOPS(dest)->sp(ACT(dest).regs));
    ST_INFO("ra_reg: %p [st_rewrite_stack]\n", REGOPS(dest)->ra_reg(ACT(dest).regs));
    ST_INFO("pc: %p [st_rewrite_stack]\n", REGOPS(dest)->pc(ACT(dest).regs));

    if(!get_site_by_id(dest->handle, ACT(src).site.id, &ACT(dest).site))
      ST_ERR(1, "could not get destination call site information (address=%p, ID=%ld)\n",
             REGOPS(src)->pc(ACT(src).regs), ACT(src).site.id);

    ST_INFO("before set_return_address. src->act: %d, dest->act: %d [st_rewrite_stack]\n", src->act, dest->act);
    set_return_address(dest, (void*)NEXT_ACT(dest).site.addr);
    saved_fbp = get_savedfbp_loc(dest);
    ST_INFO("saved_fbp: %p\n", saved_fbp);
    ASSERT(saved_fbp, "invalid saved frame pointer location\n");
    pop_frame(dest, true);
    *saved_fbp = (uint64_t)REGOPS(dest)->fbp(ACT(dest).regs);
    ST_INFO("Old FP saved to %p\n", saved_fbp);
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
#ifndef _PER_LOG_OPEN
  fflush(__log);
#endif
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
  ST_ERR(1, "on-demand rewriting not yet supported\n");

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
  ctx->handle = handle;
  ctx->num_acts = 0;
  ctx->act = 0;
  init_data_pools(ctx);
  list_init(fixup, &ctx->stack_pointers);
  ACT(ctx).regs = ctx->regset_pool;
  REGOPS(ctx)->regset_copyin(ACT(ctx).regs, regset);
  ctx->regs = regset;
  ctx->stack_base = sp_base;
  ctx->stack = REGOPS(ctx)->sp(ACT(ctx).regs);

  // We need to fix PC by skipping NOPs inserted after function
  // calls by the linker
  #if defined(__powerpc64__)
    void* pc = REGOPS(ctx)->pc(ACT(ctx).regs);

    ST_INFO("pc: %p [init_src_context]\n", pc);
    pc = fix_pc(pc);
    ST_INFO("updated pc: %p [init_src_context]\n", pc);
    REGOPS(ctx)->set_pc(ACT(ctx).regs, pc);
  #endif

  if(!get_site_by_addr(handle, REGOPS(ctx)->pc(ACT(ctx).regs), &ACT(ctx).site))
    ST_ERR(1, "could not get source call site information for outermost frame "
           "(address=%p)\n", REGOPS(ctx)->pc(ACT(ctx).regs));
  ASSERT(ctx->stack, "invalid stack pointer\n");

  // Note: *must* call after looking up call site in order to calculate CFA
  bootstrap_first_frame(ctx);

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
  init_data_pools(ctx);
  list_init(fixup, &ctx->stack_pointers);
  ACT(ctx).regs = ctx->regset_pool;
  REGOPS(ctx)->set_pc(ACT(ctx).regs, pc);
  ACT(ctx).site = EMPTY_CALL_SITE;

  ctx->regs = regset;
  ctx->stack_base = sp_base;
  // Note: cannot setup frame information because CFA will be invalid (need to
  // set up SP first)

  ST_INFO("pc: %p [init_dest_context]\n", REGOPS(ctx)->pc(ACT(ctx).regs));
  ST_INFO("sp_base: %p [init_dest_context]\n", ctx->stack_base);
  TIMER_STOP(init_dest_context);
  return ctx;
}

/*
 * Initialize the context's data pools.
 */
static void init_data_pools(rewrite_context ctx)
{
  size_t num_regs = REGOPS(ctx)->num_regs;
  ctx->callee_saved_pool = malloc(bitmap_size(num_regs) * MAX_FRAMES);
  ctx->regset_pool = malloc(REGOPS(ctx)->regset_size * MAX_FRAMES);
  ASSERT(ctx->callee_saved_pool && ctx->regset_pool,
         "could not initialize data pools");
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
            node->data.src_addr, node->data.act);
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
  free(ctx->regset_pool);
}

/////////////////// harubyy ////////////////////
static void traverse_activations(rewrite_context src){
  do
   {
     pop_frame(src, false);
     /*printf("return address: %p\n", REGOPS(src)->pc(ACT(src).regs));
     printf("pc: %p ra_reg:%p\n", REGOPS(src)->pc(ACT(src).regs), REGOPS(src)->ra_reg(ACT(src).regs));
     src->act++;*/
     src->num_acts++;
   } while(!first_frame(ACT(src).site.id));
}
/////////////////// harubyy ////////////////////

/*
 * Unwind source stack to find live frames & size destination stack.
 * Simultaneously caches function & call-site information.
 */
static void unwind_and_size(rewrite_context src,
                            rewrite_context dest)
{
  size_t stack_size = 0;

  TIMER_START(unwind_and_size);

/////////////////// harubyy ////////////////////
//  list_sites_by_addr(src->handle);
//  traverse_activations(src);
//  return;
/////////////////// harubyy ////////////////////

  do
  {
    ST_INFO("[pop_frame]\n");
    pop_frame(src, false);
    src->num_acts++;
    dest->num_acts++;
    dest->act++;

    ST_INFO("fbp: %lx[unwind_and_size]\n", (long)(REGOPS(src)->fbp(ACT(src).regs)));
    ST_INFO("sp: %lx[unwind_and_size]\n", (long)(REGOPS(src)->sp(ACT(src).regs)));
    ST_INFO("pc: %lx[unwind_and_size]\n", (long)(REGOPS(src)->pc(ACT(src).regs)));

    /*
     * Call site meta-data will be used to get return addresses, canonical
     * frame addresses and frame-base pointer locations.
     */
    if(!get_site_by_addr(src->handle, REGOPS(src)->pc(ACT(src).regs), &ACT(src).site))
      ST_ERR(1, "could not get source call site information (address=%p)\n",
             REGOPS(src)->pc(ACT(src).regs));

    if(!get_site_by_id(dest->handle, ACT(src).site.id, &ACT(dest).site))
      ST_ERR(1, "could not get destination call site information (address=%p, ID=%ld)\n",
             REGOPS(src)->pc(ACT(src).regs), ACT(src).site.id);

    /* Update stack size with newly discovered stack frame's size */
    stack_size += ACT(dest).site.frame_size;

    /*
     * Set the CFA for the current frame in order to set the SP when unwinding
     * to the next frame.  However, we can only set the CFA after we've gotten
     * the call site metadata.
     */
    ACT(src).cfa = calculate_cfa(src, src->act);
    
    ST_INFO("frame size: %lu[unwind_and_size]\n", stack_size);
    ST_INFO("fbp: %lx[unwind_and_size]\n", (long)(REGOPS(src)->fbp(ACT(src).regs)));
    ST_INFO("sp: %lx[unwind_and_size]\n", (long)(REGOPS(src)->sp(ACT(src).regs)));
    ST_INFO("pc: %lx[unwind_and_size]\n\n", (long)(REGOPS(src)->pc(ACT(src).regs)));
  }
  while(!first_frame(ACT(src).site.id));


  /* Do one more iteration for starting function */
  pop_frame(src, false);
  src->num_acts++;
  dest->num_acts++;
  dest->act++;

  if(!get_site_by_addr(src->handle, REGOPS(src)->pc(ACT(src).regs), &ACT(src).site))
    ST_ERR(1, "could not get source call site information (address=%p)\n",
           REGOPS(src)->pc(ACT(src).regs));
  if(!get_site_by_id(dest->handle, ACT(src).site.id, &ACT(dest).site))
    ST_ERR(1, "could not get destination call site information (address=%p, ID=%ld)\n",
           REGOPS(src)->pc(ACT(src).regs), ACT(src).site.id);

  stack_size += ACT(dest).site.frame_size;

  ASSERT(stack_size < MAX_STACK_SIZE / 2, "invalid stack size\n");

  ST_INFO("Number of live activations: %d\n", src->num_acts);
  ST_INFO("Destination stack size: %lu\n", stack_size);

  /* Reset to outer-most frame. */
  src->act = 0;
  dest->act = 0;

  /* Set destination stack pointer (align if necessary). */
  dest->stack = dest->stack_base - stack_size;

  ST_INFO("stack before align: %p [unwind_and_size]\n", dest->stack);
  ST_INFO("PROPS(dest)->sp_needs_align:%d [unwind_and_size]\n", PROPS(dest)->sp_needs_align);
  if(PROPS(dest)->sp_needs_align)
    dest->stack = PROPS(dest)->align_sp(dest->stack);
  REGOPS(dest)->set_sp(ACT(dest).regs, dest->stack);

  ST_INFO("Top of new stack: %p\n", dest->stack);

  ST_INFO("(dest->stack)sp after alignment: %lx[unwind_and_size]\n", (long)(REGOPS(dest)->sp(ACT(dest).regs)));

  /* Clear the callee-saved bitmaps for all destination frames. */
  memset(dest->callee_saved_pool, 0, bitmap_size(REGOPS(dest)->num_regs) *
                                     dest->num_acts);

  /* Set up outermost activation for destination since we have a SP. */
  bootstrap_first_frame_funcentry(dest);

  TIMER_STOP(unwind_and_size);
}

/*
 * Rewrite an individual value from the source to destination call frame.
 */
static bool rewrite_val(rewrite_context src, const live_value* val_src,
                        rewrite_context dest, const live_value* val_dest)
{
  bool skip = false, needs_local_fixup = false;
  void* stack_addr;
  fixup fixup_data;
  node_t(fixup)* fixup_node;

  ASSERT(val_src && val_dest, "invalid values\n");

  // TODO hack -- va_list is implemented as a different type for aarch64 &
  // x86-64, and thus has a different size.  Need to handle more gracefully.
  // x86_64:    24
  // aarch64:   32
  // powerpc64:  8 :)
  // TODO: We need to fix this for migration between powerpc & aarch when we're able to migrate between all 3 arhcitectures
  if(val_src->is_alloca && VAL_SIZE(val_src) == 24 &&
     val_dest->is_alloca && VAL_SIZE(val_dest) == 32)
    skip = true;
  else if(val_src->is_alloca && VAL_SIZE(val_src) == 32 &&
          val_dest->is_alloca && VAL_SIZE(val_dest) == 24)
    skip = true;
  else if(val_src->is_alloca && VAL_SIZE(val_src) == 24 &&
          val_dest->is_alloca && VAL_SIZE(val_dest) == 8)
    skip = true;
  else if(val_src->is_alloca && VAL_SIZE(val_src) == 8 &&
          val_dest->is_alloca && VAL_SIZE(val_dest) == 24)
    skip = true;

  if(skip)
  {
    ST_INFO("Skipping va_list (different size for aarch64/x86-64)\n");
    return false;
  }

  ASSERT(VAL_SIZE(val_src) == VAL_SIZE(val_dest),
         "value has different size (%u vs. %u)\n",
         VAL_SIZE(val_src), VAL_SIZE(val_dest));
  ASSERT(!(val_src->is_ptr ^ val_dest->is_ptr),
         "value does not have same type (%s vs. %s)\n",
         (val_src->is_ptr ? "pointer" : "non-pointer"),
         (val_dest->is_ptr ? "pointer" : "non-pointer"));
  ASSERT(!(val_src->is_alloca ^ val_dest->is_alloca),
         "value does not have same type (%s vs. %s)\n",
         (val_src->is_alloca ? "alloca" : "non-alloca"),
         (val_dest->is_alloca ? "alloca" : "non-alloca"));

  /*
   * If value is a pointer to the stack, record a fixup.  Otherwise, copy
   * the value into the destination frame.
   */
  if((stack_addr = points_to_stack(src, val_src)))
  {
    if(src->act == 0 || stack_addr >= PREV_ACT(src).cfa)
    {
      ST_INFO("Adding fixup for pointer-to-stack %p\n", stack_addr);
      fixup_data.src_addr = stack_addr;
      fixup_data.act = dest->act;
      fixup_data.dest_loc = val_dest;
      list_add(fixup, &dest->stack_pointers, fixup_data);

      /* Are we pointing to a value within the same frame? */
      if(stack_addr < ACT(src).cfa) needs_local_fixup = true;
    }
    // Note: it's an error for a pointer to point to frames down the call
    // chain, this is most likely garbage pointer data
    else
      ST_WARN("Pointer-to-stack points to called functions");
  }
  else put_val(src, val_src, dest, val_dest);

  /* Check if value is pointed to by other values & fix up if so. */
  // Note: can only be pointed to if value is in memory, so optimize by
  // filtering out non-allocas
  if(val_src->is_alloca)
  {
    fixup_node = list_begin(fixup, &dest->stack_pointers);
    while(fixup_node)
    {
      if((stack_addr = points_to_data(src, val_src,
                                      dest, val_dest,
                                      fixup_node->data.src_addr)))
      {
        ST_INFO("Found fixup for %p (in frame %d)\n",
                fixup_node->data.src_addr, fixup_node->data.act);

        put_val_data(dest,
                     fixup_node->data.dest_loc,
                     fixup_node->data.act,
                     (uint64_t)stack_addr);
        fixup_node = list_remove(fixup, &dest->stack_pointers, fixup_node);
      }
      else fixup_node = list_next(fixup, fixup_node);
    }
  }

  return needs_local_fixup;
}

/*
 * Fix up pointers to same-frame data.
 */
static inline void
fixup_local_pointers(rewrite_context src, rewrite_context dest)
{
  size_t i, j, src_offset, dest_offset;
  bool found_fixup;
  void* stack_addr;
  const live_value* val_src, *val_dest;
  node_t(fixup)* fixup_node;

  ST_INFO("Resolving local fix-ups\n");

  // Search over all fix-ups
  fixup_node = list_begin(fixup, &dest->stack_pointers);
  while(fixup_node)
  {
    if(fixup_node->data.src_addr <= ACT(src).cfa) // Is fixup in this frame?
    {
      // Note: we should have resolved all fixups for this frame from frames
      // down the call chain by this point.  If not, the fixup may be
      // pointing to garbage data (e.g. uninitialized local values)
      if(fixup_node->data.act != src->act)
      {
        ST_WARN("unresolved fixup for %p (frame %d)\n",
                fixup_node->data.src_addr, fixup_node->data.act);
        fixup_node = list_next(fixup, fixup_node);
        continue;
      }

      // Find the same-frame data which corresponds to the fixup
      found_fixup = false;
      src_offset = ACT(src).site.live_offset;
      dest_offset = ACT(dest).site.live_offset;
      for(i = 0, j = 0; j < ACT(dest).site.num_live; i++, j++)
      {
        val_src = &src->handle->live_vals[i + src_offset];
        val_dest = &dest->handle->live_vals[j + dest_offset];

        ASSERT(!val_src->is_duplicate, "invalid duplicate location record\n");
        ASSERT(!val_dest->is_duplicate, "invalid duplicate location record\n");

        /*
         * Advance past duplicate location records, which can never be
         * pointed-to (these are spilled values, not stack allocations).
         */
        while(src->handle->live_vals[i + 1 + src_offset].is_duplicate) i++;
        while(dest->handle->live_vals[j + 1 + dest_offset].is_duplicate) j++;

        /* Can only have stack pointers to allocas */
        if(!val_src->is_alloca || !val_dest->is_alloca) continue;

        if((stack_addr = points_to_data(src, val_src,
                                        dest, val_dest,
                                        fixup_node->data.src_addr)))
        {
          ST_INFO("Found local fixup for %p\n", fixup_node->data.src_addr);

          put_val_data(dest,
                       fixup_node->data.dest_loc,
                       fixup_node->data.act,
                       (uint64_t)stack_addr);
          fixup_node = list_remove(fixup, &dest->stack_pointers, fixup_node);
          found_fixup = true;
          break;
        }
      }
    }

    if(!found_fixup) fixup_node = list_next(fixup, fixup_node);
  }
}

/*
 * Transform an individual frame from the source to destination stack.
 */
static void rewrite_frame(rewrite_context src, rewrite_context dest)
{
  size_t i, j, src_offset, dest_offset;
  const live_value* val_src, *val_dest;
  bool needs_local_fixup = false;

  TIMER_FG_START(rewrite_frame);
  ST_INFO("Rewriting frame (CFA: %p -> %p)\n", ACT(src).cfa, ACT(dest).cfa);

  /* Copy live values */
  src_offset = ACT(src).site.live_offset;
  dest_offset = ACT(dest).site.live_offset;


  ST_INFO("src_offset: %zu [rewrite_frame]\n", src_offset);
  ST_INFO("dest_offset: %zu [rewrite_frame]\n", dest_offset);

  ST_INFO("num of live values @dest: %d [rewrite_frame]\n", ACT(dest).site.num_live);
  for(i = 0, j = 0; j < ACT(dest).site.num_live; i++, j++)
  {
    ASSERT(i < src->handle->live_vals_count,
           "out-of-bounds live value record access in source handle\n");
    ASSERT(j < dest->handle->live_vals_count,
           "out-of-bounds live value record access in destination handle\n");

    val_src = &src->handle->live_vals[i + src_offset];
    val_dest = &dest->handle->live_vals[j + dest_offset];

    ASSERT(!val_src->is_duplicate, "invalid duplicate location record\n");
    ASSERT(!val_dest->is_duplicate, "invalid duplicate location record\n");

    /* Apply to first location record */
    needs_local_fixup |= rewrite_val(src, val_src, dest, val_dest);

    ST_INFO("before handling duplicates [rewrite_frame]\n");

    /* Apply to all duplicate location records */
    while(dest->handle->live_vals[j + 1 + dest_offset].is_duplicate)
    {
      j++;
      val_dest = &dest->handle->live_vals[j + dest_offset];
      ASSERT(!val_dest->is_alloca, "invalid duplicate location record\n");
      ST_INFO("Applying to duplicate location record\n");
      needs_local_fixup |= rewrite_val(src, val_src, dest, val_dest);
    }

    /* Advance source value past duplicates location records */
    while(src->handle->live_vals[i + 1 + src_offset].is_duplicate) i++;
  }

  ST_INFO("after handling all live values [rewrite_frame]\n");

  ASSERT(i == ACT(src).site.num_live && j == ACT(dest).site.num_live,
        "did not handle all live values\n");

  // TODO: We need to handle this for PowerPC either
  #if !defined(__powerpc64__)
    /* Set architecture-specific live values */
    dest_offset = ACT(dest).site.arch_live_offset;
    for(i = 0; i < ACT(dest).site.num_arch_live; i++)
      put_val_arch(dest, &dest->handle->arch_live_vals[i + dest_offset]);
    ST_INFO("after handling architecture live values [rewrite_frame]\n");
  #endif

  /* Fix up pointers to arguments or local values */
  if(needs_local_fixup) fixup_local_pointers(src, dest);

  ST_INFO("after fixup_local_pointers  [rewrite_frame]\n");

  TIMER_FG_STOP(rewrite_frame);
}

