/**
 * String definitions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 4/19/2016
 */

#include "retval.h"

const char *ret_t_str[] = {
#define X(a, b) b,
	RETURN_TYPES
#undef X
};

