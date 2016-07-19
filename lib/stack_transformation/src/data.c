/*
 * APIs for accessing frame-specific data, i.e. arguments/local variables/live
 * values, return address, and saved frame pointer location.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/12/2015
 */

#include "data.h"
#include "unwind.h"
#include "func.h"
#include "query.h"
#include "util.h"

///////////////////////////////////////////////////////////////////////////////
// DWARF stack handling
///////////////////////////////////////////////////////////////////////////////

/*
 * The DWARF standard describes a fairly extensive expression evaluation
 * procedure based on an abstract stack machine, implemented below.
 */

/* Stack machine state */
typedef struct stack
{
  Dwarf_Unsigned data[EXPR_STACK_SIZE];
  int top;
} stack;

/* Initialize a stack */
inline static void stack_init(stack* stack)
{
  stack->top = -1;
}

/* Macros for generating boilerplate code for different stack operations. */
#define STACK_UN_OP( NAME, TYPE, OP ) \
inline static Dwarf_##TYPE stack_##NAME(stack* st) \
{ \
  Dwarf_##TYPE data = (Dwarf_##TYPE)st->data[st->top]; \
  st->data[st->top] = OP data; \
  return st->data[st->top]; \
}

#define STACK_UN_CALL( NAME, TYPE, FUNC ) \
inline static Dwarf_##TYPE stack_##NAME(stack* st) \
{ \
  Dwarf_##TYPE data = (Dwarf_##TYPE)st->data[st->top]; \
  st->data[st->top] = FUNC (data); \
  return st->data[st->top]; \
}

#define STACK_BIN_OP( NAME, TYPE, OP ) \
inline static Dwarf_##TYPE stack_##NAME(stack* st) \
{ \
  Dwarf_##TYPE data1 = (Dwarf_##TYPE)st->data[st->top]; \
  Dwarf_##TYPE data2 = (Dwarf_##TYPE)st->data[st->top - 1]; \
  st->data[--st->top] = data2 OP data1; \
  return st->data[st->top]; \
}

/* Stack movement operations */

inline static Dwarf_Unsigned stack_push(stack* st, Dwarf_Unsigned data)
{
  st->data[++st->top] = data;
  return data;
}

inline static Dwarf_Unsigned stack_dup(stack* st)
{
  Dwarf_Unsigned data = st->data[st->top];
  st->data[++st->top] = data;
  return data;
}

inline static Dwarf_Unsigned stack_drop(stack* st)
{
  return st->data[st->top--];
}

inline static Dwarf_Unsigned stack_pick(stack* st, Dwarf_Unsigned idx)
{
  Dwarf_Unsigned data = st->data[st->top - idx];
  st->data[++st->top] = data;
  return data;
}

inline static Dwarf_Unsigned stack_over(stack* st)
{
  Dwarf_Unsigned data = st->data[st->top - 1];
  st->data[++st->top] = data;
  return data;
}

inline static Dwarf_Unsigned stack_swap(stack* st)
{
  Dwarf_Unsigned data = st->data[st->top];
  st->data[st->top] = st->data[st->top - 1];
  st->data[st->top - 1] = data;
  return st->data[st->top];
}

inline static Dwarf_Unsigned stack_rot(stack* st)
{
  Dwarf_Unsigned data = st->data[st->top];
  st->data[st->top] = st->data[st->top - 1];
  st->data[st->top - 1] = st->data[st->top - 2];
  st->data[st->top - 2] = data;
  return st->data[st->top];
}

/* Arithmetic and logical operations */
STACK_UN_CALL( abs, Signed, llabs )
STACK_BIN_OP( and, Unsigned, & )
STACK_BIN_OP( div, Signed, / )
STACK_BIN_OP( minus, Unsigned, - )
STACK_BIN_OP( mod, Unsigned, % )
STACK_BIN_OP( mul, Unsigned, * )
STACK_UN_OP( neg, Signed, - )
STACK_UN_OP( not, Unsigned, ! )
STACK_BIN_OP( or, Unsigned, | )
STACK_BIN_OP( plus, Unsigned, + )
STACK_BIN_OP( shl, Unsigned, << )
STACK_BIN_OP( shr, Unsigned, >> )
STACK_BIN_OP( shra, Signed, >> )
STACK_BIN_OP( xor, Unsigned, ^ )
STACK_BIN_OP( le, Signed, <= )
STACK_BIN_OP( ge, Signed, >= )
STACK_BIN_OP( eq, Signed, == )
STACK_BIN_OP( lt, Signed, < )
STACK_BIN_OP( gt, Signed, > )
STACK_BIN_OP( ne, Signed, != )

///////////////////////////////////////////////////////////////////////////////
// File-local API
///////////////////////////////////////////////////////////////////////////////

/* Extract a value from the location described by LOC in CTX. */
static value get_value_from_loc(rewrite_context ctx, value_loc loc);

/*
 * Evaluate a DWARF expression list defined by LOC and return a location
 * describing how to read/write the value.
 */
static value_loc eval_expr_desc(rewrite_context ctx, const Dwarf_Locdesc* loc);

#if _LIVE_VALS == STACKMAP_LIVE_VALS

/*
 * Evaluate a stack map location record and return a location describing how to
 * read/write the value.
 */
static value_loc eval_expr_sm(rewrite_context ctx, const variable* var);

#endif /* STACKMAP_LIVE_VALS */

/*
 * Forward-propagate value in callee-saved register (in the specified
 * activation) to the stack frame in which it is saved, or the appropriate
 * register of the outer-most activation.
 */
static void propagate_reg(rewrite_context ctx,
                          value val,
                          dwarf_reg reg,
                          int act);

///////////////////////////////////////////////////////////////////////////////
// Data access
///////////////////////////////////////////////////////////////////////////////

#if _LIVE_VALS == DWARF_LIVE_VALS

/*
 * Read VAR's value from HANDLE.
 */
value get_var_val(rewrite_context ctx, const variable* var)
{
  void* pc;
  Dwarf_Locdesc* loc;
  value val = { .is_valid = false, .is_addr = false, .val = 0 };

  ST_INFO("Getting variable '%s' (%s)\n",
          var->name, arch_name(ctx->handle->arch));

  pc = ACT(ctx).regs->pc(ACT(ctx).regs);
  loc = get_loc_desc(var->num_locs, var->locs, pc);
  if(loc) val = get_val_from_desc(ctx, loc);
  else ST_INFO("Value not defined @ PC=%p\n", pc);

  return val;
}

/*
 * Read VAR's location from HANDLE.
 */
value_loc get_var_loc(rewrite_context ctx, const variable* var)
{
  void* pc;
  Dwarf_Locdesc* loc;
  value_loc val_loc = {.is_valid = false};

  ST_INFO("Getting variable location for '%s' (%s)\n",
          var->name, arch_name(ctx->handle->arch));

  pc = ACT(ctx).regs->pc(ACT(ctx).regs);
  loc = get_loc_desc(var->num_locs, var->locs, pc);
  if(loc) val_loc = eval_expr_desc(ctx, loc);
  else ST_INFO("Value's location not defined @ PC=%p\n", pc);

  return val_loc;
}

/*
 * Put VAL into VAR in HANDLE.
 */
value_loc put_var_val(rewrite_context ctx, const variable* var, value val)
{
  void* pc;
  Dwarf_Locdesc* loc;
  value_loc val_loc = {.is_valid = false};

  ST_INFO("Putting variable '%s' (%s)\n",
          var->name, arch_name(ctx->handle->arch));

  pc = ACT(ctx).regs->pc(ACT(ctx).regs);
  loc = get_loc_desc(var->num_locs, var->locs, pc);
  if(loc) val_loc = put_val_from_desc(ctx, loc, var->size, val);
  else ST_INFO("Value not defined @ PC=%p\n", pc);

  return val_loc;
}

#else /* STACKMAP_LIVE_VALS */

/*
 * Read VAR's value from HANDLE.
 */
value get_var_val(rewrite_context ctx, const variable* var)
{
  value_loc loc = eval_expr_sm(ctx, var);
  return get_value_from_loc(ctx, loc);
}

/*
 * Read VAR's location from HANDLE.
 */
value_loc get_var_loc(rewrite_context ctx, const variable* var)
{
  return eval_expr_sm(ctx, var);
}

/*
 * Put VAL into VAR in HANDLE.
 */
value_loc put_var_val(rewrite_context ctx, const variable* var, value val)
{
  value_loc loc = eval_expr_sm(ctx, var);
  put_val_loc(ctx, val, var->size, loc, ctx->act);
  return loc;
}

#endif /* STACKMAP_LIVE_VALS */

/*
 * Put VAL at LOC in ACT from CTX.
 */
void put_val_loc(rewrite_context ctx,
                 value val,
                 uint64_t size,
                 value_loc loc,
                 size_t act)
{
  ASSERT(val.is_valid && loc.is_valid, "invalid value and/or location\n");

  TIMER_FG_START(put_val);

  if(val.is_addr) ST_INFO("Value stored at %p (size=%lu)\n", val.addr, size);
  else ST_INFO("Value: %ld / %lu / %lx\n", val.val, val.val, val.val);

  switch(loc.type)
  {
  case ADDRESS:
    ST_INFO("Storing to %p\n", (void*)loc.addr);
    if(val.is_addr) memcpy((void*)loc.addr, val.addr, size);
    else *(uint64_t*)loc.addr = val.val;
    break;
  case REGISTER:
    // Note: we're copying callee-saved registers into the current frame's
    // register set & the activation where it is saved (or is still alive).
    // This is cheap & should support both eager & on-demand rewriting.
    if(ctx->act > 0 && ctx->handle->props->is_callee_saved(loc.reg))
      propagate_reg(ctx, val, loc.reg, act);
    ctx->acts[act].regs->set_reg(ctx->acts[act].regs, loc.reg, val.val);
    break;
  case CONSTANT: break; // Nothing to do, value is not stored anywhere
  default: ASSERT(false, "unknown value location type\n"); break;
  }

  TIMER_FG_STOP(put_val);
}

/*
 * Using the specified handle, apply the operations described in a location
 * description to extract a value from the current execution state.
 */
value get_val_from_desc(rewrite_context ctx, const Dwarf_Locdesc* loc)
{
  value_loc val_loc = eval_expr_desc(ctx, loc);
  return get_value_from_loc(ctx, val_loc);
}

/*
 * Using the specified handle, apply the operations described in a location
 * description to obtain a value's location and store the specified value
 * there.
 */
value_loc put_val_from_desc(rewrite_context ctx,
                            const Dwarf_Locdesc* loc,
                            uint64_t size,
                            value val)
{
  value_loc val_loc = eval_expr_desc(ctx, loc);
  ASSERT(val_loc.is_valid, "unhandled DWARF location expression\n");
  put_val_loc(ctx, val, size, val_loc, ctx->act);
  return val_loc;
}

/*
 * Set return address of outermost frame in HANDLE to RETADDR.
 */
void set_return_address(rewrite_context ctx, void* retaddr)
{
  size_t rule;
  value_loc loc;
#ifdef _DEBUG
  const char* op_name;
#endif

  ASSERT(retaddr, "invalid return address\n");

  rule = ACT(ctx).regs->ra_rule;
  loc = get_stored_loc(ctx, &ACT(ctx), &ACT(ctx).rules.rt3_rules[rule], false);
  if(loc.is_valid)
  {
    switch(loc.type)
    {
    case ADDRESS:
      ST_INFO("Saving return address %p to %p\n", retaddr, (void*)loc.addr);

      *(uint64_t*)loc.addr = (uint64_t)retaddr;
      break;
    case REGISTER:
#ifdef _DEBUG
      dwarf_get_OP_name(loc.reg.reg, &op_name);
      ST_INFO("Saving return address %p to register %s\n", retaddr, op_name);
#endif
      ACT(ctx).regs->set_reg(ACT(ctx).regs, loc.reg, (uint64_t)retaddr);
      break;
    default:
      ASSERT(false, "invalid return address location\n");
      break;
    }
  }
  else ACT(ctx).regs->set_ra_reg(ACT(ctx).regs, (uint64_t)retaddr);
}

/*
 * Return where in the current frame the previous frame pointer is saved.
 */
uint64_t* get_savedfbp_loc(rewrite_context ctx)
{
  size_t rule;
  value_loc loc;

  rule = ACT(ctx).regs->fbp_rule;
  loc = get_stored_loc(ctx, &ACT(ctx), &ACT(ctx).rules.rt3_rules[rule], false);
  ASSERT(loc.is_valid && loc.type == ADDRESS,
        "invalid saved frame pointer location\n");
  return (uint64_t*)loc.addr;
}

///////////////////////////////////////////////////////////////////////////////
// File-local API (implementation)
///////////////////////////////////////////////////////////////////////////////

/*
 * Extract a value from the location described by LOC in CTX.
 */
static value get_value_from_loc(rewrite_context ctx, value_loc val_loc)
{
  value ret = { .is_valid = true, .is_addr = false, .val = 0 };

  TIMER_FG_START(get_val);

  ASSERT(val_loc.is_valid, "unhandled DWARF location expression\n");
  ret.is_addr = (val_loc.type == ADDRESS ? true : false);
  switch(val_loc.type)
  {
  case ADDRESS: ret.addr = (void*)val_loc.addr; break;
  case REGISTER: ret.val = ACT(ctx).regs->reg(ACT(ctx).regs, val_loc.reg); break;
  case CONSTANT: ret.val = val_loc.val; break;
  default: ST_ERR(1, "unknown value location type\n"); break;
  }

  if(val_loc.type == REGISTER || val_loc.type == CONSTANT)
  {
    /* TODO use this when switching to a byte-mask
    for(i = 0; i < sizeof(ret.val); i++)
      if(!(val_loc.num_bytes & (1 << i)))
        ret.val &= ~(0xff << (i * 8));*/
    switch(val_loc.num_bytes)
    {
    case 0: ret.val &= 0; break;
    case 1: ret.val &= 0xff; break;
    case 2: ret.val &= 0xffff; break;
    case 3: ret.val &= 0xffffff; break;
    case 4: ret.val &= 0xffffffff; break;
    case 5: ret.val &= 0xffffffffff; break;
    case 6: ret.val &= 0xffffffffffff; break;
    case 7: ret.val &= 0xffffffffffffff; break;
    case 8: ret.val &= 0xffffffffffffffff; break;
    default: break;
    }
  }

  TIMER_FG_STOP(get_val);

  return ret;
}

/*
 * Evaluate an expression list contained in LOC using CTX.
 */
static value_loc eval_expr_desc(rewrite_context ctx, const Dwarf_Locdesc* loc)
{
  bool finished = false;
  Dwarf_Unsigned i;
  dwarf_reg reg;
  stack stack;
  value_loc ret;
#if _LIVE_VALS == DWARF_LIVE_VALS
  value_loc fb;
#endif
#ifdef _DEBUG
  const char* op_name;
#endif

  ASSERT(loc, "invalid location description\n");

  TIMER_FG_START(eval_location);

  ret.is_valid = true;
  ret.type = ADDRESS;
  ret.num_bytes = 8;
  ret.addr = 0;
  stack_init(&stack);

  ST_INFO("Number of ops: %d\n", loc->ld_cents);

  for(i = 0; i < loc->ld_cents && !finished; i++)
  {
#ifdef _DEBUG
    dwarf_get_OP_name(loc->ld_s[i].lr_atom, &op_name);
    ST_INFO("%s\n", op_name);
#endif
    switch(loc->ld_s[i].lr_atom)
    {
    /* Literals */
    case DW_OP_lit0: ret.addr = stack_push(&stack, 0); break;
    case DW_OP_lit1: ret.addr = stack_push(&stack, 1); break;
    case DW_OP_lit2: ret.addr = stack_push(&stack, 2); break;
    case DW_OP_lit3: ret.addr = stack_push(&stack, 3); break;
    case DW_OP_lit4: ret.addr = stack_push(&stack, 4); break;
    case DW_OP_lit5: ret.addr = stack_push(&stack, 5); break;
    case DW_OP_lit6: ret.addr = stack_push(&stack, 6); break;
    case DW_OP_lit7: ret.addr = stack_push(&stack, 7); break;
    case DW_OP_lit8: ret.addr = stack_push(&stack, 8); break;
    case DW_OP_lit9: ret.addr = stack_push(&stack, 9); break;
    case DW_OP_lit10: ret.addr = stack_push(&stack, 10); break;
    case DW_OP_lit11: ret.addr = stack_push(&stack, 11); break;
    case DW_OP_lit12: ret.addr = stack_push(&stack, 12); break;
    case DW_OP_lit13: ret.addr = stack_push(&stack, 13); break;
    case DW_OP_lit14: ret.addr = stack_push(&stack, 14); break;
    case DW_OP_lit15: ret.addr = stack_push(&stack, 15); break;
    case DW_OP_lit16: ret.addr = stack_push(&stack, 16); break;
    case DW_OP_lit17: ret.addr = stack_push(&stack, 17); break;
    case DW_OP_lit18: ret.addr = stack_push(&stack, 18); break;
    case DW_OP_lit19: ret.addr = stack_push(&stack, 19); break;
    case DW_OP_lit20: ret.addr = stack_push(&stack, 20); break;
    case DW_OP_lit21: ret.addr = stack_push(&stack, 21); break;
    case DW_OP_lit22: ret.addr = stack_push(&stack, 22); break;
    case DW_OP_lit23: ret.addr = stack_push(&stack, 23); break;
    case DW_OP_lit24: ret.addr = stack_push(&stack, 24); break;
    case DW_OP_lit25: ret.addr = stack_push(&stack, 25); break;
    case DW_OP_lit26: ret.addr = stack_push(&stack, 26); break;
    case DW_OP_lit27: ret.addr = stack_push(&stack, 27); break;
    case DW_OP_lit28: ret.addr = stack_push(&stack, 28); break;
    case DW_OP_lit29: ret.addr = stack_push(&stack, 29); break;
    case DW_OP_lit30: ret.addr = stack_push(&stack, 30); break;
    case DW_OP_lit31: ret.addr = stack_push(&stack, 31); break;

    case DW_OP_addr: ret.addr = stack_push(&stack, loc->ld_s[i].lr_number); break;

    case DW_OP_const1u:
      ret.addr = stack_push(&stack, (uint8_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const2u:
      ret.addr = stack_push(&stack, (uint16_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const4u:
      ret.addr = stack_push(&stack, (uint32_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const8u:
      ret.addr = stack_push(&stack, (uint64_t)loc->ld_s[i].lr_number); break;

    case DW_OP_const1s:
      ret.addr = stack_push(&stack, (int8_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const2s:
      ret.addr = stack_push(&stack, (int16_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const4s:
      ret.addr = stack_push(&stack, (int32_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const8s:
      ret.addr = stack_push(&stack, (int64_t)loc->ld_s[i].lr_number); break;

    case DW_OP_constu:
      ret.addr = stack_push(&stack, loc->ld_s[i].lr_number); break;
    case DW_OP_consts:
      ret.addr = stack_push(&stack, (Dwarf_Signed)loc->ld_s[i].lr_number); break;

    /* Register-based addressing */
    // Note: we assume that these are not extended registers, i.e. we aren't
    // doing offsets from non-64-bit registers
#if _LIVE_VALS == DWARF_LIVE_VALS
    case DW_OP_fbreg:
      TIMER_FG_STOP(eval_location);
      fb = eval_expr_desc(ctx, get_func_fb(ACT(ctx).function));
      TIMER_FG_START(eval_location);
      switch(fb.type)
      {
      case ADDRESS:
        ret.addr = stack_push(&stack, fb.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
        break;
      case REGISTER:
        ret.addr = stack_push(&stack, ACT(ctx).regs->reg(ACT(ctx).regs, fb.reg) +
                                      (Dwarf_Signed)loc->ld_s[i].lr_number);
        break;
      default:
        ST_ERR(1, "invalid value location type (must be address or register\n");
      }
      break;
#endif /* DWARF_LIVE_VALS */
    case DW_OP_breg0:
      reg.reg = DW_OP_reg0;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg1:
      reg.reg = DW_OP_reg1;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg2:
      reg.reg = DW_OP_reg2;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg3:
      reg.reg = DW_OP_reg3;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg4:
      reg.reg = DW_OP_reg4;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg5:
      reg.reg = DW_OP_reg5;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg6:
      reg.reg = DW_OP_reg6;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg7:
      reg.reg = DW_OP_reg7;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg8:
      reg.reg = DW_OP_reg8;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg9:
      reg.reg = DW_OP_reg9;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg10:
      reg.reg = DW_OP_reg10;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg11:
      reg.reg = DW_OP_reg11;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg12:
      reg.reg = DW_OP_reg12;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg13:
      reg.reg = DW_OP_reg13;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg14:
      reg.reg = DW_OP_reg14;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg15:
      reg.reg = DW_OP_reg15;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg16:
      reg.reg = DW_OP_reg16;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg17:
      reg.reg = DW_OP_reg17;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg18:
      reg.reg = DW_OP_reg18;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg19:
      reg.reg = DW_OP_reg19;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg20:
      reg.reg = DW_OP_reg20;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg21:
      reg.reg = DW_OP_reg21;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg22:
      reg.reg = DW_OP_reg22;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg23:
      reg.reg = DW_OP_reg23;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg24:
      reg.reg = DW_OP_reg24;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg25:
      reg.reg = DW_OP_reg25;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg26:
      reg.reg = DW_OP_reg26;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg27:
      reg.reg = DW_OP_reg27;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg28:
      reg.reg = DW_OP_reg28;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg29:
      reg.reg = DW_OP_reg29;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg30:
      reg.reg = DW_OP_reg30;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg31:
      reg.reg = DW_OP_reg31;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_bregx:
      reg.reg = DW_OP_regx;
      reg.x = loc->ld_s[i].lr_number;
      ret.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = stack_push(&stack, ret.addr + ((Dwarf_Signed)loc->ld_s[i].lr_number2));
      break;

    case DW_OP_piece: ret.num_bytes = (uint8_t)loc->ld_s[i].lr_number; break;

    /* Stack operations */
    case DW_OP_dup: ret.addr = stack_dup(&stack); break;
    case DW_OP_drop: ret.addr = stack_drop(&stack); break;
    case DW_OP_pick: ret.addr = stack_pick(&stack, loc->ld_s[i].lr_number); break;
    case DW_OP_over: ret.addr = stack_over(&stack); break;
    case DW_OP_swap: ret.addr = stack_swap(&stack); break;
    case DW_OP_rot: ret.addr = stack_rot(&stack); break;
    case DW_OP_deref:
      ret.addr = stack_drop(&stack);
      ret.addr = stack_push(&stack, *(Dwarf_Unsigned*)ret.addr);
      break;
    case DW_OP_deref_size:
      ret.addr = *(Dwarf_Unsigned*)stack_drop(&stack);
      switch(loc->ld_s[i].lr_number)
      {
      case 0: ret.addr = 0; break;
      case 1: ret.addr &= 0xff; break;
      case 2: ret.addr &= 0xffff; break;
      case 3: ret.addr &= 0xffffff; break;
      case 4: ret.addr &= 0xffffffff; break;
      case 5: ret.addr &= 0xffffffffff; break;
      case 6: ret.addr &= 0xffffffffffff; break;
      case 7: ret.addr &= 0xffffffffffffff; break;
      case 8: break;
      default:
        ST_WARN("error in DWARF operation encoding\n"); break;
      }
      ret.addr = stack_push(&stack, ret.addr);
      break;

    case DW_OP_xderef: // TODO
    case DW_OP_xderef_size: // TODO
      ret.is_valid = false;
      finished = true;
      ST_WARN("multiple address space dereferencing not supported\n");
      break;

    case DW_OP_push_object_address: // TODO
      ret.is_valid = false;
      finished = true;
      ST_WARN("user-defined expressions not supported\n");
      break;

    case DW_OP_form_tls_address: // TODO
      ret.is_valid = false;
      finished = true;
      ST_WARN("thread-local storage addressing not supported\n");
      break;

    case DW_OP_call_frame_cfa:
      ret.addr = stack_push(&stack, (Dwarf_Unsigned)ACT(ctx).cfa); break;

    /* Arithmetic and logical operations */
    case DW_OP_abs: ret.addr = stack_abs(&stack); break;
    case DW_OP_and: ret.addr = stack_and(&stack); break;
    case DW_OP_div: ret.addr = stack_div(&stack); break;
    case DW_OP_minus: ret.addr = stack_minus(&stack); break;
    case DW_OP_mod: ret.addr = stack_mod(&stack); break;
    case DW_OP_mul: ret.addr = stack_mul(&stack); break;
    case DW_OP_neg: ret.addr = stack_neg(&stack); break;
    case DW_OP_not: ret.addr = stack_not(&stack); break;
    case DW_OP_or: ret.addr = stack_or(&stack); break;
    case DW_OP_plus: ret.addr = stack_plus(&stack); break;
    case DW_OP_plus_uconst:
      ret.addr = stack_drop(&stack);
      ret.addr = stack_push(&stack, ret.addr + loc->ld_s[i].lr_number);
      break;
    case DW_OP_shl: ret.addr = stack_shl(&stack); break;
    case DW_OP_shr: ret.addr = stack_shr(&stack); break;
    case DW_OP_shra: ret.addr = stack_shra(&stack); break;
    case DW_OP_xor: ret.addr = stack_xor(&stack); break;
    case DW_OP_le: ret.addr = stack_le(&stack); break;
    case DW_OP_ge: ret.addr = stack_ge(&stack); break;
    case DW_OP_eq: ret.addr = stack_eq(&stack); break;
    case DW_OP_lt: ret.addr = stack_lt(&stack); break;
    case DW_OP_gt: ret.addr = stack_gt(&stack); break;
    case DW_OP_ne: ret.addr = stack_ne(&stack); break;

    case DW_OP_skip: // TODO
    case DW_OP_bra: // TODO
    case DW_OP_call2: // TODO
    case DW_OP_call4: // TODO
    case DW_OP_call_ref: // TODO
      ret.is_valid = false;
      finished = true;
#ifdef _DEBUG
      dwarf_get_OP_name(loc->ld_s[i].lr_atom, &op_name);
      ST_WARN("control-flow operations not yet supported (%s)\n", op_name);
#endif
      break;

    case DW_OP_nop: break;

    case DW_OP_GNU_entry_value:
      // Data has been clobbered since function entry -- do we need to actually
      // recover it?
      ret.is_valid = false;
      finished = true;
#ifdef _DEBUG
      dwarf_get_OP_name(DW_OP_GNU_entry_value, &op_name);
      ST_WARN("value was clobbered (%s)\n", op_name);
#endif
      break;

    /* Register location descriptions */
    case DW_OP_reg0: case DW_OP_reg1: case DW_OP_reg2: case DW_OP_reg3:
    case DW_OP_reg4: case DW_OP_reg5: case DW_OP_reg6: case DW_OP_reg7:
    case DW_OP_reg8: case DW_OP_reg9: case DW_OP_reg10: case DW_OP_reg11:
    case DW_OP_reg12: case DW_OP_reg13: case DW_OP_reg14: case DW_OP_reg15:
    case DW_OP_reg16: case DW_OP_reg17: case DW_OP_reg18: case DW_OP_reg19:
    case DW_OP_reg20: case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23:
    case DW_OP_reg24: case DW_OP_reg25: case DW_OP_reg26: case DW_OP_reg27:
    case DW_OP_reg28: case DW_OP_reg29: case DW_OP_reg30: case DW_OP_reg31:
    case DW_OP_regx:
      reg.reg = loc->ld_s[i].lr_atom;
      reg.x = loc->ld_s[i].lr_number;
      if(ctx->handle->props->is_ext_reg(reg))
        ret.addr = (Dwarf_Unsigned)ACT(ctx).regs->ext_reg(ACT(ctx).regs, reg);
      else
      {
        ret.type = REGISTER;
        ret.reg.reg = loc->ld_s[i].lr_atom;
        ret.reg.x = loc->ld_s[i].lr_number;
      }
      break;

    /* Implicit location descriptions */
    case DW_OP_implicit_value:
      ret.type = CONSTANT;
      ret.val = loc->ld_s[i].lr_number;
      break;
    case DW_OP_stack_value:
      ret.type = CONSTANT;
      ret.val = stack_drop(&stack);
      break;

    default:
      ret.is_valid = false;
      finished = true;
#ifdef _DEBUG
      dwarf_get_OP_name(loc->ld_s[i].lr_atom, &op_name);
      ST_WARN("unknown/unsupported operation (%s)\n", op_name);
#endif
      break;
    }
  }

  TIMER_FG_STOP(eval_location);

  return ret;
}

#if _LIVE_VALS == STACKMAP_LIVE_VALS

/*
 * Evaluate stack map location record VAR and return a value location.
 */
static value_loc eval_expr_sm(rewrite_context ctx, const variable* var)
{
  dwarf_reg reg;
  value_loc loc = {
    .is_valid = true,
    .num_bytes = var->size,
    .type = ADDRESS,
    .addr = 0
  };

  TIMER_FG_START(eval_location);

  switch(var->type)
  {
  case SM_REGISTER: // Value is in register
    loc.num_bytes = var->size;
    if(ctx->handle->props->is_ext_reg(OP_REG(var->regnum)))
    {
      loc.addr = (Dwarf_Unsigned)ACT(ctx).regs->ext_reg(ACT(ctx).regs,
                                                        OP_REG(var->regnum));
      ST_INFO("Value is in extended register %d\n", var->regnum);
    }
    else
    {
      loc.type = REGISTER;
      loc.reg = OP_REG(var->regnum);
      ST_INFO("Value is in register %d\n", var->regnum);
    }
    break;
  case SM_DIRECT: // Value is allocated on stack
    loc.num_bytes = ctx->handle->ptr_size;
    reg = OP_REG(var->regnum);
    loc.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg) +
               var->offset_or_constant;
    ST_INFO("Value is at %p\n", (void*)loc.addr);
    break;
  case SM_INDIRECT: // Value is in register, but spilled to the stack
    loc.num_bytes = var->size;
    reg = OP_REG(var->regnum);
    loc.addr = ACT(ctx).regs->reg(ACT(ctx).regs, reg) +
               var->offset_or_constant;
    ST_INFO("Value is at %p\n", (void*)loc.addr);
    break;
  case SM_CONSTANT: // Value is constant
    loc.num_bytes = sizeof(var->offset_or_constant);
    loc.type = CONSTANT;
    loc.val = var->offset_or_constant;
    ST_INFO("Value is %lu\n", loc.val);
    break;
  case SM_CONST_IDX:
    ASSERT(false, "constants in constant pool not supported\n");
    loc.is_valid = false;
    break;
  }

  TIMER_FG_STOP(eval_location);

  return loc;
}

#endif /* STACKMAP_LIVE_VALS */

static void propagate_reg(rewrite_context ctx,
                          value val,
                          dwarf_reg reg,
                          int act)
{
  bool saved = false;
  value_loc loc;
#ifdef _DEBUG
  const char* reg_name;
  dwarf_get_OP_name(reg.reg, &reg_name);
#endif

  ASSERT(val.is_valid, "cannot propagate invalid value\n");
  ASSERT(act > 0, "cannot propagate value from outermost frame\n");

  for(act--; act >= 0; act--)
  {
    if(bitmap_is_set(ctx->acts[act].callee_saved, RAW_REG(reg)))
    {
      loc = get_stored_loc(ctx,
                           &ctx->acts[act],
                           &ctx->acts[act].rules.rt3_rules[RAW_REG(reg)],
                           false);
      ASSERT(loc.type == ADDRESS, "invalid callee-saved slot\n");
      *(uint64_t*)loc.addr = val.val;
      ST_INFO("Saving callee-saved register %s at %p (frame %d)\n",
              reg_name, (void*)loc.addr, act);
      saved = true;
      break;
    }
  }

  if(!saved)
  {
    ctx->acts[0].regs->set_reg(ctx->acts[0].regs, reg, val.val);
    ST_INFO("Callee-saved register %s live in outer-most frame\n", reg_name);
  }
}

