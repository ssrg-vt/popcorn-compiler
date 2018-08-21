/*
 * Implements the logic necessary to unwind/un-unwind stack frame activations.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/18/2015
 */

#include "unwind.h"

///////////////////////////////////////////////////////////////////////////////
// File-local API
///////////////////////////////////////////////////////////////////////////////

/*
 * Set up the register set for activation ACT (copies initial registers from
 * ACT - 1).
 */
static inline void setup_regset(rewrite_context ctx, int act)
{
  ASSERT(act > 0, "Cannot set up outermost activation using this function\n");
  ctx->acts[act].regs = &ctx->regset_pool[act * REGOPS(ctx)->regset_size];
  REGOPS(ctx)->regset_clone(ctx->acts[act - 1].regs, ctx->acts[act].regs);
}

/*
 * Set up the callee-saved bitmap for activation ACT.
 */
static inline void setup_callee_saved_bits(rewrite_context ctx, int act)
{
  size_t num_regs;
  num_regs = REGOPS(ctx)->num_regs;
  ctx->acts[act].callee_saved.size = num_regs;
  ctx->acts[act].callee_saved.bits =
    &ctx->callee_saved_pool[act * bitmap_size(num_regs)];
}

/*
 * Restore activation ACT's called-saved registers (saved in activation
 * ACT - 1).
 */
static inline void restore_callee_saved_regs(rewrite_context ctx, int act)
{
  const unwind_loc* locs;
  uint32_t unwind_start, unwind_end, i;
  void* saved_loc;

  ASSERT(act > 0, "Cannot set up outermost activation using this function\n");

  /* Get offsets into unwinding information section & unwind the frame */
  locs = ctx->handle->unwind_locs;
  unwind_start = ctx->acts[act - 1].site.unwind_offset;
  unwind_end = unwind_start + ctx->acts[act - 1].site.num_unwind;

  for(i = unwind_start; i < unwind_end; i++)
  {
    saved_loc = REGOPS(ctx)->fbp(ctx->acts[act - 1].regs) + locs[i].offset;
    ST_INFO("Callee-saved: %u at FBP + %d (%p)\n",
            locs[i].reg, locs[i].offset, saved_loc);
    memcpy(REGOPS(ctx)->reg(ctx->acts[act].regs, locs[i].reg), saved_loc,
           PROPS(ctx)->callee_reg_size(locs[i].reg));
    bitmap_set(ctx->acts[act - 1].callee_saved, locs[i].reg);
  }

  /*
   * Some ABIs map the return address to the PC register (e.g. x86-64) and some
   * map it to another register (e.g. RA is mapped to x30 for AArch64 and LR on
   * PowerPC64).  Handle the latter case by explicitly setting the new PC.
   */
  if(REGOPS(ctx)->has_ra_reg)
    REGOPS(ctx)->set_pc(ctx->acts[act].regs,
                        REGOPS(ctx)->ra_reg(ctx->acts[act].regs));
  ST_INFO("Return address: %p\n", REGOPS(ctx)->pc(ctx->acts[act].regs));
}

/*
 * Set up the frame's bounds, i.e., stack pointer, canonical frame address and
 * frame base pointer.
 *
 *  - stack pointer: by definition the canonical frame address of the previous
 *    frame
 *  - canonical frame address: sum of the new stack pointer and the frame size
 *    from the metadata
 *  - the frame base pointer: architecture-specific formula
 */
static inline void setup_frame_bounds(rewrite_context ctx, int act)
{
  void *new_sp;

  ASSERT(act > 0, "Cannot set up outermost activation using this method\n");
  ASSERT(ctx->acts[act - 1].cfa, "Invalid CFA for frame %d\n", act - 1);
  ASSERT(ctx->acts[act].site.addr, "Invalid call site information\n");

  new_sp = ctx->acts[act - 1].cfa;
  REGOPS(ctx)->set_sp(ctx->acts[act].regs, new_sp);
  ctx->acts[act].cfa = calculate_cfa(ctx, act);
  REGOPS(ctx)->setup_fbp(ctx->acts[act].regs, ctx->acts[act].cfa);

  ASSERT(REGOPS(ctx)->fbp(ctx->acts[act].regs), "Invalid frame pointer\n");

  ST_INFO("New frame bounds: SP=%p, FBP=%p, CFA=%p\n",
          REGOPS(ctx)->sp(ctx->acts[act].regs),
          REGOPS(ctx)->fbp(ctx->acts[act].regs),
          ctx->acts[act].cfa);
}

///////////////////////////////////////////////////////////////////////////////
// Stack unwinding
///////////////////////////////////////////////////////////////////////////////

/*
 * Return whether or not the call site record is for a starting function.
 */
bool first_frame(uint64_t id)
{
  if(id == UINT64_MAX || /* "__libc_start_main()" in __libc_start_main.c */
     id == UINT64_MAX - 1 || /* "start()" in pthread_create.c */
     id == UINT64_MAX - 2 || /* "start_c11()" in pthread_create.c */
     id == UINT64_MAX - 3 || /* "libc_start" in newlib crt0.c */
     id == UINT64_MAX - 4) /* thread_entry in hermitcore's tasks.c */
    return true;
  else return false;
}

/*
 * Calculate the frame's canonical frame address, defined as the stack pointer
 * plus the frame's size.
 */
inline void* calculate_cfa(rewrite_context ctx, int act)
{
  ASSERT(ctx->acts[act].site.addr, "Invalid call site information\n");
  ASSERT(REGOPS(ctx)->sp(ctx->acts[act].regs), "Invalid stack pointer\n");
  return REGOPS(ctx)->sp(ctx->acts[act].regs) + ctx->acts[act].site.frame_size;
}

/*
 * Boot strap the outermost frame's information.  Only needed during
 * initialization as pop_frame performs the same functionality during
 * unwinding.
 */
void bootstrap_first_frame(rewrite_context ctx, void* regset)
{
  ASSERT(ctx->act == 0, "Can only bootstrap outermost frame\n");
  setup_callee_saved_bits(ctx, 0);
  ctx->acts[0].regs = ctx->regset_pool;
  REGOPS(ctx)->regset_copyin(ctx->acts[0].regs, regset);
}

/*
 * Boot strap the outermost frame's information.  This is a special case for
 * setting up information before the function has set up the frame, i.e.,
 * directly upon function entry.  Only needed during initialization as
 * pop_frame_funcentry performs the same functionality during unwinding.
 */
void bootstrap_first_frame_funcentry(rewrite_context ctx, void* sp)
{
  ASSERT(ctx->act == 0, "Can only bootstrap outermost frame\n");
  setup_callee_saved_bits(ctx, 0);
  ctx->acts[0].regs = ctx->regset_pool;
  REGOPS(ctx)->set_sp(ctx->acts[0].regs, sp);
  ctx->acts[0].cfa = sp + PROPS(ctx)->cfa_offset_funcentry;
}

/*
 * Pop a frame from CTX's stack.  Sets up the stack pointer & callee-saved
 * registers.  Sets up the frame base pointer & canonical frame address, if
 * requested.
 */
void pop_frame(rewrite_context ctx, bool setup_bounds)
{
  int next_frame = ctx->act + 1;

  TIMER_FG_START(pop_frame);
  ST_INFO("Popping frame (CFA = %p)\n", ACT(ctx).cfa);

  setup_regset(ctx, next_frame);
  setup_callee_saved_bits(ctx, next_frame);
  restore_callee_saved_regs(ctx, next_frame);

  /*
   * setup_frame_bounds() calculates SP, FBP, & CFA.  Even if we don't want
   * FBP/CFA, we still need to set up SP.
   */
  if(setup_bounds) setup_frame_bounds(ctx, next_frame);
  else REGOPS(ctx)->set_sp(NEXT_ACT(ctx).regs, ACT(ctx).cfa);

  /* Advance to next frame. */
  ctx->act++;
  ASSERT(ctx->act < MAX_FRAMES, "too many frames on stack\n");

  TIMER_FG_STOP(pop_frame);
}

/*
 * Pop a frame from CTX's stack.  This is a special case for popping the frame
 * before the function has set it up, i.e., directly upon function entry.  Sets
 * up the stack pointer, frame base pointer & canonical frame address.
 */
void pop_frame_funcentry(rewrite_context ctx)
{
  int next_frame = ctx->act + 1;

  TIMER_FG_START(pop_frame);
  ST_INFO("Popping frame (CFA = %p)\n", ACT(ctx).cfa);

  setup_regset(ctx, next_frame);
  setup_callee_saved_bits(ctx, next_frame);

  /*
   * We only have to worry about restoring the PC, since the function hasn't
   * set anything else up yet.
   */
  if(REGOPS(ctx)->has_ra_reg)
    REGOPS(ctx)->set_pc(NEXT_ACT(ctx).regs, REGOPS(ctx)->ra_reg(ACT(ctx).regs));
  else
    REGOPS(ctx)->set_pc(NEXT_ACT(ctx).regs,
                        *(void**)REGOPS(ctx)->sp(ACT(ctx).regs));
  ST_INFO("Return address: %p\n", REGOPS(ctx)->pc(NEXT_ACT(ctx).regs));

  setup_frame_bounds(ctx, next_frame);

  /*
   * We also need to set the current frame's FBP to the caller's FBP since it
   * hasn't been stored on the stack yet.
   */
  REGOPS(ctx)->set_fbp(ACT(ctx).regs, REGOPS(ctx)->fbp(NEXT_ACT(ctx).regs));

  /* Advance to next frame. */
  ctx->act++;
  ASSERT(ctx->act < MAX_FRAMES, "too many frames on stack\n");

  TIMER_FG_STOP(pop_frame);
}

/*
 * Process unwinding rule to get the saved location for the register.
 */
void* get_register_save_loc(rewrite_context ctx, activation* act, uint16_t reg)
{
  const unwind_loc* unwind_locs;
  uint32_t unwind_start, unwind_end;
  void* addr = NULL;

  ASSERT(act, "invalid arguments to get_stored_loc()\n");
  ASSERT(bitmap_is_set(act->callee_saved, reg),
         "attempted to find register not saved in specified activation");

  unwind_locs = ctx->handle->unwind_locs;
  unwind_start = act->site.unwind_offset;
  unwind_end = unwind_start + act->site.num_unwind;
  for(uint32_t i = unwind_start; i < unwind_end; i++)
  {
    if(unwind_locs[i].reg == reg)
    {
      addr = REGOPS(ctx)->fbp(act->regs) + unwind_locs[i].offset;
      break;
    }
  }

  return addr;
}

/*
 * Free a stack activation's information.
 */
void clear_activation(st_handle handle, activation* act)
{
  ASSERT(act, "invalid arguments to free_activation()\n");

  memset(&act->site, 0, sizeof(call_site));
  act->cfa = NULL;
  memset(act->regs, 0, handle->regops->regset_size);
  act->regs = NULL;
  memset(&act->callee_saved, 0, sizeof(bitmap));
}

