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
  size_t num_callee_saved;
  uint64_t* callee_saved;

  /////////////////////////////////////////////////////////////////////////////
  // Functions
  /////////////////////////////////////////////////////////////////////////////

  /* Fix up the stack pointer for function-entry alignment. */
  void* (*align_sp)(void* sp);

  /* Is the register callee-saved? */
  bool (*is_callee_saved)(dwarf_reg reg);

  /* Is the register a non-standard-sized register? */
  bool (*is_ext_reg)(dwarf_reg reg);
};

typedef struct properties_t* properties_t;

#endif /* _PROPERTIES_H */

