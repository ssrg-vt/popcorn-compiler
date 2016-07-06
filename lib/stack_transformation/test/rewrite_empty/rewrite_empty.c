#include <stdlib.h>
#include <stdio.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

static int max_depth = 10;
static int post_transform = 0;

int outer_frame()
{
  if(!post_transform)
  {
#ifdef __aarch64__
    TIME_AND_TEST_REWRITE("./rewrite_empty_aarch64", outer_frame);
#elif defined(__x86_64__)
    TIME_AND_TEST_REWRITE("./rewrite_empty_x86-64", outer_frame);
#endif
  }
  return rand();
}

int recurse(int depth)
{
  if(depth < max_depth) return recurse(depth + 1) + 1;
  else return outer_frame();
}

int main(int argc, char** argv)
{
  if(argc > 1)
    max_depth = atoi(argv[1]);

  return recurse(1);
}

