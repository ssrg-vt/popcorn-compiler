#include <stdlib.h>
#include <stdio.h>
//#include <assert.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

static int max_depth = 10;
static int post_transform = 0;

long outer_frame()
{
  if(!post_transform)
  {
#ifdef __aarch64__
    TIME_AND_TEST_REWRITE("./rewrite_copy_aarch64", outer_frame);
#elif defined(__powerpc64__)
    TIME_AND_TEST_REWRITE("./rewrite_copy_powerpc64", outer_frame);
#elif defined(__x86_64__)
    TIME_AND_TEST_REWRITE("./prog_x86-64", outer_frame);
#endif
  }
  return rand();
}

long recurse(int depth, int rand1, int rand2, int rand3, int rand4)
{
  int all, of, these, variables, are, in, use, now, ret;

  all = (rand() + rand1) % 8;
  of = (rand() + rand2) % 8;
  these = (rand() + rand3) % 8;
  variables = (rand() + rand4) % 8;
  are = (rand() + rand1) % 8;
  in = (rand() + rand2) % 8;
  use = (rand() + rand3) % 8;
  now = (rand() + rand4) % 8;

  printf("Before values: %d %d %d %d %d %d %d %d\n",
         all, of, these, variables, are, in, use, now);

  if(depth < max_depth)
    ret = recurse(depth + 1,
                  all + of,
                  these + variables,
                  are + in,
                  use + now);
  else ret = outer_frame();
  ret %= 8;

  printf("After values: %d %d %d %d %d %d %d %d\n",
         all, of, these, variables, are, in, use, now);

  switch(ret)
  {
  case 0: return all;
  case 1: return of;
  case 2: return these;
  case 3: return variables;
  case 4: return are;
  case 5: return in;
  case 6: return use;
  case 7: return now;
 // default: assert(0 && "Did not correctly restore stack frame"); return 0;
  }
}

int main(int argc, char** argv)
{
  if(argc > 1)
    max_depth = atoi(argv[1]);

  return recurse(1, rand(), rand(), rand(), rand());
}

