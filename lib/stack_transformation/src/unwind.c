/*
 * Implements the logic necessary to unwind/un-unwind stack frame activations.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/18/2015
 */

#include "unwind.h"

///////////////////////////////////////////////////////////////////////////////
// Stack unwinding
///////////////////////////////////////////////////////////////////////////////

/*
 * Set up the frame's information.
 */
void setup_frame_info(rewrite_context ctx)
{
  size_t num_regs;

  num_regs = REGOPS(ctx)->num_regs;
  ACT(ctx).callee_saved.size = num_regs;
  ACT(ctx).callee_saved.bits =
    &ctx->callee_saved_pool[ctx->act * bitmap_size(num_regs)];
  ACT(ctx).cfa = REGOPS(ctx)->fbp(ACT(ctx).regs) + PROPS(ctx)->cfa_offset;
}

/*
 * Set up the frame context as if we just entered the current function.
 */
void setup_frame_info_funcentry(rewrite_context ctx)
{
  size_t num_regs;

  num_regs = REGOPS(ctx)->num_regs;
  ACT(ctx).callee_saved.size = num_regs;
  ACT(ctx).callee_saved.bits =
    &ctx->callee_saved_pool[ctx->act * bitmap_size(num_regs)];
  ACT(ctx).cfa = REGOPS(ctx)->sp(ACT(ctx).regs) +
                 PROPS(ctx)->cfa_offset_funcentry;
}

/*
 * Return whether or not the call site record is for a starting function.
 */
bool first_frame(uint64_t id)
{
  if(id == UINT64_MAX || id == UINT64_MAX - 1) return true;
  else return false;
}

/*
 * Pop a frame from CTX's stack.
 */
void pop_frame(rewrite_context ctx)
{
  const unwind_loc* locs;
  uint32_t unwind_start, unwind_end;
  void* saved_loc;

  TIMER_FG_START(pop_frame);

  ST_INFO("Popping frame (CFA = %p)\n", ACT(ctx).cfa);

  /* Initialize next activation's regset */
  NEXT_ACT(ctx).regs = REGOPS(ctx)->regset_clone(ACT(ctx).regs);

  /* Get offsets into unwinding information section & unwind the frame */
  locs = ctx->handle->unwind_locs;
  unwind_start = ACT(ctx).site.unwind_offset;
  unwind_end = unwind_start + ACT(ctx).site.num_unwind;
  for(uint32_t i = unwind_start; i < unwind_end; i++)
  {
    saved_loc = REGOPS(ctx)->fbp(ACT(ctx).regs) + locs[i].offset;
    ST_INFO("Callee-saved: %u at FBP + %d (%p)\n",
            locs[i].reg, locs[i].offset, saved_loc);
    memcpy(REGOPS(ctx)->reg(NEXT_ACT(ctx).regs, locs[i].reg), saved_loc,
           PROPS(ctx)->callee_reg_size(locs[i].reg));
    bitmap_set(ACT(ctx).callee_saved, locs[i].reg);
  }

  /*
   * Some ABIs map the return address to the PC register (e.g. x86-64) and some
   * map it to another register (e.g. RA is mapped to x30 for AArch64).  Handle
   * the latter case by explicitly setting the new PC.
   */
  if(REGOPS(ctx)->has_ra_reg)
    REGOPS(ctx)->set_pc(NEXT_ACT(ctx).regs,
                        REGOPS(ctx)->ra_reg(NEXT_ACT(ctx).regs));
  ST_INFO("Return address: %p\n", REGOPS(ctx)->pc(NEXT_ACT(ctx).regs));

  /*
   * Set the stack pointer in the previous frame which is by definition the CFA
   * of the current frame.
   */
  REGOPS(ctx)->set_sp(NEXT_ACT(ctx).regs, ACT(ctx).cfa);

  /* Advance to next frame. */
  ctx->act++;
  ASSERT(ctx->act < MAX_FRAMES, "too many frames on stack\n");

  TIMER_FG_STOP(pop_frame);
}

/*
 * Pop a frame from CTX's stack.  This is a special case for popping the frame
 * before the function has set it up, i.e., directly upon function entry.
 */
void pop_frame_funcentry(rewrite_context ctx)
{
  TIMER_FG_START(pop_frame);
  ST_INFO("Popping frame (CFA = %p)\n", ACT(ctx).cfa);

  /* Initialize next activation's regset */
  NEXT_ACT(ctx).regs = REGOPS(ctx)->regset_clone(ACT(ctx).regs);

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

  /*
   * Set the stack pointer in the previous frame which is by definition the CFA
   * of the current frame.
   */
  REGOPS(ctx)->set_sp(NEXT_ACT(ctx).regs, ACT(ctx).cfa);

  /* Advance to next frame. */
  ctx->act++;
  ASSERT(ctx->act < MAX_FRAMES, "too many frames on stack\n");

  TIMER_FG_STOP(pop_frame);
}

/*
 * Process unwinding rule to get the saved location for the register.
 */
value get_register_save_loc(rewrite_context ctx, activation* act, uint16_t reg)
{
  const unwind_loc* unwind_locs;
  uint32_t unwind_start, unwind_end;
  value loc = {
    .is_valid = false,
    .act = ctx->act,
    .type = ADDRESS,
  };

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
      loc.is_valid = true;
      loc.addr = REGOPS(ctx)->fbp(act->regs) + unwind_locs[i].offset;
      break;
    }
  }

  return loc;
}

/*
 * Free a stack activation's information.
 */
void free_activation(st_handle handle, activation* act)
{
  ASSERT(act, "invalid arguments to free_activation()\n");

  act->regs->free(act->regs);
#ifdef _CHECKS
  memset(&act->site, 0, sizeof(call_site));
  act->cfa = NULL;
  act->regs = NULL;
  memset(&act->callee_saved, 0, sizeof(bitmap));
#endif
}

