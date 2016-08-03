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

  /* Callee-saved registers, only need to check these for frame unwinding. */
  const size_t num_callee_saved;
  const uint16_t* callee_saved;

  /*
   * Size of stored register contents -- the ABI may specify that only a subset
   * of register contents are to be saved (e.g. FP regs on aarch64).
   */
  const uint16_t* callee_save_size;

  /////////////////////////////////////////////////////////////////////////////
  // Functions
  /////////////////////////////////////////////////////////////////////////////

  /* Fix up the stack pointer for function-entry alignment. */
  void* (*align_sp)(void* sp);

  /* Is the register callee-saved? */
  bool (*is_callee_saved)(dwarf_reg reg);

  /* Size of a register in bytes. */
  uint16_t (*reg_size)(dwarf_reg reg);
};

typedef struct properties_t* properties_t;

#endif /* _PROPERTIES_H */

