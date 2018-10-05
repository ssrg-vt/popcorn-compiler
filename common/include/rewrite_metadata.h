/**
 * Call-site metadata used for frame re-writing.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2016
 */

/*
 * NOTE: PLEASE READ ME FIRST!
 *
 * If you change the format of any of the structs below (or any of the raw
 * stackmaps emitted by LLVM) you *must* recompile & reinstall any libraries
 * that require stackmaps including (but not limited to):
 *
 *   libc (musl-libc)
 *   libmigrate (migration)
 *   libopenpop
 *
 * NOTE 2: the metadata sections emitted by LLVM are set up to be 4-byte
 * aligned.  Make sure the sizes of following metadata structures are a
 * multiple of 4:
 *
 *   function_record
 *   stack_slot
 *   unwind_loc
 */

#ifndef _REWRITE_METADATA_H
#define _REWRITE_METADATA_H

#include <stdint.h>

#define POPCORN_PACKED __attribute__((__packed__))

/* Reference to another section */
typedef struct POPCORN_PACKED section_ref {
  uint16_t num; /* Number of contiguous entries */
  uint64_t offset; /* Offset into section */
} section_ref;

/*
 * Function address, code size, on-stack size and references to function
 * activation metadata contained in other sections.
 */
typedef struct POPCORN_PACKED function_record {
  uint64_t addr; /* function address */
  uint32_t code_size; /* size of function's code */
  uint32_t frame_size; /* size of the stack frame */
  section_ref unwind; /* reference to unwinding entries */
  section_ref stack_slot; /* reference to stack slots */
} function_record;

/* A stack slot's location, size and alignment. */
typedef struct POPCORN_PACKED stack_slot {
  uint16_t base_reg; /* base register from which to offset */
  int16_t offset; /* offset from base register */
  uint32_t size;
  uint32_t alignment;
} stack_slot;

/* Stack location for callee-saved register. Encodes offset from FBP. */
typedef struct POPCORN_PACKED unwind_loc {
  uint16_t reg; /* which register is saved onto the stack */
  int16_t offset; /* offset from FBP where register contents were spilled */
} unwind_loc;

#define EMPTY_CALL_SITE \
  ((call_site){ \
    .id = 0, \
    .func = 0, \
    .addr = 0, \
    .live_vals = {0, 0} \
    .arch_live_vals = {0, 0} \
  })

/* Transformation metadata for a particular call site. */
typedef struct POPCORN_PACKED call_site {
  uint64_t id; /* call site ID -- maps sites across binaries */
  uint32_t func; /* index of function record */
  uint64_t addr; /* call site return address */
  section_ref live; /* reference to live values */
  section_ref arch_live; /* reference to arch-specific live values */
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
typedef struct POPCORN_PACKED live_value {
  uint8_t is_temporary : 1;
  uint8_t is_duplicate : 1;
  uint8_t is_alloca : 1;
  uint8_t is_ptr : 1;
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

/*
 * An architecture-specific live values's location & value at a call site.
 * Similar to a call_site_value, but also contains instructions for populating
 * the location.
 */
typedef struct POPCORN_PACKED arch_live_value {
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

#endif /* _REWRITE_METADATA_H */

