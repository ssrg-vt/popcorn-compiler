/*
 * x86-64 stack properties.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/18/2016
 */

#include "definitions.h"
#include "arch/x86_64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define X86_64_RA_OFFSET -0x8
#define X86_64_SAVED_FBP_OFFSET -0x10
#define X86_64_CFA_OFFSET 0x10
#define X86_64_CFA_OFFSET_FUNCENTRY 0x8
#define X86_64_STACK_ALIGNMENT 0x10
#define X86_64_SP_FIXUP 0x8

static const uint16_t callee_saved_x86_64[] = {
  RBX, RBP, R12, R13, R14, R15, RIP
};

static const uint16_t callee_saved_size_x86_64[] = {
  8, 8, 8, 8, 8, 8, 8
};

static void* align_sp_x86_64(void* sp);
static bool is_callee_saved_x86_64(uint16_t reg);
static uint16_t callee_reg_size_x86_64(uint16_t reg);

/* x86-64 properties. */
const struct properties_t properties_x86_64 = {
  .sp_needs_align = true,
  .num_callee_saved = sizeof(callee_saved_x86_64) / sizeof(uint16_t),
  .callee_saved = callee_saved_x86_64,
  .callee_saved_size = callee_saved_size_x86_64,
  .ra_offset = X86_64_RA_OFFSET,
  .savedfbp_offset = X86_64_SAVED_FBP_OFFSET,
  .cfa_offset = X86_64_CFA_OFFSET,
  .cfa_offset_funcentry = X86_64_CFA_OFFSET_FUNCENTRY,

  .align_sp = align_sp_x86_64,
  .is_callee_saved = is_callee_saved_x86_64,
  .callee_reg_size = callee_reg_size_x86_64
};

///////////////////////////////////////////////////////////////////////////////
// x86-64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* align_sp_x86_64(void* sp)
{
  uint64_t stack_ptr = (uint64_t)sp;
  stack_ptr &= ~(X86_64_SP_FIXUP - 1);
  if(!(stack_ptr & X86_64_STACK_ALIGNMENT))
    stack_ptr -= X86_64_SP_FIXUP;
  return (void*)stack_ptr;
}

static bool is_callee_saved_x86_64(uint16_t reg)
{
  switch(reg)
  {
  case RBX: case RBP: case R12: case R13: case R14: case R15: case RIP:
    return true;
  default:
    return false;
  }
}

static uint16_t callee_reg_size_x86_64(uint16_t reg)
{
  switch(reg)
  {
  case RBX: case RBP: case R12: case R13: case R14: case R15: case RIP:
    return 8;
  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (x86-64)\n", reg);
}

