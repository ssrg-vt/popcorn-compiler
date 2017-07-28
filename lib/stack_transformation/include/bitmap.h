/*
 * Variable-size bitmap.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 3/3/2016
 */

#ifndef _BITMAP_H
#define _BITMAP_H

#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// Definitions & declarations
///////////////////////////////////////////////////////////////////////////////

typedef uint64_t STORAGE_TYPE;
#define STORAGE_TYPE_BITS (sizeof(STORAGE_TYPE) * 8)

typedef struct bitmap
{
  size_t size;
  STORAGE_TYPE* bits;
} bitmap;

///////////////////////////////////////////////////////////////////////////////
// Bitmap operations
///////////////////////////////////////////////////////////////////////////////

/*
 * Initialize a bitmap to the given size with all bits set to zero.
 *
 * @param size the number of bits in the bitmap
 */
bitmap bitmap_init(size_t size);

/*
 * Free a bitmap.
 *
 * @param bitmap a bitmap object
 */
void bitmap_free(bitmap bitmap);

/*
 * Size of the bitmap array, in bytes.
 *
 * @param the number of bits in the bitmap
 */
#define bitmap_size(num) \
  (sizeof(STORAGE_TYPE) * \
  (num % STORAGE_TYPE_BITS ? (num / STORAGE_TYPE_BITS) + 1 : \
                              num / STORAGE_TYPE_BITS))

/*
 * Set a bit.
 *
 * @param bitmap a bitmap object
 * @param num the bit to set
 */
#define bitmap_set(bitmap, num) \
  { \
    ASSERT(bitmap.bits, "invalid bitmap\n"); \
    ASSERT(num < bitmap.size, "invalid bit number\n"); \
    size_t coarse = num / STORAGE_TYPE_BITS; \
    size_t fine = num % STORAGE_TYPE_BITS; \
    bitmap.bits[coarse] |= (1 << fine); \
  }

/*
 * Set all bits in the bitmap.
 *
 * @param bitmap a bitmap object
 */
#define bitmap_set_all(bitmap) \
  { \
    ASSERT(bitmap.bits, "invalid bitmap\n"); \
    memset(bitmap.bits, 0xff, bitmap_size(bitmap.size)); \
  }

/*
 * Clear a bit.
 *
 * @param bitmap a bitmap object
 * @param num the bit to clear
 */
#define bitmap_clear(bitmap, num) \
  { \
    ASSERT(bitmap.bits, "invalid bitmap\n"); \
    ASSERT(num < bitmap.size, "invalid bit number\n"); \
    size_t coarse = num / STORAGE_TYPE_BITS; \
    size_t fine = num % STORAGE_TYPE_BITS; \
    bitmap.bits &= ~(1 << fine); \
  }

/*
 * Clear all bits in the bitmap.
 *
 * @param bitmap a bitmap object
 */
#define bitmap_clear_all(bitmap) \
  { \
    ASSERT(bitmap.bits, "invalid bitmap\n"); \
    memset(bitmap.bits, 0, bitmap_size(bitmap.size)); \
  }

/*
 * Returns whether or not the specified bit is set.
 *
 * @param bitmap a bitmap object
 * @param num the bit to check
 */
#define bitmap_is_set(bitmap, num) \
  ({ \
    ASSERT(bitmap.bits, "invalid bitmap\n"); \
    ASSERT(num < bitmap.size, "invalid bit number\n"); \
    size_t coarse = num / STORAGE_TYPE_BITS; \
    size_t fine = num % STORAGE_TYPE_BITS; \
    bool is_set = bitmap.bits[coarse] & (1 << fine); \
    is_set; \
  })

#endif /* _BITMAP_H */

