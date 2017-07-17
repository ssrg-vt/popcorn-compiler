/*
 * Implements a variable-sized bitmap.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 3/3/2016
 */

#include "definitions.h"
#include "bitmap.h"

#define MAX_BITMAP_SIZE UINT16_MAX

///////////////////////////////////////////////////////////////////////////////
// Bitmap operations
///////////////////////////////////////////////////////////////////////////////

bitmap bitmap_init(size_t size)
{
  bitmap new;
  ASSERT(size < MAX_BITMAP_SIZE, "requested bitmap size too large");
  new.size = size;
  new.bits = malloc(bitmap_size(size));
  memset(new.bits, 0, bitmap_size(size));
  return new;
}

void bitmap_free(bitmap bitmap)
{
  ASSERT(bitmap.bits, "invalid bitmap\n");
  free(bitmap.bits);
}

