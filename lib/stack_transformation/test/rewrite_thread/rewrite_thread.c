#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

static int max_depth = 10;
static int post_transform = 0;

int outer_frame()
{
  if(!post_transform)
  {
    printf("--> child beginning re-write <--\n");
#ifdef __aarch64__
    TIME_AND_TEST_REWRITE("./rewrite_thread_aarch64", outer_frame);
#elif defined(__powerpc64__)
    TIME_AND_TEST_REWRITE("./rewrite_thread_powerpc64", outer_frame);
#elif defined(__x86_64__)
    TIME_AND_TEST_REWRITE("./rewrite_thread_x86-64", outer_frame);
#endif
  }
  printf("--> child finished re-write <--\n");
  return rand();
}

int recurse(int depth)
{
  if(depth < max_depth) return recurse(depth + 1) + 1;
  else return outer_frame();
}

void* thread_main(void* args)
{
  recurse(1);
  return NULL;
}

int main(int argc, char** argv)
{
  pthread_t child;

  if(argc > 1)
    max_depth = atoi(argv[1]);

  if(pthread_create(&child, NULL, thread_main, NULL))
  {
    printf("Couldn't spawn child thread\n");
    exit(1);
  }

  if(pthread_join(child, NULL))
  {
    printf("Couldn't join child thread\n");
    exit(1);
  }

  return 0;
}

