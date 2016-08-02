/*
 * aarch64 stack properties.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/18/2016
 */

#include "definitions.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define AARCH64_FPREG_LOW 64
#define AARCH64_FPREG_HIGH 96
#define AARCH64_FPREG_SAVED_LOW 72
#define AARCH64_FPREG_SAVED_HIGH 80

static const uint16_t callee_saved_aarch64[] = {
  19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, /* General-purpose */
  72, 73, 74, 75, 76, 77, 78, 79 /* Floating-point/SIMD (only 64-bits) */
};

static const uint16_t callee_save_size_aarch64[] = {
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* General-purpose */
  8, 8, 8, 8, 8, 8, 8, 8 /* Floating-point/SIMD (only 64-bits) */
};

static void* align_sp_aarch64(void* sp);
static bool is_callee_saved_aarch64(dwarf_reg reg);
static uint16_t reg_size_aarch64(dwarf_reg reg);

/* aarch64 properties */
const struct properties_t properties_aarch64 = {
  .sp_needs_align = false,

  .num_callee_saved = sizeof(callee_saved_aarch64) / sizeof(uint16_t),
  .callee_saved = callee_saved_aarch64,
  .callee_save_size = callee_save_size_aarch64,

  .align_sp = align_sp_aarch64,
  .is_callee_saved = is_callee_saved_aarch64,
  .reg_size = reg_size_aarch64
};

///////////////////////////////////////////////////////////////////////////////
// aarch64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* align_sp_aarch64(void* sp)
{
  // Nothing to do for aarch64, stack pointer is at correct alignment on
  // function entry
  ASSERT(false, "stack-pointer alignment not needed for aarch64\n");
  return NULL;
}

static bool is_callee_saved_aarch64(dwarf_reg reg)
{
  switch(reg.reg)
  {
  /* General-purpose registers x19-x28 */
  case DW_OP_reg19: case DW_OP_reg20: case DW_OP_reg21: case DW_OP_reg22:
  case DW_OP_reg23: case DW_OP_reg24: case DW_OP_reg25: case DW_OP_reg26:
  case DW_OP_reg27: case DW_OP_reg28:
    return true;

  case DW_OP_regx:
    /* Floating-point registers v8-v15 */
    if(AARCH64_FPREG_SAVED_LOW <= reg.x && reg.x < AARCH64_FPREG_SAVED_HIGH)
      return true;
    break;

  default: break;
  }

  return false;
}

static uint16_t reg_size_aarch64(dwarf_reg reg)
{
  const char* op_name;

  switch(reg.reg)
  {
  /* General-purpose registers */
  case DW_OP_reg0: case DW_OP_reg1: case DW_OP_reg2: case DW_OP_reg3:
  case DW_OP_reg4: case DW_OP_reg5: case DW_OP_reg6: case DW_OP_reg7:
  case DW_OP_reg8: case DW_OP_reg9: case DW_OP_reg10: case DW_OP_reg11:
  case DW_OP_reg12: case DW_OP_reg13: case DW_OP_reg14: case DW_OP_reg15:
  case DW_OP_reg16: case DW_OP_reg17: case DW_OP_reg18: case DW_OP_reg19:
  case DW_OP_reg20: case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23:
  case DW_OP_reg24: case DW_OP_reg25: case DW_OP_reg26: case DW_OP_reg27:
  case DW_OP_reg28: case DW_OP_reg29: case DW_OP_reg30: case DW_OP_reg31:
    return sizeof(uint64_t);

  case DW_OP_regx:
    /* Floating-point registers */
    if(AARCH64_FPREG_LOW <= reg.x && reg.x < AARCH64_FPREG_HIGH)
      return sizeof(unsigned __int128);
    break;

  default: break;
  }

  dwarf_get_OP_name(reg.reg, &op_name);
  ASSERT(false, "unknown/invalid register '%s' (aarch64)\n", op_name);
  return 0;
}

