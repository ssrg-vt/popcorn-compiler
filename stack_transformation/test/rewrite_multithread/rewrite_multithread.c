#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

#include <stack_transform.h>
#include "stack_transform_timing.h"

static int num_threads = 10;
static int max_depth = 10;
static st_handle handle = NULL;
static __thread int post_transform = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int outer_frame()
{
  if(!post_transform)
  {
    pthread_mutex_lock(&lock);
    int tid = syscall(SYS_gettid);
    printf("--> Child %d beginning re-write <--\n", tid);
    TIME_AND_TEST_NO_INIT(handle, outer_frame);
  }
  else
    pthread_mutex_unlock(&lock);
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
  int i;
  pthread_t* children;
  int* child_num;

  if(!(handle = st_init(argv[0]))) {
    printf("Couldn't initialize stack transformation handle\n");
    exit(1);
  }

  if(argc > 1) max_depth = atoi(argv[1]);
  if(argc > 2) num_threads = atoi(argv[2]);
  children = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
  child_num = (int*)malloc(sizeof(int) * num_threads);

  for(i = 1; i < num_threads; i++) {
    child_num[i] = i;
    if(pthread_create(&children[i], NULL, thread_main, &child_num[i])) {
      printf("Couldn't spawn child thread\n");
      exit(1);
    }
  }

  for(i = 1; i < num_threads; i++) {
    if(pthread_join(children[i], NULL)) {
      printf("Couldn't join child thread\n");
      exit(1);
    }
  }

  free(children);
  free(child_num);
  return 0;
}

