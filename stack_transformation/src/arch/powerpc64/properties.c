/*
 * powerpc64 stack properties.
 *
 * Author: Buse Yilmaz <busey@vt.edu>
 * Date: 05/11/2017
 */

#include "definitions.h"
#include "arch/powerpc64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define POWERPC64_RA_OFFSET 0x10
#define POWERPC64_SAVED_FBP_OFFSET -0x8
#define POWERPC64_CFA_OFFSET_FUNCENTRY 0x0

#define POWERPC64_STACK_ALIGNMENT 0x8
#define POWERPC64_SP_FIXUP 0x8

// TODO: LR is not documented to be callee-saved in the ABI (Rev 1.4 March,21 2017
// But it's saved by popcorn-clang 3.7
// CR2-CR4 is Callee-Saved (defined by the ABI) but not supported in this implementation
static const uint16_t callee_saved_powerpc64[] = {
  /* General-purpose */
  R1, R2, R14, R15, R16, R17, R18, R19, R20, R21, R22, R23, R24, R25, R26, R27, R28, R29, R30, R31, LR,

  /* Floating-point/SIMD (only least-significant 64-bits) */
  F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25, F26, F27, F28, F29, F30, F31
};

static const uint16_t callee_saved_size_powerpc64[] = {
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* General-purpose */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8        /* Floating-point/SIMD (only 64-bits) */
};

static void* align_sp_powerpc64(void* sp);
static bool is_callee_saved_powerpc64(uint16_t reg);
static uint16_t callee_reg_size_powerpc64(uint16_t reg);

/* powerpc64 properties */
const struct properties_t properties_powerpc64 = {
  .sp_needs_align = true,
  .num_callee_saved = sizeof(callee_saved_powerpc64) / sizeof(uint16_t),
  .callee_saved = callee_saved_powerpc64,
  .callee_saved_size = callee_saved_size_powerpc64,
  .ra_offset = POWERPC64_RA_OFFSET,
  .savedfbp_offset = POWERPC64_SAVED_FBP_OFFSET,
  .cfa_offset_funcentry = POWERPC64_CFA_OFFSET_FUNCENTRY,

  .align_sp = align_sp_powerpc64,
  .is_callee_saved = is_callee_saved_powerpc64,
  .callee_reg_size = callee_reg_size_powerpc64
};

///////////////////////////////////////////////////////////////////////////////
// powerpc64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* align_sp_powerpc64(void* sp)
{
  uint64_t stack_ptr = (uint64_t)sp;
  stack_ptr &= ~(POWERPC64_SP_FIXUP - 1);
  if(!(stack_ptr & POWERPC64_STACK_ALIGNMENT))
    stack_ptr -= POWERPC64_SP_FIXUP;
  return (void*)stack_ptr;
}

static bool is_callee_saved_powerpc64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers r1, r2, r14-r31 */
  case R1:  case R2:  case R14: case R15: case R16: case R17: 
  case R18: case R19: case R20: case R21: case R22: case R23:
  case R24: case R25: case R26: case R27: case R28: case R29:
  case R30: case R31: case LR:
    return true;

  /* Floating-point registers f14-f31 */
  case F14: case F15: case F16: case F17: case F18: case F19:
  case F20: case F21: case F22: case F23: case F24: case F25: 
  case F26: case F27: case F28: case F29: case F30: case F31:
    return true;

  default: return false;
  }
}

static uint16_t callee_reg_size_powerpc64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers r1, r2, r14-r31 */
  case R1:  case R2:  case R14: case R15: case R16: case R17: 
  case R18: case R19: case R20: case R21: case R22: case R23:
  case R24: case R25: case R26: case R27: case R28: case R29:
  case R30: case R31: case LR: case CTR:
    return 8;

  /* Floating-point/SIMD (only least-significant 64-bits) */
  case F14: case F15: case F16: case F17: case F18: case F19:
  case F20: case F21: case F22: case F23: case F24: case F25: 
  case F26: case F27: case F28: case F29: case F30: case F31:
    return 8;

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (powerpc64)\n", reg);
  return 0;
}

