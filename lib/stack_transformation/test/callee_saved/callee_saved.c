#include <stdlib.h>
#include <stdio.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

extern uint64_t get_magic(void);
extern uint64_t get_magic_a(void);
extern uint64_t get_magic_b(void);
static int post_transform = 0;

void outer_frame()
{
  if(!post_transform)
  {
#ifdef __aarch64__
    TIME_AND_TEST_REWRITE("./callee_saved_aarch64", outer_frame);
#elif defined(__powerpc64__)
    TIME_AND_TEST_REWRITE("./callee_saved_powerpc64", outer_frame);
#elif defined(__x86_64__)
    TIME_AND_TEST_REWRITE("./callee_saved_x86-64", outer_frame);
#endif
  }
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

  magic = get_magic_a();
  outer_frame();
  magic |= get_magic_b();

  printf("Expected %lx, got %lx\n", get_magic(), magic);
  return 0;
}

