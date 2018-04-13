#include <stdlib.h>
#include <stdio.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

static int max_depth = 10;
static int post_transform = 0;

void outer_frame()
{
  if(!post_transform)
  {
#ifdef __aarch64__
    TIME_AND_TEST_REWRITE("./stack_pointer_aarch64", outer_frame);
#elif defined(__powerpc64__)
    TIME_AND_TEST_REWRITE("./stack_pointer_powerpc64", outer_frame);
#elif defined(__x86_64__)
    TIME_AND_TEST_REWRITE("./prog_x86-64", outer_frame);
#endif
  }
}

void recurse(int depth, int* myvar)
{
  if(depth < max_depth) recurse(depth + 1, myvar);
  else outer_frame();
  (*myvar)++;
}

int main(int argc, char** argv)
{
  int myvar = 0;

  if(argc > 1)
    max_depth = atoi(argv[1]);

  recurse(1, &myvar);
  printf("myvar = %d\n", myvar);
  return (myvar == 0 ? 1 : 0);
}

