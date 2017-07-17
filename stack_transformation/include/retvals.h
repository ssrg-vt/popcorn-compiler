/*
 * Library return values.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 10/26/2015
 */

#ifndef _RETVALS_H
#define _RETVALS_H

#define RETVALS \
  X(SUCCESS = 0, "success")

enum st_retval {
#define X(a, b) a,
  RETVALS
#undef X
};

#endif /* _RETVALS_H */

