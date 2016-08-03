/*
 * x86-64 stack properties.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/18/2016
 */

#include "definitions.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define X86_64_STACK_ALIGNMENT 0x10
#define X86_64_SP_FIXUP 0x8

static const uint16_t callee_saved_x86_64[] = {
  3, 6, 12, 13, 14, 15, 16
};

static const uint16_t callee_save_size_x86_64[] = {
  8, 8, 8, 8, 8, 8, 8
};

static void* align_sp_x86_64(void* sp);
static bool is_callee_saved_x86_64(dwarf_reg reg);
static uint16_t reg_size_x86_64(dwarf_reg reg);

/* x86-64 properties. */
const struct properties_t properties_x86_64 = {
  .sp_needs_align = true,

  .num_callee_saved = sizeof(callee_saved_x86_64) / sizeof(uint16_t),
  .callee_saved = callee_saved_x86_64,
  .callee_save_size = callee_save_size_x86_64,

  .align_sp = align_sp_x86_64,
  .is_callee_saved = is_callee_saved_x86_64,
  .reg_size = reg_size_x86_64
};

///////////////////////////////////////////////////////////////////////////////
// x86-64 APIs
///////////////////////////////////////////////////////////////////////////////

// TODO can we expect sp to always be 16-byte aligned?
static void* align_sp_x86_64(void* sp)
{
  ASSERT(!((uint64_t)sp % X86_64_STACK_ALIGNMENT), "invalid stack pointer\n");
  return sp - X86_64_SP_FIXUP;
}

static bool is_callee_saved_x86_64(dwarf_reg reg)
{
  switch(reg.reg)
  {
  case DW_OP_reg3: /* RBX */
  case DW_OP_reg6: /* RBP */
  case DW_OP_reg12: /* R12 */
  case DW_OP_reg13: /* R13 */
  case DW_OP_reg14: /* R14 */
  case DW_OP_reg15: /* R15 */
  case DW_OP_reg16: /* RIP */
    return true;
  default:
    return false;
  }
}

static uint16_t reg_size_x86_64(dwarf_reg reg)
{
  const char* op_name;

  switch(reg.reg)
  {
  /* General-purpose registers */
  case DW_OP_reg0: case DW_OP_reg1: case DW_OP_reg2: case DW_OP_reg3:
  case DW_OP_reg4: case DW_OP_reg5: case DW_OP_reg6: case DW_OP_reg7:
  case DW_OP_reg8: case DW_OP_reg9: case DW_OP_reg10: case DW_OP_reg11:
  case DW_OP_reg12: case DW_OP_reg13: case DW_OP_reg14: case DW_OP_reg15:
  case DW_OP_reg16:
    return sizeof(uint64_t);

  /* XMM floating-point registers */
  case DW_OP_reg17: case DW_OP_reg18: case DW_OP_reg19: case DW_OP_reg20:
  case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23: case DW_OP_reg24:
  case DW_OP_reg25: case DW_OP_reg26: case DW_OP_reg27: case DW_OP_reg28:
  case DW_OP_reg29: case DW_OP_reg30: case DW_OP_reg31:
    return sizeof(unsigned __int128);
  case DW_OP_regx:
    switch(reg.x)
    {
    case 32: return sizeof(unsigned __int128);
    default: break;
    }
    break;

  default: break;
  }

  dwarf_get_OP_name(reg.reg, &op_name);
  ASSERT(false, "unknown/invalid register '%s' (x86-64)\n", op_name);
  return 0;
}

