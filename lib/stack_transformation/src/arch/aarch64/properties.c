/*
 * aarch64 stack properties.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/18/2016
 */

#include "definitions.h"
#include "arch/aarch64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define AARCH64_RA_OFFSET -0x8
#define AARCH64_SAVED_FBP_OFFSET -0x10
#define AARCH64_CFA_OFFSET_FUNCENTRY 0x0

static const uint16_t callee_saved_aarch64[] = {
  /* General-purpose */
  X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30,

  /* Floating-point/SIMD (only least-significant 64-bits) */
  V8, V9, V10, V11, V12, V13, V14, V15
};

static const uint16_t callee_saved_size_aarch64[] = {
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* General-purpose */
  8, 8, 8, 8, 8, 8, 8, 8 /* Floating-point/SIMD (only 64-bits) */
};

static void* align_sp_aarch64(void* sp);
static bool is_callee_saved_aarch64(uint16_t reg);
static uint16_t callee_reg_size_aarch64(uint16_t reg);

/* aarch64 properties */
const struct properties_t properties_aarch64 = {
  .sp_needs_align = false,
  .num_callee_saved = sizeof(callee_saved_aarch64) / sizeof(uint16_t),
  .callee_saved = callee_saved_aarch64,
  .callee_saved_size = callee_saved_size_aarch64,
  .ra_offset = AARCH64_RA_OFFSET,
  .savedfbp_offset = AARCH64_SAVED_FBP_OFFSET,
  .cfa_offset_funcentry = AARCH64_CFA_OFFSET_FUNCENTRY,

  .align_sp = align_sp_aarch64,
  .is_callee_saved = is_callee_saved_aarch64,
  .callee_reg_size = callee_reg_size_aarch64,
};

///////////////////////////////////////////////////////////////////////////////
// aarch64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* align_sp_aarch64(void* sp)
{
  // Nothing to do for aarch64, stack pointer is at correct alignment on
  // function entry
  ST_ERR(0, "stack-pointer alignment not needed for aarch64\n");
  return NULL;
}

static bool is_callee_saved_aarch64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers x19-x28 */
  case X19: case X20: case X21: case X22: case X23: case X24:
  case X25: case X26: case X27: case X28: case X29: case X30:
    return true;

  /* Floating-point registers v8-v15 */
  case V8: case V9: case V10: case V11: case V12: case V13: case V14: case V15:
    return true;

  default: return false;
  }
}

static uint16_t callee_reg_size_aarch64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers x19-x28 */
  case X19: case X20: case X21: case X22: case X23: case X24:
  case X25: case X26: case X27: case X28: case X29: case X30:
    return 8;

  /* Floating-point/SIMD (only least-significant 64-bits) */
  case V8: case V9: case V10: case V11: case V12: case V13: case V14: case V15:
    return 8;

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (aarch64)\n", reg);
  return 0;
}

