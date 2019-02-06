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

#include "arch_regs.h"

#define REGSET_POOL (MAX_REGSET_SIZE * MAX_FRAMES)
#define CALLEE_POOL (MAX_CALLEE_SIZE * MAX_FRAMES)

/*
 * Declare all rewriting space at compile time to avoid malloc whenever
 * possible.  We only need to declare a pair of each as each thread will only
 * ever use 2 at a time.
 */
static __thread struct rewrite_context src_ctx, dest_ctx;
static __thread char src_regs[REGSET_POOL], dest_regs[REGSET_POOL];
static __thread char src_callee[CALLEE_POOL], dest_callee[CALLEE_POOL];

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
 * stack SP_BASE.  Store destination REGSET pointer to be filled with
 * destination thread's resultant register state.
 */
static rewrite_context init_dest_context(st_handle handle,
                                         void* regset,
                                         void* sp_base);

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

int st_rewrite_randomized(void* cham_handle,
                          get_rand_info info_func,
                          st_handle handle,
                          void* regset_src,
                          void* sp_src_base,
                          void* sp_src_buf,
                          void* regset_dst,
                          void* sp_dest_base,
                          void* sp_dest_buf) {
  rewrite_context src, dst;
  uint64_t* saved_fbp;

  if(!cham_handle || !info_func || !handle || !regset_src || !sp_src_base ||
     !sp_src_buf || !regset_dst || !sp_dest_base || !sp_dest_buf)
  {
    ST_WARN("invalid arguments\n");
    return 1;
  }

  TIMER_START(st_rewrite_stack);

  ST_INFO("--> Initializing randomized rewrite (%s) <--\n",
          arch_name(handle->arch));

  /* Initialize rewriting contexts. */
  src = init_src_context(handle, regset_src, sp_src_base);
  dst = init_dest_context(handle, regset_dst, sp_dest_base);
  src->buf = sp_src_buf;
  dst->buf = sp_dest_buf;
  src->cham_handle = dst->cham_handle = cham_handle;
  src->rand_info = dst->rand_info = info_func;

  if(!src || !dst)
  {
    if(src) free_context(src);
    if(dst) free_context(dst);
    return 1;
  }

  ST_INFO("--> Unwinding source stack to find live activations <--\n");

  /* Unwind source stack to determine destination stack size. */
  unwind_and_size(src, dst);

  // Note: the following code is brittle -- it has to happen in this *exact*
  // order because of the way the stack is unwound and information in the
  // current & surrounding frames is accessed.  Modify with care!

  ST_INFO("--> Rewriting from source to destination stack <--\n");

  TIMER_START(rewrite_stack);

  /* Rewrite outer-most frame. */
  ST_INFO("--> Rewriting outermost frame <--\n");

  set_return_address_funcentry(dst, (void*)NEXT_ACT(dst).site.addr);
  pop_frame_funcentry(dst, true);

  /* Rewrite rest of frames. */
  for(src->act = 1; src->act < src->num_acts - 1; src->act++)
  {
    ST_INFO("--> Rewriting frame %d <--\n", src->act);

    set_return_address(dst, (void*)NEXT_ACT(dst).site.addr);
    rewrite_frame(src, dst);
    saved_fbp = translate_stack_address(dst, dst->act, get_savedfbp_loc(dst));
    ASSERT(saved_fbp, "invalid saved frame pointer location\n");
    pop_frame(dst, true);
    *saved_fbp = (uint64_t)REGOPS(dst)->fbp(ACT(dst).regs);
    ST_INFO("Old FP saved to %p\n", saved_fbp);
  }

  // Note: there may be a few things to fix up in the innermost function, e.g.,
  // the TOC pointer on PowerPC
  ST_INFO("--> Rewriting frame %d (starting function) <--\n", src->act);
  rewrite_frame(src, dst);

  TIMER_STOP(rewrite_stack);

  /* Copy out register state for destination & clean up. */
  REGOPS(dst)->regset_copyout(dst->acts[0].regs, dst->regs);
  free_context(dst);
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
  src = init_src_context(handle_src, regset_src, sp_base_src);
  dest = init_dest_context(handle_dest, regset_dest, sp_base_dest);

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
  pop_frame_funcentry(dest, true);

  /* Rewrite rest of frames. */
  for(src->act = 1; src->act < src->num_acts - 1; src->act++)
  {
    ST_INFO("--> Rewriting frame %d <--\n", src->act);

    set_return_address(dest, (void*)NEXT_ACT(dest).site.addr);
    rewrite_frame(src, dest);
    saved_fbp = get_savedfbp_loc(dest);
    ASSERT(saved_fbp, "invalid saved frame pointer location\n");
    pop_frame(dest, true);
    *saved_fbp = (uint64_t)REGOPS(dest)->fbp(ACT(dest).regs);
    ST_INFO("Old FP saved to %p\n", saved_fbp);
  }

  // Note: there may be a few things to fix up in the innermost function, e.g.,
  // the TOC pointer on PowerPC
  ST_INFO("--> Rewriting frame %d (starting function) <--\n", src->act);
  rewrite_frame(src, dest);

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
  ctx->regset_pool = src_regs;
  ctx->callee_saved_pool = src_callee;
#else
  ctx = (rewrite_context)MALLOC(sizeof(struct rewrite_context));
#endif
  ctx->handle = handle;
  ctx->num_acts = 1;
  ctx->act = 0;
  ctx->regs = regset;
  ctx->stack_base = sp_base;

#if _TLS_IMPL != COMPILER_TLS
  init_data_pools(ctx);
#endif
  list_init(fixup, &ctx->stack_pointers);
  bootstrap_first_frame(ctx, regset); // Sets up initial register set
  ctx->stack = REGOPS(ctx)->sp(ACT(ctx).regs);
  ASSERT(ctx->stack, "invalid stack pointer\n");

#ifndef CHAMELEON
  /*
   * Find the initial call site and set up the outermost frame's CFA in
   * preparation for unwinding the stack.
   */
  // Note: we need both the SP & call site information to set up CFA
  if(!get_site_by_addr(handle, REGOPS(ctx)->pc(ACT(ctx).regs), &ACT(ctx).site))
    ST_ERR(1, "could not get source call site information for outermost frame "
           "(address=%p)\n", REGOPS(ctx)->pc(ACT(ctx).regs));
  ACT(ctx).cfa = calculate_cfa(ctx, 0);
#else
  // Chameleon sets up the source stack to be at either a function entry or
  // exit, meaning it looks like we entered the outermost function
  ACT(ctx).cfa = ctx->stack + PROPS(ctx)->cfa_offset_funcentry;
  ACT(ctx).site.id = 0; // Make sure we don't accidentally trigger early exit
  ACT(ctx).nslots = 0;
#endif

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
                                         void* sp_base)
{
  rewrite_context ctx;

  TIMER_START(init_dest_context);

#if _TLS_IMPL == COMPILER_TLS
  ctx = &dest_ctx;
  ctx->regset_pool = dest_regs;
  ctx->callee_saved_pool = dest_callee;
#else
  ctx = (rewrite_context)MALLOC(sizeof(struct rewrite_context));
#endif
  ctx->handle = handle;
  ctx->num_acts = 1;
  ctx->act = 0;
  ctx->regs = regset;
  ctx->stack_base = sp_base;

#if _TLS_IMPL != COMPILER_TLS
  init_data_pools(ctx);
#endif
  list_init(fixup, &ctx->stack_pointers);

  // Note: cannot setup frame information because CFA will be invalid, need to
  // set up SP & find call site information

  TIMER_STOP(init_dest_context);
  return ctx;
}

/*
 * Initialize the context's data pools.
 */
static void init_data_pools(rewrite_context ctx)
{
  size_t num_regs = REGOPS(ctx)->num_regs;
  ctx->regset_pool = MALLOC(REGOPS(ctx)->regset_size * MAX_FRAMES);
  ctx->callee_saved_pool = MALLOC(bitmap_size(num_regs) * MAX_FRAMES);
  ASSERT(ctx->callee_saved_pool && ctx->regset_pool,
         "could not initialize data pools");
}

/*
 * Free an architecture-specific context.
 */
static void free_context(rewrite_context ctx)
{
  node_t(fixup)* node;

  TIMER_START(free_context);

  node = list_begin(fixup, &ctx->stack_pointers);
  while(node)
  {
    ST_WARN("could not find stack pointer fixup for %p (in activation %d)\n",
            node->data.src_addr, node->data.act);
    node = list_remove(fixup, &ctx->stack_pointers, node);
  }

#ifdef _CHECKS
  int i;
  for(i = 0; i < ctx->num_acts; i++)
    clear_activation(ctx->handle, &ctx->acts[i]);
#endif
#if _TLS_IMPL != COMPILER_TLS
  free_data_pools(ctx);
  free(ctx);
#endif

  TIMER_STOP(free_context);
}

/*
 * Free a rewrite context's data pools.
 */
static void free_data_pools(rewrite_context ctx)
{
  free(ctx->regset_pool);
  free(ctx->callee_saved_pool);
#ifdef _DEBUG
  ctx->regset_pool = NULL;
  ctx->callee_saved_pool = NULL;
#endif
}

/*
 * Unwind source stack to find live frames & size destination stack.
 * Simultaneously caches function & call-site information.
 */
static void unwind_and_size(rewrite_context src,
                            rewrite_context dest)
{
  size_t stack_size = 8; // Account for possible already-pushed return address
  void* fn;

  TIMER_START(unwind_and_size);

#ifndef CHAMELEON
  do
  {
    pop_frame(src, false);
    src->num_acts++;
    dest->num_acts++;
    dest->act++;

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
    stack_size += CUR_FUNC(dest).frame_size;

    /* Set the CFA for the current frame, which becomes the next frame's SP */
    // Note: we need both the SP & call site information to set up CFA
    ACT(src).cfa = calculate_cfa(src, src->act);
  }
  while(!first_frame(ACT(src).site.id));
#else
  func_rand_info rand_info;

  pop_frame_funcentry(src, false);
  src->num_acts++;
  dest->num_acts++;
  dest->act++;

  /*
   * Call site meta-data will be used to get return addresses, canonical
   * frame addresses and frame-base pointer locations.
   */
  if(!get_site_by_addr(src->handle, REGOPS(src)->pc(ACT(src).regs), &ACT(src).site))
    ST_ERR(1, "could not get source call site information (address=%p)\n",
           REGOPS(src)->pc(ACT(src).regs));
  ACT(dest).site = ACT(src).site;
  rand_info = src->rand_info(src->cham_handle, ACT(src).site.addr);

  ACT(src).frame_size = rand_info.old_frame_size;
  ACT(src).nslots = rand_info.num_old_slots;
  ACT(src).slots = rand_info.old_rand_slots;

  ACT(dest).frame_size = rand_info.new_frame_size;
  ACT(dest).nslots = rand_info.num_new_slots;
  ACT(dest).slots = rand_info.new_rand_slots;
  stack_size += rand_info.new_frame_size;

  /* Set the CFA for the current frame, which becomes the next frame's SP */
  // Note: we need both the SP & call site information to set up CFA
  ACT(src).cfa = calculate_cfa(src, src->act);

  while(!first_frame(ACT(src).site.id))
  {
    pop_frame(src, false);
    src->num_acts++;
    dest->num_acts++;
    dest->act++;

    if(!get_site_by_addr(src->handle, REGOPS(src)->pc(ACT(src).regs), &ACT(src).site))
      ST_ERR(1, "could not get source call site information (address=%p)\n",
             REGOPS(src)->pc(ACT(src).regs));
    ACT(dest).site = ACT(src).site;
    rand_info = src->rand_info(src->cham_handle, ACT(src).site.addr);

    ACT(src).frame_size = rand_info.old_frame_size;
    ACT(src).nslots = rand_info.num_old_slots;
    ACT(src).slots = rand_info.old_rand_slots;

    ACT(dest).frame_size = rand_info.new_frame_size;
    ACT(dest).nslots = rand_info.num_new_slots;
    ACT(dest).slots = rand_info.new_rand_slots;
    stack_size += rand_info.new_frame_size;
    ACT(src).cfa = calculate_cfa(src, src->act);
  }

  // Account for other stuff above the stack, e.g., TLS, environment variables
  stack_size += src->stack_base - REGOPS(src)->sp(ACT(src).regs);
#endif /* CHAMELEON */

  ASSERT(stack_size < MAX_STACK_SIZE / 2, "invalid stack size\n");

  ST_INFO("Number of live activations: %d\n", src->num_acts);
  ST_INFO("Destination stack size: %lu\n", stack_size);

  /* Reset to outer-most frame. */
  src->act = 0;
  dest->act = 0;

  /* Set destination stack pointer and finish setting up outermost frame */
  dest->stack = PROPS(dest)->align_sp(dest->stack_base - stack_size);
  bootstrap_first_frame_funcentry(dest, dest->stack);
#ifndef CHAMELEON
  fn = get_function_address(src->handle, REGOPS(src)->pc(ACT(src).regs));
  ASSERT(fn, "Could not find function address of outermost frame\n");
  REGOPS(dest)->set_pc(ACT(dest).regs, fn);
#else
  fn = REGOPS(src)->pc(ACT(src).regs);
  REGOPS(dest)->set_pc(ACT(dest).regs, fn);
#endif

  ST_INFO("Top of new stack: %p\n", dest->stack);
  ST_INFO("Rewriting destination as if entering function @ %p\n", fn);

  /* Clear the callee-saved bitmaps for all destination frames. */
  memset(dest->callee_saved_pool, 0, bitmap_size(REGOPS(dest)->num_regs) *
                                     dest->num_acts);

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

  if(val_dest->is_temporary)
  {
    ST_INFO("Skipping temporary value\n");
    return false;
  }

  // TODO hack -- va_list is implemented with different sizes for different
  // architectures.  Need to handle more gracefully.
  //   x86_64:    24
  //   aarch64:   32
  //   powerpc64:  8
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
  ASSERT(!(val_src->is_alloca ^ val_dest->is_alloca) || val_src->is_temporary,
         "value does not have same type (%s vs. %s)\n",
         (val_src->is_alloca ? "alloca" : "non-alloca"),
         (val_dest->is_alloca ? "alloca" : "non-alloca"));

  /*
   * If value is a pointer to the stack, record a fixup.  Otherwise, copy
   * the value into the destination frame.
   */
  if((stack_addr = points_to_stack(src, val_src)))
  {
    if(stack_addr >= PREV_ACT(src).cfa || src->act == 0)
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
    // chain, this is most likely uninitialized pointer data
    else
      ST_WARN("Pointer-to-stack points to called functions\n");
  }
  else put_val(src, val_src, dest, val_dest);

  /* Check if value is pointed to by other values & fix up if so. */
  // Note: can only be pointed to if value is in memory, i.e., allocas
  if(val_src->is_alloca && !val_src->is_temporary)
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

#ifdef CHAMELEON
        stack_addr = randomized_address(dest, dest->act, stack_addr);
#endif
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
    // TODO If the code creates a pointer to an argument, is LLVM forced to
    // create an alloca and copy the argument into the local stack space?
    // Otherwise, how does LLVM understand argument/register conventions?
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
      src_offset = ACT(src).site.live.offset;
      dest_offset = ACT(dest).site.live.offset;
      for(i = 0, j = 0; j < ACT(dest).site.live.num; i++, j++)
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

#ifdef CHAMELEON
          stack_addr = randomized_address(dest, dest->act, stack_addr);
#endif
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
  src_offset = ACT(src).site.live.offset;
  dest_offset = ACT(dest).site.live.offset;
  for(i = 0, j = 0; j < ACT(dest).site.live.num; i++, j++)
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

    /* Apply to all duplicate location records */
    while((j + 1 + dest_offset) < dest->handle->live_vals_count &&
          dest->handle->live_vals[j + 1 + dest_offset].is_duplicate)
    {
      j++;
      val_dest = &dest->handle->live_vals[j + dest_offset];
      ASSERT(!val_dest->is_alloca, "invalid duplicate location record\n");
      ST_INFO("Applying to duplicate location record\n");
      needs_local_fixup |= rewrite_val(src, val_src, dest, val_dest);
    }

    /* Advance source value past duplicates location records */
    while((i + 1 + src_offset) < src->handle->live_vals_count &&
          src->handle->live_vals[i + 1 + src_offset].is_duplicate) i++;
  }
  ASSERT(i == ACT(src).site.live.num && j == ACT(dest).site.live.num,
        "did not handle all live values\n");

  /* Set architecture-specific live values */
  dest_offset = ACT(dest).site.arch_live.offset;
  for(i = 0; i < ACT(dest).site.arch_live.num; i++)
    put_val_arch(dest, &dest->handle->arch_live_vals[i + dest_offset]);

  /* Fix up pointers to local values */
  if(needs_local_fixup) fixup_local_pointers(src, dest);

  TIMER_FG_STOP(rewrite_frame);
}

