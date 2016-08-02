/*
 * The base register set type, which provides architecture-agnostic data and
 * the functional interface.  Architecture-specific implementations will add
 * their own register sets and provide implementations of the functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/26/2016
 */

#ifndef _REGS_H
#define _REGS_H

/* Macros to convert between DW_OP_reg* and raw values */
#define OP_REG( _reg ) (_reg < 32 ? (dwarf_reg){(_reg + DW_OP_reg0), 0} : \
                                    (dwarf_reg){DW_OP_regx, _reg})
#define RAW_REG( _reg ) (_reg.reg == DW_OP_regx ? _reg.x : _reg.reg - DW_OP_reg0)

/* A DWARF register specification. */
typedef struct dwarf_reg
{
  Dwarf_Small reg;
  Dwarf_Unsigned x;
} dwarf_reg;

/* The register set class. */
struct regset_t
{
  /////////////////////////////////////////////////////////////////////////////
  // Fields
  /////////////////////////////////////////////////////////////////////////////

  /* Number of registers in the set */
  const size_t num_regs;

  /* The return address unwinding rule number */
  const size_t ra_rule;

  /* The frame base pointer unwinding rule number */
  const size_t fbp_rule;

  /* Is the return address mapped to a non-PC register? */
  const bool has_ra_reg;

  /////////////////////////////////////////////////////////////////////////////
  // Constructors/destructors
  /////////////////////////////////////////////////////////////////////////////

  /* Default constructor -- create an empty register set */
  struct regset_t* (*regset_default)(void);

  /* Create a register set from the provided register values */
  struct regset_t* (*regset_init)(const void* regs);

  /* Copy an existing register set */
  struct regset_t* (*regset_clone)(const struct regset_t* regset);

  /* Copy register data to outside struct */
  void (*regset_copyout)(const struct regset_t* regset, void* regs);

  /* Free a register set */
  void (*free)(struct regset_t* regset); /* free a register set */

  /////////////////////////////////////////////////////////////////////////////
  // Special register access
  /////////////////////////////////////////////////////////////////////////////

  /* Get the program counter value */
  void* (*pc)(const struct regset_t* regset);

  /* Get the stack pointer value */
  void* (*sp)(const struct regset_t* regset);

  /* Get the frame pointer value */
  void* (*fbp)(const struct regset_t* regset);

  /* Get the return address-mapped register's value */
  void* (*ra_reg)(const struct regset_t* regset);

  /* Set the program counter */
  void (*set_pc)(struct regset_t* regset, void* pc);

  /* Set the stack pointer */
  void (*set_sp)(struct regset_t* regset, void* sp);

  /* Set the frame pointer value */
  void (*set_fbp)(struct regset_t* regset, void* fp);

  /* Set the return-address mapped register */
  void (*set_ra_reg)(struct regset_t* regset, void* ra);

  /////////////////////////////////////////////////////////////////////////////
  // General-purpose register access
  /////////////////////////////////////////////////////////////////////////////

  /*
   * Get pointer to register, used for both reading & writing. This allows
   * a single API for registers of all sizes.
   *
   * Note: this does *NOT* return the registers contents!
   */
  void* (*reg)(struct regset_t* regset, dwarf_reg reg);
};

typedef struct regset_t* regset_t;
typedef const struct regset_t* const_regset_t;

#endif /* _REGS_H */

