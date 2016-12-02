/*
 * Per-architecture stack properties.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 2/18/2016
 */

#ifndef _PROPERTIES_H
#define _PROPERTIES_H

struct properties_t
{
  /////////////////////////////////////////////////////////////////////////////
  // Fields
  /////////////////////////////////////////////////////////////////////////////

  /* Does the stack pointer need a specific alignment upon function entry? */
  const bool sp_needs_align;

  /* Callee-saved registers */
  const size_t num_callee_saved;
  const uint16_t* callee_saved;

  /*
   * Size of callee-saved registers saved on the stack.
   *
   * The ABI may specify only a subset of register contents are to be saved
   * (e.g. FP regs on aarch64).  Sizes match directly with the callee_saved
   * array above.
   */
  const uint16_t* callee_saved_size;

  /* Offset from CFA for return address & saved FBP */
  const int32_t ra_offset;
  const int32_t savedfbp_offset;

  /* Offset of CFA from FBP */
  const int32_t cfa_offset;

  /* Offset of CFA from SP (upon function entry) */
  const int32_t cfa_offset_funcentry;

  /////////////////////////////////////////////////////////////////////////////
  // Functions
  /////////////////////////////////////////////////////////////////////////////

  /* Fix up the stack pointer for function-entry alignment. */
  void* (*align_sp)(void* sp);

  /* Is the register callee-saved? */
  bool (*is_callee_saved)(uint16_t reg);

  /* Size of callee-saved register spilled onto the stack. */
  uint16_t (*callee_reg_size)(uint16_t reg);
};

typedef struct properties_t* properties_t;

#endif /* _PROPERTIES_H */

