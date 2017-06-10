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

/* The register set class. */
struct regset_t
{
  // Allows the runtime to do sanity checks.  Note: we really just need a field
  // so the struct is non-empty.
  bool initialized;
};

typedef struct regset_t* regset_t;
typedef const struct regset_t* const_regset_t;

/* The register operations class. */
struct regops_t
{
  /////////////////////////////////////////////////////////////////////////////
  // Fields
  /////////////////////////////////////////////////////////////////////////////

  /* Number of registers in the set */
  const size_t num_regs;

  /* Is the return address mapped to a non-PC register? */
  const bool has_ra_reg;

  /* Size of registers in the register set */
  size_t regset_size;

  /////////////////////////////////////////////////////////////////////////////
  // Constructors/destructors
  /////////////////////////////////////////////////////////////////////////////

  /* Default constructor -- allocate & initialize an empty register set */
  regset_t (*regset_default)(void);

  /* Allocate & initialize a register set from the provided register values */
  regset_t (*regset_init)(const void* regs);

  /* Free a register set */
  void (*regset_free)(regset_t regset);

  /* Copy an existing register set. Note: does not allocate memory. */
  void (*regset_clone)(const_regset_t src, regset_t dest);

  /*
   * Copy outside struct to regset_t.  Similar to regset_init except does not
   * allocate memory.
   */
  void (*regset_copyin)(regset_t regset, const void* regs);

  /* Copy regset_t data to outside struct.  Note: does not free memory. */
  void (*regset_copyout)(const_regset_t regset, void* regs);

  /////////////////////////////////////////////////////////////////////////////
  // Special register access
  /////////////////////////////////////////////////////////////////////////////

  /* Get the program counter value */
  void* (*pc)(const_regset_t regset);

  /* Get the stack pointer value */
  void* (*sp)(const_regset_t regset);

  /* Get the frame pointer value */
  void* (*fbp)(const_regset_t regset);

  /* Get the return address-mapped register's value */
  void* (*ra_reg)(const_regset_t regset);

  /* Set the program counter */
  void (*set_pc)(regset_t regset, void* pc);

  /* Set the stack pointer */
  void (*set_sp)(regset_t regset, void* sp);

  /* Set the frame pointer value */
  void (*set_fbp)(regset_t regset, void* fp);

  /* Set the return-address mapped register */
  void (*set_ra_reg)(regset_t regset, void* ra);

  /* Architecture-specific frame base pointer setup */
  void (*setup_fbp)(regset_t regset, void* cfa);

  /////////////////////////////////////////////////////////////////////////////
  // General-purpose register access
  /////////////////////////////////////////////////////////////////////////////

  /* Size of a register in bytes. */
  uint16_t (*reg_size)(uint16_t reg);

  /*
   * Get pointer to register, used for both reading & writing. This allows
   * a single API for registers of all sizes.
   *
   * Note: this does *NOT* return the register's contents!
   */
  void* (*reg)(regset_t regset, uint16_t reg);
};

typedef struct regops_t* regops_t;
typedef const struct regops_t* const_regops_t;

#endif /* _REGS_H */

