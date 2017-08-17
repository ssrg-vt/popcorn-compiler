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
#if defined(__powerpc64__)
    printf("verbose_unwind: power\n");
    TIME_AND_TEST_REWRITE("./verbose_unwind_powerpc64", outer_frame);
#elif defined(__aarch64__)
    printf("verbose_unwind: arm\n");
    TIME_AND_TEST_REWRITE("./verbose_unwind_aarch64", outer_frame);
#elif defined(__x86_64__)
    printf("verbose_unwind: x86\n");
    TIME_AND_TEST_REWRITE("./verbose_unwind_x86-64", outer_frame);
#endif
  }
  return rand();
}

int recurse(int depth)
{
  int retval;

  printf("Entering recurse (%d)\n", depth);
  if(depth < max_depth) retval = recurse(depth + 1) + 1;
  else retval = outer_frame();
  printf("Leaving recurse (%d)\n", depth);

  return retval;
}

int main(int argc, char** argv)
{
  if(argc > 1)
    max_depth = atoi(argv[1]);

  return recurse(1);
}

