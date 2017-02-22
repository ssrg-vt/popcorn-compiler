/*
 * APIs for accessing frame-specific data, i.e. live values, return address,
 * and saved frame pointer.
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
 * Get a pointer to a value's location.  Returns the memory address needed to
 * read/write a register or the value's location in memory.
 */
static inline void* get_val_loc(rewrite_context ctx,
                                uint8_t type,
                                uint16_t regnum,
                                int32_t offset_or_constant,
                                int act);

/*
 * Get the location for a call site value.  Used for the source call site
 * values, and will return addresses for constants.
 */
static const void* get_src_loc(rewrite_context ctx,
                               const call_site_value* val,
                               int act);

/*
 * Get the location for a call site value.  Used for the destination call site
 * value (doesn't return addresses for constants).
 */
static void* get_dest_loc(rewrite_context ctx,
                          const call_site_value* val,
                          int act);

/*
 * Get pointer to the stack save slot or the register in the outer-most
 * activation in which a callee-saved register is saved.
 */
static void* callee_saved_loc(rewrite_context ctx,
                              uint16_t regnum,
                              int act);

///////////////////////////////////////////////////////////////////////////////
// Data access
///////////////////////////////////////////////////////////////////////////////

/*
 * Put SRC_VAL from SRC at DEST_VAL from DEST (copying SIZE bytes).
 */
void put_val(rewrite_context src,
             const call_site_value* src_val,
             rewrite_context dest,
             const call_site_value* dest_val)
{
  const void* src_addr;
  void* dest_addr, *callee_addr = NULL;

  TIMER_FG_START(put_val);
  ASSERT(src->act == dest->act, "non-matching activations (%u vs. %u)\n",
         src->act, dest->act);

  // Avoid the copy if destination value is constant
  if(dest_val->type == SM_CONSTANT || dest_val->type == SM_CONST_IDX)
  {
    ST_INFO("Skipping value (destination value is constant)\n");
    return;
  }

  ASSERT(VAL_SIZE(src_val) == VAL_SIZE(dest_val),
         "value sizes don't match (%u vs. %u)\n",
         VAL_SIZE(src_val), VAL_SIZE(dest_val));

  ST_INFO("Getting source value: ");
  src_addr = get_src_loc(src, src_val, src->act);
  ST_INFO("Putting destination value (size=%u): ", VAL_SIZE(dest_val));
  dest_addr = get_dest_loc(dest, dest_val, src->act);

  // Note: we're copying callee-saved registers into the current frame's
  // register set & the activation where it is saved (or is still alive).
  // This is cheap & supports both eager & on-demand rewriting.
  if(dest_val->type == SM_REGISTER &&
     PROPS(dest)->is_callee_saved(dest_val->regnum))
    callee_addr = callee_saved_loc(dest, dest_val->regnum, dest->act);

  ASSERT(dest_addr, "invalid destination location\n");
  memcpy(dest_addr, src_addr, VAL_SIZE(dest_val));
  if(callee_addr) memcpy(callee_addr, src_addr, VAL_SIZE(dest_val));

  TIMER_FG_STOP(put_val);
}

/*
 * Evaluate architecture-specific location record VAL and set the appropriate
 * value in CTX.
 */
void put_val_arch(rewrite_context ctx, const arch_const_value* val)
{
  void* dest_addr, *callee_addr = NULL;

  TIMER_FG_START(put_val)
  ASSERT(val->type == SM_REGISTER || val->type == SM_DIRECT,
         "Invalid architecture-specific value type (%u)\n", val->type);

  ST_INFO("Putting architecture-specific destination value\n");
  dest_addr = get_val_loc(ctx, val->type, val->regnum, val->offset, ctx->act);
  if(val->type == SM_REGISTER && PROPS(ctx)->is_callee_saved(val->regnum))
    callee_addr = callee_saved_loc(ctx, val->regnum, ctx->act);

  ASSERT(dest_addr, "invalid destination location\n");
  memcpy(dest_addr, &val->value, val->size);
  if(callee_addr) memcpy(callee_addr, &val->value, val->size);

  TIMER_FG_STOP(put_val);
}

/*
 * Set the live value VAL's in activation ACT and context CTX to DATA.
 */
void put_val_data(rewrite_context ctx,
                  const call_site_value* val,
                  int act,
                  uint64_t data)
{
  void* dest_addr, *callee_addr = NULL;

  TIMER_FG_START(put_val);

  ST_INFO("Setting data: ");
  dest_addr = get_dest_loc(ctx, val, act);
  if(val->type == SM_REGISTER && PROPS(ctx)->is_callee_saved(val->regnum))
    callee_addr = callee_saved_loc(ctx, val->regnum, act);

  ASSERT(dest_addr, "invalid destination location\n");
  memcpy(dest_addr, &data, sizeof(uint64_t));
  if(callee_addr) memcpy(callee_addr, &data, sizeof(data));

  TIMER_FG_STOP(put_val);
}

/*
 * Return whether or not a pointer refers to a value on the stack.
 */
void* points_to_stack(const rewrite_context ctx,
                      const call_site_value* val)
{
  void* stack_addr = NULL;

  /* Is it a pointer (NOT an alloca/stack value)? */
  if(val->is_ptr && !val->is_alloca)
  {
    /* Get the pointed-to address */
    switch(val->type)
    {
    case SM_REGISTER:
      // Note: we assume that we're doing offsets from 64-bit registers
      ASSERT(REGOPS(ctx)->reg_size(val->regnum) == 8,
             "invalid register size for pointer\n");

      stack_addr = *(void**)REGOPS(ctx)->reg(ACT(ctx).regs, val->regnum);
      break;
    case SM_DIRECT:
      // Note: SM_DIRECT live values are allocated to the stack (allocas) and
      // thus should have been weeded out in the if-statement above
      ST_ERR(1, "incorrectly encoded live value\n");
      break;
    case SM_INDIRECT:
      stack_addr = *(void**)REGOPS(ctx)->reg(ACT(ctx).regs, val->regnum) +
                   val->offset_or_constant;
      stack_addr = *(void**)stack_addr;
      break;
    case SM_CONSTANT:
      ST_ERR(1, "directly-encoded constants too small to store pointers\n");
      break;
    case SM_CONST_IDX:
      ST_ERR(1, "constant pool entries not supported\n");
      break;
    default:
      ST_ERR(1, "invalid value type (%d)", val->type);
      break;
    }

    /* Check if we're within the stack's bounds.  If not, wipe the pointer */
    if(stack_addr < ctx->stack || ctx->stack_base <= stack_addr)
      stack_addr = NULL;
  }

  return stack_addr;
}

/*
 * Return whether or not a pointer refers to the specified live value.
 */
void* points_to_data(const rewrite_context src,
                     const call_site_value* src_val,
                     const rewrite_context dest,
                     const call_site_value* dest_val,
                     void* src_ptr)
{
  void* src_addr, *dest_addr = NULL;

  ASSERT(src_val->type == SM_DIRECT && dest_val->type == SM_DIRECT,
         "invalid value types (must be allocas for pointed-to analysis)\n");

  ST_INFO("Checking if %p points to: ", src_ptr);
  src_addr = get_val_loc(src, src_val->type,
                         src_val->regnum,
                         src_val->offset_or_constant,
                         src->act);
  if(src_addr <= src_ptr && src_ptr < (src_addr + VAL_SIZE(src_val)))
  {
    ST_INFO("Reifying address of source value %p to: ", src_addr);
    dest_addr = get_val_loc(dest, dest_val->type,
                            dest_val->regnum,
                            dest_val->offset_or_constant,
                            dest->act);
    dest_addr += (src_ptr - src_addr);
  }

  return dest_addr;
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

static inline void* get_val_loc(rewrite_context ctx,
                                uint8_t type,
                                uint16_t regnum,
                                int32_t offset_or_constant,
                                int act)
{
  void* val_loc = NULL;

  switch(type)
  {
  case SM_REGISTER: // Value is in register
    val_loc = REGOPS(ctx)->reg(ctx->acts[act].regs, regnum);
    ST_RAW_INFO("Live value in register %u\n", regnum);
    break;
  // Note: these value types are fundamentally different, but their locations
  // are generated in an identical manner
  case SM_DIRECT: // Value is allocated on stack
  case SM_INDIRECT: // Value is in register, but spilled to the stack
    val_loc = *(void**)REGOPS(ctx)->reg(ctx->acts[act].regs, regnum) +
              offset_or_constant;
    ST_RAW_INFO("Live value at %p\n", val_loc);
    break;
  case SM_CONSTANT: // Value is constant
  case SM_CONST_IDX: // Value is in constant pool
    ST_ERR(1, "cannot get location for constant/constant index\n");
    break;
  default:
    ST_ERR(1, "invalid live value location type (%u)\n", type);
    break;
  }

  return val_loc;
}

static const void* get_src_loc(rewrite_context ctx,
                               const call_site_value* val,
                               int act)
{
  switch(val->type)
  {
  case SM_REGISTER: case SM_DIRECT: case SM_INDIRECT:
    return get_val_loc(ctx,
                       val->type,
                       val->regnum,
                       val->offset_or_constant,
                       act);
  case SM_CONSTANT:
    ST_RAW_INFO("Constant live value: %d / %u / %x\n",
            val->offset_or_constant,
            val->offset_or_constant,
            val->offset_or_constant);
    return &val->offset_or_constant;
  case SM_CONST_IDX:
    ST_ERR(1, "constant pool entries not supported\n");
    break;
  default:
    ST_ERR(1, "invalid live value location type (%u)\n", val->type);
    break;
  }
  return NULL;
}

static void* get_dest_loc(rewrite_context ctx,
                          const call_site_value* val,
                          int act)
{
  return get_val_loc(ctx,
                     val->type,
                     val->regnum,
                     val->offset_or_constant,
                     act);
}

static void* callee_saved_loc(rewrite_context ctx,
                              uint16_t regnum,
                              int act)
{
  void* saved_addr;

  /* Nothing to propagate from outermost frame */
  if(act <= 0) return NULL;

  /* Walk call chain to check if register has been saved. */
  for(act--; act >= 0; act--)
  {
    if(bitmap_is_set(ctx->acts[act].callee_saved, regnum))
    {
      saved_addr = get_register_save_loc(ctx, &ctx->acts[act], regnum);
      ASSERT(saved_addr, "invalid callee-saved slot\n");
      ST_INFO("Saving callee-saved register %u at %p (frame %d)\n",
              regnum, saved_addr, act);
      return saved_addr;
    }
  }

  /* Register is still live in outermost frame. */
  ST_INFO("Callee-saved register %u live in outer-most frame\n", regnum);
  return REGOPS(ctx)->reg(ctx->acts[0].regs, regnum);
}

