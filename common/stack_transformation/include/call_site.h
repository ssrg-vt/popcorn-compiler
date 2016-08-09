/**
 * Call-site metadata used for frame re-writing.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2016
 */

#ifndef _CALL_SITE_H
#define _CALL_SITE_H

#include <stdint.h>

#if _LIVE_VALS == DWARF_LIVE_VALS

#define EMPTY_CALL_SITE \
  ((call_site){ \
    .id = 0, \
    .addr = 0, \
    .fbp_offset = 0, \
    .has_fbp = 0 \
  })

/*
 * A call-site record containing metadata for finding & rewriting a stack frame
 * at the given call site.
 */
typedef struct __attribute__((__packed__)) call_site {
  uint64_t id; /* call site ID -- maps sites across binaries */
  uint64_t addr; /* call site return address */
  uint32_t fbp_offset; /* frame pointer offset from top of stack */
  uint8_t has_fbp; /* does this frame have a frame pointer? */
} call_site;

#else /* STACKMAP_LIVE_VALS */

#define EMPTY_CALL_SITE \
  ((call_site){ \
    .id = 0, \
    .addr = 0, \
    .fbp_offset = 0, \
    .num_live = 0, \
    .live_offset = 0 \
  })

typedef struct __attribute__((__packed__)) call_site {
  uint64_t id; /* call site ID -- maps sites across binaries */
  uint64_t addr; /* call site return address */
  uint32_t fbp_offset; /* frame pointer offset from top of stack */
  uint32_t num_live; /* number of live values at site */
  uint64_t live_offset; /* beginning of live value location records in live value section */
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
typedef struct __attribute__((__packed__)) call_site_value {
  uint8_t type;
  uint8_t size;
  uint16_t regnum;
  int32_t offset_or_constant;
  uint8_t is_ptr;
  uint8_t is_alloca;
  uint8_t is_duplicate;
  uint8_t padding;
  uint32_t pointed_size;
} call_site_value;

#endif

#endif /* _CALL_SITE_H */

