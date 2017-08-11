#include <stdlib.h>
#include <stdio.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

static int max_depth = 2;
static int post_transform = 0;

double outer_frame()
{
  if(!post_transform)
  {
#if defined(__powerpc64__)
    printf("rewrite_double: power\n");
    TIME_AND_TEST_REWRITE("./rewrite_double_powerpc64", outer_frame);
#elif defined(__aarch64__)
    printf("rewrite_double: arm\n");
    TIME_AND_TEST_REWRITE("./rewrite_double_aarch64", outer_frame);
#elif defined(__x86_64__)
    printf("rewrite_double: x86\n");
    TIME_AND_TEST_REWRITE("./rewrite_double_x86-64", outer_frame);
#endif
  }
  return 1.0;
}

double recurse(int depth, double val)
{
  if(depth < max_depth) return recurse(depth + 1, val * 1.2) + val;
  else return outer_frame();
}

int main(int argc, char** argv)
{
  if(argc > 1)
    max_depth = atoi(argv[1]);
  srand(10);

  double ret = recurse(1, 1.0);
  printf("Calculated %f\n", ret);

  return 0;
}

