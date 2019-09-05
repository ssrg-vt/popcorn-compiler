/*
 * riscv64 stack properties.
 *
 * Author: Cesar Philippidis
 * Date: 2/7/2020
 */

#include "definitions.h"
#include "arch/riscv64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define RISCV64_STACK_ALIGNMENT 0x10
#define RISCV64_RA_OFFSET -0x8
#define RISCV64_CFA_OFFSET_FUNCENTRY 0x0

static const uint16_t callee_saved_riscv64[] = {
  /* General-purpose */
  X8, X9, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27,

  /* Floating-point */
  F8, F9, F18, F19, F20, F21, F22, F23, F24, F25, F26, F27
};

static const uint16_t callee_saved_size_riscv64[] = {
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, /* General-purpose */
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 /* Floating-point */
};

static void* align_sp_riscv64(void* sp);
static bool is_callee_saved_riscv64(uint16_t reg);
static uint16_t callee_reg_size_riscv64(uint16_t reg);

/* riscv64 properties */
const struct properties_t properties_riscv64 = {
  .num_callee_saved = sizeof(callee_saved_riscv64) / sizeof(uint16_t),
  .callee_saved = callee_saved_riscv64,
  .callee_saved_size = callee_saved_size_riscv64,
  .ra_offset = RISCV64_RA_OFFSET,
  .cfa_offset_funcentry = RISCV64_CFA_OFFSET_FUNCENTRY,

  .align_sp = align_sp_riscv64,
  .is_callee_saved = is_callee_saved_riscv64,
  .callee_reg_size = callee_reg_size_riscv64
};

///////////////////////////////////////////////////////////////////////////////
// riscv64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* align_sp_riscv64(void* sp)
{
  /*
   * Per the Riscv64 ABI:
   *   "Additionally, at any point at which memory is accessed via SP, the
   *    hardware requires that
   *      - SP mod 16 = 0. The stack must be quad-word aligned."
   */
  return sp -
    (RISCV64_STACK_ALIGNMENT - ((uint64_t)sp % RISCV64_STACK_ALIGNMENT));
}

static bool is_callee_saved_riscv64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers x8-x9, x18-x27 */
  case X8: case X9: case X18: case X19: case X20: case X21: case X22: case X23:
  case X24: case X25: case X26: case X27:
    return true;

  /* Handle SP.  */
  case X1:
    return true;

  /* Floating-point registers f8-f9, f18-f27 */
  case F8: case F9: case F18: case F19: case F20: case F21: case F22: case F23:
  case F24: case F25: case F26: case F27:
    return true;

  default: return false;
  }
}

static uint16_t callee_reg_size_riscv64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers x8-x9, x18-x27 */
  case X8: case X9: case X18: case X19: case X20: case X21: case X22: case X23:
  case X24: case X25: case X26: case X27:
    return 8;

  /* Handle SP.  */
  case X1:
    return 8;

  /* Floating-point registers f8-f9, f18-f27 */
  case F8: case F9: case F18: case F19: case F20: case F21: case F22: case F23:
  case F24: case F25: case F26: case F27:
    return 8;

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (riscv64)\n", reg);
  return 0;
}

