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

#define AARCH64_FBP_OFFSET 2
#define AARCH64_FPREG_LOW 64
#define AARCH64_FPREG_HIGH 96
#define AARCH64_FPREG_SAVED_LOW 72
#define AARCH64_FPREG_SAVED_HIGH 80

static uint64_t callee_saved_aarch64[] = {
  19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, /* General-purpose */
  72, 73, 74, 75, 76, 77, 78, 79 /* Floating-point/SIMD (only 64-bits) */
};

static void* align_sp_aarch64(void* sp);
static bool is_callee_saved_aarch64(dwarf_reg reg);
static bool is_ext_reg_aarch64(dwarf_reg reg);

/* aarch64 properties */
const struct properties_t properties_aarch64 = {
  .sp_needs_align = false,
  .num_callee_saved = sizeof(callee_saved_aarch64) / sizeof(uint64_t),
  .callee_saved = callee_saved_aarch64,

  .align_sp = align_sp_aarch64,
  .is_callee_saved = is_callee_saved_aarch64,
  .is_ext_reg = is_ext_reg_aarch64
};

///////////////////////////////////////////////////////////////////////////////
// aarch64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* align_sp_aarch64(void* sp)
{
  // Nothing to do for aarch64, stack pointer is at correct alignment on
  // function entry
  ASSERT(false, "stack-pointer fixup not needed for aarch64\n");
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

  /* Floating-point registers v8-v15 */
  case DW_OP_regx:
    if(AARCH64_FPREG_SAVED_LOW <= reg.x && reg.x < AARCH64_FPREG_SAVED_HIGH)
      return true;
    else
      return false;

  default:
    return false;
  }
}

static bool is_ext_reg_aarch64(dwarf_reg reg)
{
  /* Only floating-point registers are extended-size */
  if(reg.reg == DW_OP_regx &&
     (AARCH64_FPREG_LOW <= reg.x && reg.x < AARCH64_FPREG_HIGH))
    return true;
  else return false;
}

