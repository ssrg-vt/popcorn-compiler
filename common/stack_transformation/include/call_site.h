/**
 * Call-site metadata used for frame re-writing.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2016
 */

#ifndef _CALL_SITE_H
#define _CALL_SITE_H

#include <stdint.h>

/*
 * Function address & starting offset in unwind information section.  Used for
 * finding unwinding information for PCs which do not directly correspond to a
 * call site.
 */
typedef struct __attribute__((__packed__)) unwind_addr {
  uint64_t addr; /* function address */
  uint32_t num_unwind;
  uint32_t unwind_offset; /* offset into unwind info section */
} unwind_addr;

/* Stack location for callee-saved register. Encodes offset from FBP. */
// Note: these records are referenced both by unwind_addr and call_site
// structures
typedef struct __attribute__((__packed__)) unwind_loc {
  uint16_t reg; /* which register is saved onto the stack */
  int16_t offset; /* offset from FBP where register contents were spilled */
} unwind_loc;

// TODO currently we encode some function-specific information into the call
// site record (e.g., fbp_offset).  This information is duplicated when there
// are multiple stack maps in a given function.  Should we offload fbp_offset,
// num_unwind & unwind_offset into a separate section and store an index into
// that section here?  It may adversely affect cache behavior in terms of
// pointer chasing but will reduce the size of these records.

#define EMPTY_CALL_SITE \
  ((call_site){ \
    .id = 0, \
    .addr = 0, \
    .frame_size = 0, \
    .num_unwind = 0, \
    .unwind_offset = 0, \
    .num_live = 0, \
    .live_offset = 0, \
    .num_arch_live = 0, \
    .arch_live_offset = 0, \
    .padding = UINT16_MAX \
  })

typedef struct __attribute__((__packed__)) call_site {
  uint64_t id; /* call site ID -- maps sites across binaries */
  uint64_t addr; /* call site return address */
  uint32_t frame_size; /* size of the stack frame */
  uint16_t num_unwind; /* number of registers saved by the function */
  uint64_t unwind_offset; /* beginning of unwinding info records in unwind info section */
  uint16_t num_live; /* number of live values at site */
  uint64_t live_offset; /* beginning of live value location records in live value section */
  uint16_t num_arch_live; /* number of arch-specific live values at site */
  uint64_t arch_live_offset; /* beginning of arch-specific live value records in section */
  uint16_t padding; /* Make 4-byte aligned */
} call_site;

/* Type of location where live value lives. */
enum location_type {
  SM_REGISTER = 0x1,
  SM_DIRECT = 0x2,
  SM_INDIRECT = 0x3,
  SM_CONSTANT = 0x4,
  SM_CONST_IDX = 0x5
};

/* A location description record for a live value at a call site. */
// Note: the compiler lays out the bit fields from least-significant to
// most-significant, meaning they *must* be in the following order to adhere to
// the on-disk layout
typedef struct __attribute__((__packed__)) live_value {
  uint8_t is_duplicate : 1;
  uint8_t is_alloca : 1;
  uint8_t is_ptr : 1;
  uint8_t bit_pad : 1;
  uint8_t type : 4;
  uint8_t size;
  uint16_t regnum;
  int32_t offset_or_constant;
  uint32_t alloca_size;
} live_value;

/* Operation types for generating architecture-specific values. */
enum inst_type {
#define X(inst) inst,
#include "StackTransformTypes.def"
VALUE_GEN_INST
#undef X
};

#ifdef _DEBUG
/* Human-readable names for generating architecture-specific values. */
const char* inst_type_names[] = {
#define X(inst) #inst,
VALUE_GEN_INST
#undef X
}
#endif

/*
 * An architecture-specific live values's location & value at a call site.
 * Similar to a call_site_value, but also contains instructions for populating
 * the location.
 */
typedef struct __attribute__((__packed__)) arch_live_value {
  /* Location */
  uint8_t is_ptr : 1;
  uint8_t bit_pad : 3;
  uint8_t type : 4;
  uint8_t size;
  uint16_t regnum;
  uint32_t offset;

  /* Operation/operand */
  uint8_t operand_type : 3;
  uint8_t is_gen : 1;
  uint8_t inst_type : 4;
  uint8_t operand_size;
  uint16_t operand_regnum;
  int64_t operand_offset_or_constant;
} arch_live_value;

#endif /* _CALL_SITE_H */

