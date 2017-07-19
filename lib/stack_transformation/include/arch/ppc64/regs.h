/*
 * Register definitions and macros for access for ppc64.
 *
 * DWARF register number to name mappings are derived from the ARM DWARF
 * documentation:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0057b/IHI0057B_aadwarf64.pdf
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/17/2015
 */

#ifndef _REGS_PPC64_H
#define _REGS_PPC64_H

///////////////////////////////////////////////////////////////////////////////
// ppc64 structure definitions
///////////////////////////////////////////////////////////////////////////////

/*
 * Defines an abstract register set for the ppc64 ISA, used for finding data
 * and virtually unwinding the stack.  Laid out to be compatible with kernel's
 * struct pt_regs for ppc64
 */
struct regset_ppc64
{
  unsigned long gpr[32];
  unsigned long nip;
  unsigned long msr;
  unsigned long orig_gpr3;    /* Used for restarting system calls */
  unsigned long ctr;
  unsigned long link;
  unsigned long xer;
  unsigned long ccr;
  unsigned long softe;        /* Soft enabled/disabled */
};

#endif /* _REGS_PPC64_H */
