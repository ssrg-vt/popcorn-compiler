#include <stdlib.h>
#include <stdio.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

extern uint64_t get_magic(void);
extern uint64_t get_magic_a(void);
extern uint64_t get_magic_b(void);
static int max_depth = 10;
static int post_transform = 0;

int outer_frame()
{
  if(!post_transform)
  {
#ifdef __aarch64__
    TIME_AND_TEST_REWRITE("./prog_aarch64", outer_frame);
#elif defined(__powerpc64__)
    TIME_AND_TEST_REWRITE("./callee_saved_powerpc64", outer_frame);
#elif defined(__x86_64__)
    TIME_AND_TEST_REWRITE("./prog_x86-64", outer_frame);
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
  // Note: clang/LLVM ignores the register specification, but by default will
  // allocate live values to callee-saved registers first anyway
#ifdef __aarch64__
  register uint64_t magic __asm__("x19");
#elif defined(__powerpc64__)
  register uint64_t magic __asm__("r30");
#elif defined(__x86_64__)
  register uint64_t magic __asm__("rbx");
#endif

  if(argc > 1)
    max_depth = atoi(argv[1]);

  magic = get_magic_a();
  recurse(1);
  magic |= get_magic_b();

  printf("Expected %lx, got %lx\n", get_magic(), magic);
  return 0;
}

