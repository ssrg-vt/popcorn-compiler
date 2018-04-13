#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

static int max_depth = 10;
static int post_transform = 0;

long outer_frame()
{
  if(!post_transform)
  {
#ifdef __aarch64__
    TIME_AND_TEST_REWRITE("./rewrite_many_aarch64", outer_frame);
#elif defined(__powerpc64__)
    TIME_AND_TEST_REWRITE("./rewrite_many_powerpc64", outer_frame);
#elif defined(__x86_64__)
    TIME_AND_TEST_REWRITE("./prog_x86-64", outer_frame);
#endif
  }
  return rand();
}

long recurse(int depth)
{
  int a1, a2, a3, a4, a5, a6, a7, a8, a9,
      a10, a11, a12, a13, a14, a15, a16;
  long b1, b2, b3, b4, b5, b6, b7, b8,
       b9, b10, b11, b12, b13, b14, b15, b16;
  long unsigned ret;
  

  a1 = rand() % 8;
  a2 = rand() % 8;
  a3 = rand() % 8;
  a4 = rand() % 8;
  a5 = rand() % 8;
  a6 = rand() % 8;
  a7 = rand() % 8;
  a8 = rand() % 8;
  a9 = rand();
  a10 = rand();
  a11 = rand();
  a12 = rand();
  a13 = rand();
  a14 = rand();
  a15 = rand();
  a16 = rand();
  b1 = rand() + rand();
  b2 = rand() + rand();
  b3 = rand() + rand();
  b4 = rand() + rand();
  b5 = rand() + rand();
  b6 = rand() + rand();
  b7 = rand() + rand();
  b8 = rand() + rand();
  b9 = rand() + rand();
  b10 = rand() + rand();
  b11 = rand() + rand();
  b12 = rand() + rand();
  b13 = rand() + rand();
  b14 = rand() + rand();
  b15 = rand() + rand();
  b16 = rand() + rand();

  if(depth < max_depth)
    ret = recurse(depth + 1);
  else ret = outer_frame();

  switch(ret % 8)
  {
  case 0: return a1 + a9 + b1 + b9;
  case 1: return a2 + a10 + b2 + b10;
  case 2: return a3 + a11 + b3 + b11;
  case 3: return a4 + a12 + b4 + b12;
  case 4: return a5 + a13 + b5 + b13;
  case 5: return a6 + a14 + b6 + b14;
  case 6: return a7 + a15 + b7 + b15;
  case 7: return a8 + a16 + b8 + b16;
  default: assert(0 && "Did not correctly restore stack frame"); return 0;
  }
}

int main(int argc, char** argv)
{
  if(argc > 1)
    max_depth = atoi(argv[1]);

  recurse(1);

  return 0;
}

