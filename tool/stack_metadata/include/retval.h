/**
 * Return type declarations & definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 5/27/2016
 */

#ifndef _RETVAL_H
#define _RETVAL_H

/* Return types, including enumeration value & human-readable string */
#define RETURN_TYPES \
  X(SUCCESS = 0, "success") \
  X(INVALID_ARGUMENT, "invalid arguments") \
  X(INVALID_ARCHITECTURE, "invalid architecture") \
  X(INVALID_ELF_VERSION, "invalid ELF version") \
  X(OPEN_FILE_FAILED, "opening file failed") \
  X(INVALID_ELF, "invalid ELF") \
  X(OPEN_ELF_FAILED, "opening ELF failed") \
  X(LAYOUT_CONTROL_FAILED, "cannot control ELF output layout") \
  X(READ_ELF_FAILED, "reading ELF information failed") \
  X(FIND_SECTION_FAILED, "could not find ELF section") \
  X(WRITE_ELF_FAILED, "writing ELF information failed") \
  X(ADD_SECTION_FAILED, "adding section to binary failed") \
  X(UPDATE_SECTION_FAILED, "updating section in binary failed") \
  X(CREATE_METADATA_FAILED, "creating metadata failed") \
  X(INVALID_METADATA, "invalid metadata")

/* Return type enumeration */
typedef enum _ret_t {
#define X(a, b) a,
  RETURN_TYPES
#undef X
} ret_t;

/* Return type strings */
extern const char *ret_t_str[];

#endif /* _RETVAL_H */

