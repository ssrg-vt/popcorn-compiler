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
                               const live_value* val,
                               int act);

/*
 * Get the location for a call site value.  Used for the destination call site
 * value (doesn't return addresses for constants).
 */
static void* get_dest_loc(rewrite_context ctx,
                          const live_value* val,
                          int act);

/*
 * Get pointer to the stack save slot or the register in the outer-most
 * activation in which a callee-saved register is saved.
 */
static void* callee_saved_loc(rewrite_context ctx,
                              uint16_t regnum,
                              int act);

/*
 * Given a destination (and possible callee-saved location) apply an insruction
 * to generate an architecture-specific value.
 */
static void apply_arch_operation(rewrite_context ctx,
                                 void* dest,
                                 void* callee_dest,
                                 const arch_live_value* val);

#ifdef _DEBUG
/* Human-readable names for generating architecture-specific values. */
static const char* inst_type_names[] = {
#define X(inst) #inst,
VALUE_GEN_INST
#undef X
};
#endif

///////////////////////////////////////////////////////////////////////////////
// Data access
///////////////////////////////////////////////////////////////////////////////

/*
 * Put SRC_VAL from SRC at DEST_VAL from DEST (copying SIZE bytes).
 */
void put_val(rewrite_context src,
             const live_value* src_val,
             rewrite_context dest,
             const live_value* dest_val)
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
void put_val_arch(rewrite_context ctx, const arch_live_value* val)
{
  void* dest_addr, *callee_addr = NULL;

  TIMER_FG_START(put_val)
  ASSERT(val->type == SM_REGISTER || val->type == SM_INDIRECT,
         "Invalid architecture-specific value type (%u)\n", val->type);

  ST_INFO("Putting arch-specific destination value (size=%u): ", val->size);
  dest_addr = get_val_loc(ctx, val->type, val->regnum, val->offset, ctx->act);
  if(val->type == SM_REGISTER && PROPS(ctx)->is_callee_saved(val->regnum))
    callee_addr = callee_saved_loc(ctx, val->regnum, ctx->act);

  ASSERT(dest_addr, "invalid destination location\n");

  ST_INFO("Arch-specific live value: ");
  apply_arch_operation(ctx, dest_addr, callee_addr, val);

  TIMER_FG_STOP(put_val);
}

/*
 * Set the live value VAL's in activation ACT and context CTX to DATA.
 */
void put_val_data(rewrite_context ctx,
                  const live_value* val,
                  int act,
                  uint64_t data)
{
  void* dest_addr, *callee_addr = NULL;

  TIMER_FG_START(put_val);

  // Avoid the copy if destination value is constant
  if(val->type == SM_CONSTANT || val->type == SM_CONST_IDX)
  {
    ST_INFO("Skipping value (destination value is constant)\n");
    return;
  }

  ST_INFO("Setting data in frame %d: ", act);
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
                      const live_value* val)
{
  void* stack_addr = NULL;

  if(val->is_ptr || val->is_temporary)
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
      // Note: we assume that we're doing offsets from 64-bit registers
      ASSERT(REGOPS(ctx)->reg_size(val->regnum) == 8,
             "invalid register size for pointer\n");
      stack_addr = *(void**)REGOPS(ctx)->reg(ACT(ctx).regs, val->regnum) +
                   val->offset_or_constant;
      // Note 2: temporaries encoded as references to stack slots (i.e., this
      // branch) are by default pointers to the stack.  If it's *not* a
      // temporary but is instead a regular alloca, then we're actually
      // concerned with the value contained *in* the stack slot.
      if(!val->is_temporary) stack_addr = *(void **)stack_addr;
      break;
    case SM_INDIRECT:
      // Note: we assume that we're doing offsets from 64-bit registers
      ASSERT(REGOPS(ctx)->reg_size(val->regnum) == 8,
             "invalid register size for pointer\n");
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
                     const live_value* src_val,
                     const rewrite_context dest,
                     const live_value* dest_val,
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
  if(src_addr <= src_ptr && src_ptr < (src_addr + src_val->alloca_size))
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
  const unwind_loc* locs;
  uint32_t unwind_start, unwind_end, index;
  void* saved_loc;

  locs = ctx->handle->unwind_locs;
  unwind_start = ACT(ctx).site.unwind_offset;
  unwind_end = unwind_start + ACT(ctx).site.num_unwind;

  // Frame pointer is likely at the very end of unwinding records
  for(index = unwind_end - 1; index >= unwind_start; index--)
    if(locs[index].reg == REGOPS(ctx)->fbp_regnum) break;

  ASSERT(index >= unwind_start, "no saved frame base pointer information\n");

  saved_loc = REGOPS(ctx)->fbp(ACT(ctx).regs) + locs[index].offset;
  return (uint64_t*)saved_loc;
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
    ST_RAW_INFO("live value in register %u\n", regnum);
    break;
  // Note: these value types are fundamentally different, but their locations
  // are generated in an identical manner
  case SM_DIRECT: // Value is allocated on stack
  case SM_INDIRECT: // Value is in register, but spilled to the stack
    val_loc = *(void**)REGOPS(ctx)->reg(ctx->acts[act].regs, regnum) +
              offset_or_constant;
    ST_RAW_INFO("live value at stack address %p\n", val_loc);
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
                               const live_value* val,
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
    ST_RAW_INFO("constant live value: %d / %u / %x\n",
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
                          const live_value* val,
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

static void apply_arch_operation(rewrite_context ctx,
                                 void* dest,
                                 void* callee_dest,
                                 const arch_live_value* val)
{
  ASSERT(val->operand_size <= 8,
         "Unhandled arch-specific instruction operand size\n");
  ASSERT(val->size == val->operand_size,
         "Non-matching value sizes (%u vs. %u)\n",
         val->size, val->operand_size);

  if(val->is_gen) /* Generating a value */
  {
    // Note: we limit the types of values that can be generated to unsigned
    // 64-bit integers
    uint64_t *recast = (uint64_t*)dest;
    uint64_t reg, orig = *(uint64_t*)dest;
    int64_t constant;

    switch(val->operand_type)
    {
    case SM_REGISTER: /* Operand is uint64_t */
      ASSERT(REGOPS(ctx)->reg_size(val->operand_regnum) == 8,
             "Invalid register used for value generation\n");
      ST_RAW_INFO("%s register %u\n",
                  inst_type_names[val->inst_type],
                  val->operand_regnum);
      reg = *(uint64_t*)REGOPS(ctx)->reg(ACT(ctx).regs, val->operand_regnum);

      switch(val->inst_type)
      {
      case Set: *recast = reg; break;
      case Add: *recast = orig + reg; break;
      case Subtract: *recast = orig - reg; break;
      case Multiply: *recast = orig * reg; break;
      case Divide: *recast = orig / reg; break;
      case LeftShift: *recast = orig << reg; break;
      case RightShiftLog: *recast = orig >> reg; break;
      case RightShiftArith: *recast = orig >> (int64_t)reg; break;
      case Mask: *recast = orig & reg; break;
      default:
        ST_ERR(1, "Invalid instruction type (%d)\n", val->inst_type);
        break;
      }
      break;
    case SM_CONSTANT: /* Operand is int64_t */
      ST_RAW_INFO("%s constant %ld / %lx\n",
                  inst_type_names[val->inst_type],
                  val->operand_offset_or_constant,
                  val->operand_offset_or_constant);
      constant = val->operand_offset_or_constant;

      switch(val->inst_type)
      {
      case Set: *recast = constant; break;
      case Add: *recast = orig + constant; break;
      case Subtract: *recast = orig - constant; break;
      case Multiply: *recast = orig * constant; break;
      case Divide: *recast = orig / constant; break;
      case LeftShift: *recast = orig << constant; break;
      case RightShiftLog: *recast = orig >> (uint64_t)constant; break;
      case RightShiftArith: *recast = orig >> constant; break;
      case Mask: *recast = orig & constant; break;
      default:
        ST_ERR(1, "Invalid instruction type (%d)\n", val->inst_type);
        break;
      }
      break;
    default:
      ST_ERR(1, "invalid live value location type (%u)\n", val->operand_type);
      break;
    }
  }
  else /* Not generating a value, just use operand type to copy a value */
  {
    void* stack_slot;

    switch(val->operand_type)
    {
    case SM_REGISTER:
      memcpy(dest, REGOPS(ctx)->reg(ACT(ctx).regs, val->operand_regnum),
             val->operand_size);
      ST_RAW_INFO("copy from register %u\n", val->operand_regnum);
      break;
    case SM_DIRECT:
      stack_slot = *(void**)REGOPS(ctx)->reg(ACT(ctx).regs, val->operand_regnum) +
                   val->operand_offset_or_constant;
      memcpy(dest, stack_slot, val->operand_size);
      ST_RAW_INFO("copy from stack slot @ %p\n", stack_slot);
      break;
    case SM_INDIRECT:
      stack_slot = *(void**)REGOPS(ctx)->reg(ACT(ctx).regs, val->operand_regnum) +
                   val->operand_offset_or_constant;
      memcpy(dest, &stack_slot, val->operand_size);
      ST_RAW_INFO("reference to stack slot @ %p\n", stack_slot);
      break;
    case SM_CONSTANT:
      if(val->inst_type == Load64)
      {
        memcpy(dest, (void *)val->operand_offset_or_constant, 8);
        ST_RAW_INFO("load from address 0x%lx\n",
                    val->operand_offset_or_constant);
      }
      else
      {
        memcpy(dest, &val->operand_offset_or_constant, val->operand_size);
        ST_RAW_INFO("constant %ld / %lu / %lx\n",
                    val->operand_offset_or_constant,
                    val->operand_offset_or_constant,
                    val->operand_offset_or_constant);
      }
      break;
    default:
      ST_ERR(1, "invalid live value location type (%u)\n", val->operand_type);
      break;
    }
  }

  if(callee_dest) memcpy(callee_dest, dest, val->operand_size);
}

