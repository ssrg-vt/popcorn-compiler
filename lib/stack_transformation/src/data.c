/*
 * APIs for accessing frame-specific data, i.e. arguments/local variables/live
 * values, return address, and saved frame pointer.
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
// DWARF stack machine
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

/*
 * Get pointer to the stack save slot or the register in the outer-most
 * activation in which a callee-saved register is saved.
 */
static void* callee_saved_loc(rewrite_context ctx, value val);

///////////////////////////////////////////////////////////////////////////////
// Data access
///////////////////////////////////////////////////////////////////////////////

#if _LIVE_VALS == DWARF_LIVE_VALS

/*
 * Get VAR's value from CTX.
 */
value get_var_val(rewrite_context ctx, const variable* var)
{
  void* pc;
  Dwarf_Locdesc* loc;

  ST_INFO("Getting variable '%s' (%s)\n",
          var->name, arch_name(ctx->handle->arch));

  pc = ACT(ctx).regs->pc(ACT(ctx).regs);
  loc = get_loc_desc(var->num_locs, var->locs, pc);
  if(loc) return get_val_from_desc(ctx, loc);
  else
  {
    ST_INFO("Variable not defined @ PC=%p\n", pc);
    return VAL_NOT_DEFINED;
  }
}

#else /* STACKMAP_LIVE_VALS */

/*
 * Evaluate stack map location record VAR and return a value location.
 */
value get_var_val(rewrite_context ctx, const variable* var)
{
  dwarf_reg reg;
  value loc = {
    .is_valid = true,
    .act = ctx->act,
    .num_bytes = var->size,
  };

  TIMER_FG_START(eval_location);

  switch(var->type)
  {
  case SM_REGISTER: // Value is in register
    loc.type = REGISTER;
    loc.reg = OP_REG(var->regnum);
    break;
  // Note: these value types are fundamentally different, but their values are
  // generated in an identical manner
  case SM_DIRECT: // Value is allocated on stack
  case SM_INDIRECT: // Value is in register, but spilled to the stack
    loc.type = ADDRESS;
    reg = OP_REG(var->regnum);
    loc.addr = *(void**)ACT(ctx).regs->reg(ACT(ctx).regs, reg) +
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
  }

  TIMER_FG_STOP(eval_location);

  return loc;
}

#endif /* STACKMAP_LIVE_VALS */

/*
 * Put SRC_VAL from SRC at DEST_VAL from DEST.
 */
void put_val(rewrite_context src,
             value src_val,
             rewrite_context dest,
             value dest_val,
             uint64_t size)
{
  void* src_addr, *dest_addr, *callee_addr = NULL;

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
    ST_INFO("Source value in register %llu\n", RAW_REG(src_val.reg));
    src_addr = src->acts[src_val.act].regs->reg(src->acts[src_val.act].regs,
                                                src_val.reg);
    break;
  case CONSTANT:
    ST_INFO("Source value: %ld / %lu / %lx\n",
            src_val.cnst, src_val.cnst, src_val.cnst);
    src_addr = &src_val.cnst;
    break;
  default:
    ASSERT(false, "unknown value location type\n");
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
    // This is cheap & should support both eager & on-demand rewriting.
    ST_INFO("Destination value in register %llu\n", RAW_REG(dest_val.reg));
    dest_addr = dest->acts[dest_val.act].regs->reg(dest->acts[dest_val.act].regs,
                                                   dest_val.reg);
    if(dest->handle->props->is_callee_saved(dest_val.reg))
      callee_addr = callee_saved_loc(dest, dest_val);
    break;
  case CONSTANT:
    dest_addr = &dest_val.cnst;
    break;
  default:
    ASSERT(false, "unknown value location type\n");
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
  size_t rule;
  value loc;
  void* retaddr_slot;
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
      ST_INFO("Saving return address %p to %p\n", retaddr, loc.addr);
      retaddr_slot = loc.addr;
      break;
    case REGISTER:
#ifdef _DEBUG
      dwarf_get_OP_name(loc.reg.reg, &op_name);
      ST_INFO("Saving return address %p to register %s\n", retaddr, op_name);
#endif
      retaddr_slot = ACT(ctx).regs->reg(ACT(ctx).regs, loc.reg);
      break;
    default:
      ASSERT(false, "invalid return address location\n");
      retaddr_slot = NULL;
      break;
    }
    *(void**)retaddr_slot = retaddr;
  }
  else ACT(ctx).regs->set_ra_reg(ACT(ctx).regs, retaddr);
}

/*
 * Return where in the current frame the previous frame pointer is saved.
 */
uint64_t* get_savedfbp_loc(rewrite_context ctx)
{
  size_t rule;
  value loc;

  rule = ACT(ctx).regs->fbp_rule;
  loc = get_stored_loc(ctx, &ACT(ctx), &ACT(ctx).rules.rt3_rules[rule], false);
  ASSERT(loc.is_valid && loc.type == ADDRESS,
        "invalid saved frame pointer location\n");
  return (uint64_t*)loc.addr;
}

/*
 * Evaluate an expression list contained in LOC using CTX.
 */
value get_val_from_desc(rewrite_context ctx, const Dwarf_Locdesc* loc)
{
  bool finished = false;
  uint64_t regval;
  Dwarf_Unsigned i;
  dwarf_reg reg;
  stack stack;
  value ret = {
    .is_valid = true,
    .act = ctx->act,
    .type = ADDRESS,
    .num_bytes = ctx->handle->ptr_size,
    .addr = NULL
  };
#if _LIVE_VALS == DWARF_LIVE_VALS
  value fb;
#endif
#ifdef _DEBUG
  const char* op_name;
#endif

  ASSERT(loc, "invalid location description\n");

  TIMER_FG_START(eval_location);

  ST_INFO("Number of ops: %d\n", loc->ld_cents);

  stack_init(&stack);
  for(i = 0; i < loc->ld_cents && !finished; i++)
  {
#ifdef _DEBUG
    dwarf_get_OP_name(loc->ld_s[i].lr_atom, &op_name);
    ST_INFO("%s\n", op_name);
#endif
    switch(loc->ld_s[i].lr_atom)
    {
    /* Literals */
    case DW_OP_lit0: ret.addr = (void*)stack_push(&stack, 0); break;
    case DW_OP_lit1: ret.addr = (void*)stack_push(&stack, 1); break;
    case DW_OP_lit2: ret.addr = (void*)stack_push(&stack, 2); break;
    case DW_OP_lit3: ret.addr = (void*)stack_push(&stack, 3); break;
    case DW_OP_lit4: ret.addr = (void*)stack_push(&stack, 4); break;
    case DW_OP_lit5: ret.addr = (void*)stack_push(&stack, 5); break;
    case DW_OP_lit6: ret.addr = (void*)stack_push(&stack, 6); break;
    case DW_OP_lit7: ret.addr = (void*)stack_push(&stack, 7); break;
    case DW_OP_lit8: ret.addr = (void*)stack_push(&stack, 8); break;
    case DW_OP_lit9: ret.addr = (void*)stack_push(&stack, 9); break;
    case DW_OP_lit10: ret.addr = (void*)stack_push(&stack, 10); break;
    case DW_OP_lit11: ret.addr = (void*)stack_push(&stack, 11); break;
    case DW_OP_lit12: ret.addr = (void*)stack_push(&stack, 12); break;
    case DW_OP_lit13: ret.addr = (void*)stack_push(&stack, 13); break;
    case DW_OP_lit14: ret.addr = (void*)stack_push(&stack, 14); break;
    case DW_OP_lit15: ret.addr = (void*)stack_push(&stack, 15); break;
    case DW_OP_lit16: ret.addr = (void*)stack_push(&stack, 16); break;
    case DW_OP_lit17: ret.addr = (void*)stack_push(&stack, 17); break;
    case DW_OP_lit18: ret.addr = (void*)stack_push(&stack, 18); break;
    case DW_OP_lit19: ret.addr = (void*)stack_push(&stack, 19); break;
    case DW_OP_lit20: ret.addr = (void*)stack_push(&stack, 20); break;
    case DW_OP_lit21: ret.addr = (void*)stack_push(&stack, 21); break;
    case DW_OP_lit22: ret.addr = (void*)stack_push(&stack, 22); break;
    case DW_OP_lit23: ret.addr = (void*)stack_push(&stack, 23); break;
    case DW_OP_lit24: ret.addr = (void*)stack_push(&stack, 24); break;
    case DW_OP_lit25: ret.addr = (void*)stack_push(&stack, 25); break;
    case DW_OP_lit26: ret.addr = (void*)stack_push(&stack, 26); break;
    case DW_OP_lit27: ret.addr = (void*)stack_push(&stack, 27); break;
    case DW_OP_lit28: ret.addr = (void*)stack_push(&stack, 28); break;
    case DW_OP_lit29: ret.addr = (void*)stack_push(&stack, 29); break;
    case DW_OP_lit30: ret.addr = (void*)stack_push(&stack, 30); break;
    case DW_OP_lit31: ret.addr = (void*)stack_push(&stack, 31); break;

    case DW_OP_addr: ret.addr = (void*)stack_push(&stack, loc->ld_s[i].lr_number); break;

    case DW_OP_const1u:
      ret.addr = (void*)stack_push(&stack, (uint8_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const2u:
      ret.addr = (void*)stack_push(&stack, (uint16_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const4u:
      ret.addr = (void*)stack_push(&stack, (uint32_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const8u:
      ret.addr = (void*)stack_push(&stack, (uint64_t)loc->ld_s[i].lr_number); break;

    case DW_OP_const1s:
      ret.addr = (void*)stack_push(&stack, (int8_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const2s:
      ret.addr = (void*)stack_push(&stack, (int16_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const4s:
      ret.addr = (void*)stack_push(&stack, (int32_t)loc->ld_s[i].lr_number); break;
    case DW_OP_const8s:
      ret.addr = (void*)stack_push(&stack, (int64_t)loc->ld_s[i].lr_number); break;

    case DW_OP_constu:
      ret.addr = (void*)stack_push(&stack, loc->ld_s[i].lr_number); break;
    case DW_OP_consts:
      ret.addr = (void*)stack_push(&stack, (Dwarf_Signed)loc->ld_s[i].lr_number); break;

    /* Register-based addressing */
    // Note: we assume that we're doing offsets from 64-bit registers
#if _LIVE_VALS == DWARF_LIVE_VALS
    case DW_OP_fbreg:
      TIMER_FG_STOP(eval_location);
      fb = get_val_from_desc(ctx, get_func_fb(ACT(ctx).function));
      TIMER_FG_START(eval_location);
      switch(fb.type)
      {
      case ADDRESS:
        ret.addr = (void*)stack_push(&stack, fb.cnst + ((Dwarf_Signed)loc->ld_s[i].lr_number));
        break;
      case REGISTER:
        regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, fb.reg);
        ret.addr = (void*)stack_push(&stack, regval + (Dwarf_Signed)loc->ld_s[i].lr_number);
        break;
      default:
        ST_ERR(1, "invalid value location type (must be address or register\n");
      }
      break;
#endif /* DWARF_LIVE_VALS */
    case DW_OP_breg0:
      reg.reg = DW_OP_reg0;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg1:
      reg.reg = DW_OP_reg1;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg2:
      reg.reg = DW_OP_reg2;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg3:
      reg.reg = DW_OP_reg3;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg4:
      reg.reg = DW_OP_reg4;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg5:
      reg.reg = DW_OP_reg5;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg6:
      reg.reg = DW_OP_reg6;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg7:
      reg.reg = DW_OP_reg7;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg8:
      reg.reg = DW_OP_reg8;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg9:
      reg.reg = DW_OP_reg9;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg10:
      reg.reg = DW_OP_reg10;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg11:
      reg.reg = DW_OP_reg11;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg12:
      reg.reg = DW_OP_reg12;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg13:
      reg.reg = DW_OP_reg13;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg14:
      reg.reg = DW_OP_reg14;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg15:
      reg.reg = DW_OP_reg15;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg16:
      reg.reg = DW_OP_reg16;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg17:
      reg.reg = DW_OP_reg17;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg18:
      reg.reg = DW_OP_reg18;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg19:
      reg.reg = DW_OP_reg19;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg20:
      reg.reg = DW_OP_reg20;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg21:
      reg.reg = DW_OP_reg21;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg22:
      reg.reg = DW_OP_reg22;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg23:
      reg.reg = DW_OP_reg23;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg24:
      reg.reg = DW_OP_reg24;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg25:
      reg.reg = DW_OP_reg25;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg26:
      reg.reg = DW_OP_reg26;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg27:
      reg.reg = DW_OP_reg27;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg28:
      reg.reg = DW_OP_reg28;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg29:
      reg.reg = DW_OP_reg29;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg30:
      reg.reg = DW_OP_reg30;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_breg31:
      reg.reg = DW_OP_reg31;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number));
      break;
    case DW_OP_bregx:
      reg.reg = DW_OP_regx;
      reg.x = loc->ld_s[i].lr_number;
      regval = *(uint64_t*)ACT(ctx).regs->reg(ACT(ctx).regs, reg);
      ret.addr = (void*)stack_push(&stack, regval + ((Dwarf_Signed)loc->ld_s[i].lr_number2));
      break;

    case DW_OP_piece: ret.num_bytes = (uint8_t)loc->ld_s[i].lr_number; break;

    /* Stack operations */
    case DW_OP_dup: ret.addr = (void*)stack_dup(&stack); break;
    case DW_OP_drop: ret.addr = (void*)stack_drop(&stack); break;
    case DW_OP_pick: ret.addr = (void*)stack_pick(&stack, loc->ld_s[i].lr_number); break;
    case DW_OP_over: ret.addr = (void*)stack_over(&stack); break;
    case DW_OP_swap: ret.addr = (void*)stack_swap(&stack); break;
    case DW_OP_rot: ret.addr = (void*)stack_rot(&stack); break;
    case DW_OP_deref:
      ret.addr = (void*)stack_drop(&stack);
      ret.addr = (void*)stack_push(&stack, *(Dwarf_Unsigned*)ret.addr);
      break;
    case DW_OP_deref_size:
      // Note: manipulating value using cnst field to avoid type issues when
      // masking void*
      ret.cnst = *(Dwarf_Unsigned*)stack_drop(&stack);
      switch(loc->ld_s[i].lr_number)
      {
      case 0: ret.cnst = 0; break;
      case 1: ret.cnst &= 0xff; break;
      case 2: ret.cnst &= 0xffff; break;
      case 3: ret.cnst &= 0xffffff; break;
      case 4: ret.cnst &= 0xffffffff; break;
      case 5: ret.cnst &= 0xffffffffff; break;
      case 6: ret.cnst &= 0xffffffffffff; break;
      case 7: ret.cnst &= 0xffffffffffffff; break;
      case 8: break;
      default:
        ST_WARN("error in DWARF operation encoding\n"); break;
      }
      ret.addr = (void*)stack_push(&stack, ret.cnst);
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
      ret.addr = (void*)stack_push(&stack, (Dwarf_Unsigned)ACT(ctx).cfa); break;

    /* Arithmetic and logical operations */
    case DW_OP_abs: ret.addr = (void*)stack_abs(&stack); break;
    case DW_OP_and: ret.addr = (void*)stack_and(&stack); break;
    case DW_OP_div: ret.addr = (void*)stack_div(&stack); break;
    case DW_OP_minus: ret.addr = (void*)stack_minus(&stack); break;
    case DW_OP_mod: ret.addr = (void*)stack_mod(&stack); break;
    case DW_OP_mul: ret.addr = (void*)stack_mul(&stack); break;
    case DW_OP_neg: ret.addr = (void*)stack_neg(&stack); break;
    case DW_OP_not: ret.addr = (void*)stack_not(&stack); break;
    case DW_OP_or: ret.addr = (void*)stack_or(&stack); break;
    case DW_OP_plus: ret.addr = (void*)stack_plus(&stack); break;
    case DW_OP_plus_uconst:
      ret.addr = (void*)stack_drop(&stack);
      ret.addr = (void*)stack_push(&stack, ret.cnst + loc->ld_s[i].lr_number);
      break;
    case DW_OP_shl: ret.addr = (void*)stack_shl(&stack); break;
    case DW_OP_shr: ret.addr = (void*)stack_shr(&stack); break;
    case DW_OP_shra: ret.addr = (void*)stack_shra(&stack); break;
    case DW_OP_xor: ret.addr = (void*)stack_xor(&stack); break;
    case DW_OP_le: ret.addr = (void*)stack_le(&stack); break;
    case DW_OP_ge: ret.addr = (void*)stack_ge(&stack); break;
    case DW_OP_eq: ret.addr = (void*)stack_eq(&stack); break;
    case DW_OP_lt: ret.addr = (void*)stack_lt(&stack); break;
    case DW_OP_gt: ret.addr = (void*)stack_gt(&stack); break;
    case DW_OP_ne: ret.addr = (void*)stack_ne(&stack); break;

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
      ret.type = REGISTER;
      ret.reg.reg = loc->ld_s[i].lr_atom;
      ret.reg.x = loc->ld_s[i].lr_number;
      ret.num_bytes = ctx->handle->props->reg_size(ret.reg);
      break;

    /* Implicit location descriptions */
    case DW_OP_implicit_value:
      ret.type = CONSTANT;
      ret.num_bytes = 8;
      ret.cnst = loc->ld_s[i].lr_number;
      break;
    case DW_OP_stack_value:
      ret.type = CONSTANT;
      ret.num_bytes = 8;
      ret.cnst = stack_drop(&stack);
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

///////////////////////////////////////////////////////////////////////////////
// File-local API (implementation)
///////////////////////////////////////////////////////////////////////////////

static void* callee_saved_loc(rewrite_context ctx, value val)
{
  int act = val.act;
  value loc;
#ifdef _DEBUG
  const char* reg_name;
  dwarf_get_OP_name(val.reg.reg, &reg_name);
#endif

  ASSERT(val.is_valid, "cannot get callee-saved location for invalid value\n");
  ASSERT(val.type == REGISTER,
         "cannot get callee-saved location for non-register value type\n");

  /* Nothing to propagate from outermost frame */
  if(act <= 0) return NULL;

  /* Walk call chain to check if register has been saved. */
  for(act--; act >= 0; act--)
  {
    if(bitmap_is_set(ctx->acts[act].callee_saved, RAW_REG(val.reg)))
    {
      loc = get_stored_loc(ctx,
                           &ctx->acts[act],
                           &ctx->acts[act].rules.rt3_rules[RAW_REG(val.reg)],
                           false);
      ASSERT(loc.type == ADDRESS, "invalid callee-saved slot\n");
      ST_INFO("Saving callee-saved register %s at %p (frame %d)\n",
              reg_name, loc.addr, act);
      return loc.addr;
    }
  }

  /* Register is still live in outermost frame. */
  ST_INFO("Callee-saved register %s live in outer-most frame\n", reg_name);
  return ctx->acts[0].regs->reg(ctx->acts[0].regs, val.reg);
}

