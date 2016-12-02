/*
 * APIs for accessing frame-specific data, i.e. arguments/local variables/live
 * values, return address, and saved frame pointer.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/12/2015
 */

#include "data.h"
#include "unwind.h"

///////////////////////////////////////////////////////////////////////////////
// File-local API
///////////////////////////////////////////////////////////////////////////////

/*
 * Get pointer to the stack save slot or the register in the outer-most
 * activation in which a callee-saved register is saved.
 */
static void* callee_saved_loc(rewrite_context ctx, const value val);

///////////////////////////////////////////////////////////////////////////////
// Data access
///////////////////////////////////////////////////////////////////////////////

/*
 * Evaluate stack map location record VAR and return a value location.
 */
value get_var_val(rewrite_context ctx, const variable* var)
{
  value loc;

  TIMER_FG_START(eval_location);

  loc.is_valid = true;
  loc.act = ctx->act;
  loc.num_bytes = var->size;

  switch(var->type)
  {
  case SM_REGISTER: // Value is in register
    loc.type = REGISTER;
    loc.reg = var->regnum;
    break;
  // Note: these value types are fundamentally different, but their values are
  // generated in an identical manner
  case SM_DIRECT: // Value is allocated on stack
  case SM_INDIRECT: // Value is in register, but spilled to the stack
    loc.type = ADDRESS;
    loc.addr = *(void**)REGOPS(ctx)->reg(ACT(ctx).regs, var->regnum) +
               var->offset_or_constant;
    break;
  case SM_CONSTANT: // Value is constant
    loc.type = CONSTANT;
    loc.cnst = var->offset_or_constant;
    break;
  case SM_CONST_IDX:
    ASSERT(false, "constants in constant pool not supported\n");
    loc.is_valid = false;
    break;
  default:
    ST_ERR(1, "invalid live value location record type (%u)\n", var->type);
    break;
  }

  TIMER_FG_STOP(eval_location);

  return loc;
}

/*
 * Put SRC_VAL from SRC at DEST_VAL from DEST.
 */
void put_val(rewrite_context src,
             const value src_val,
             rewrite_context dest,
             value dest_val,
             uint64_t size)
{
  const void* src_addr;
  void* dest_addr, *callee_addr = NULL;

  ASSERT(src_val.is_valid && dest_val.is_valid, "invalid value(s)\n");

  TIMER_FG_START(put_val);
  ST_INFO("Putting value (size=%lu)\n", size);

  switch(src_val.type)
  {
  case ADDRESS:
    ST_INFO("Source value at %p\n", src_val.addr);
    src_addr = src_val.addr;
    break;
  case REGISTER:
    ST_INFO("Source value in register %u\n", src_val.reg);
    src_addr = REGOPS(src)->reg(src->acts[src_val.act].regs, src_val.reg);
    break;
  case CONSTANT:
    ST_INFO("Source value: %ld / %lu / %lx\n",
            src_val.cnst, src_val.cnst, src_val.cnst);
    src_addr = &src_val.cnst;
    break;
  default:
    ST_ERR(1, "unknown source value location type %u\n", src_val.type);
    break;
  }

  switch(dest_val.type)
  {
  case ADDRESS:
    ST_INFO("Destination value at %p\n", dest_val.addr);
    dest_addr = dest_val.addr;
    break;
  case REGISTER:
    // Note: we're copying callee-saved registers into the current frame's
    // register set & the activation where it is saved (or is still alive).
    // This is cheap & supports both eager & on-demand rewriting.
    ST_INFO("Destination value in register %u\n", dest_val.reg);
    dest_addr = REGOPS(dest)->reg(dest->acts[dest_val.act].regs, dest_val.reg);
    if(dest->handle->props->is_callee_saved(dest_val.reg))
      callee_addr = callee_saved_loc(dest, dest_val);
    break;
  case CONSTANT:
    dest_addr = &dest_val.cnst;
    break;
  default:
    ST_ERR(1, "unknown destination value location type %u\n", dest_val.type);
    break;
  }

  memcpy(dest_addr, src_addr, size);
  if(callee_addr) memcpy(callee_addr, src_addr, size);

  TIMER_FG_STOP(put_val);
}

/*
 * Set return address of current frame in CTX to RETADDR.
 */
void set_return_address(rewrite_context ctx, void* retaddr)
{
  ASSERT(retaddr, "invalid return address\n");
  *(void**)(ACT(ctx).cfa + PROPS(ctx)->ra_offset) = retaddr;
}

/*
 * Set return address of current frame in CTX to RETADDR.
 *
 * This is a special case for setting the address before the function has set
 * up the stack frame, i.e., directly upon function entry.
 */
void set_return_address_funcentry(rewrite_context ctx, void* retaddr)
{
  ASSERT(retaddr, "invalid return address\n");
  if(REGOPS(ctx)->has_ra_reg)
    REGOPS(ctx)->set_ra_reg(ACT(ctx).regs, retaddr);
  else
    *(void**)(ACT(ctx).cfa + PROPS(ctx)->ra_offset) = retaddr;
}

/*
 * Return where in the current frame the caller's frame pointer is saved.
 */
uint64_t* get_savedfbp_loc(rewrite_context ctx)
{
  return (uint64_t*)(ACT(ctx).cfa + PROPS(ctx)->savedfbp_offset);
}

///////////////////////////////////////////////////////////////////////////////
// File-local API (implementation)
///////////////////////////////////////////////////////////////////////////////

static void* callee_saved_loc(rewrite_context ctx, const value val)
{
  int act = val.act;
  value loc;

  ASSERT(val.is_valid, "cannot get callee-saved location for invalid value\n");
  ASSERT(val.type == REGISTER,
         "cannot get callee-saved location for non-register value type\n");

  /* Nothing to propagate from outermost frame */
  if(act <= 0) return NULL;

  /* Walk call chain to check if register has been saved. */
  for(act--; act >= 0; act--)
  {
    if(bitmap_is_set(ctx->acts[act].callee_saved, val.reg))
    {
      loc = get_register_save_loc(ctx, &ctx->acts[act], val.reg);
      ASSERT(loc.type == ADDRESS, "invalid callee-saved slot\n");
      ST_INFO("Saving callee-saved register %u at %p (frame %d)\n",
              val.reg, loc.addr, act);
      return loc.addr;
    }
  }

  /* Register is still live in outermost frame. */
  ST_INFO("Callee-saved register %u live in outer-most frame\n", val.reg);
  return REGOPS(ctx)->reg(ctx->acts[0].regs, val.reg);
}

