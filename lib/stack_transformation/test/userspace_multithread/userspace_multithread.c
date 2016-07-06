#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <stack_transform.h>

static int max_depth = 10;
static int num_threads = 8;

static __thread int thread_num;

#pragma popcorn
int outer_frame()
{
  printf("--> Child %d reached outer frame <--\n", thread_num);
  return rand();
}

int recurse(int depth)
{
  if(depth < max_depth) return recurse(depth + 1) + 1;
  else return outer_frame();
}

void* thread_main(void* args)
{
  thread_num = *(int*)args;
  recurse(1);
  return NULL;
}

int main(int argc, char** argv)
{
  int i;
  pthread_t *children;
  int *child_num;

  thread_num = 0;
  if(argc > 1) max_depth = atoi(argv[1]);
  if(argc > 2) num_threads = atoi(argv[2]);

  children = malloc(sizeof(pthread_t) * num_threads);
  child_num = malloc(sizeof(pthread_t) * num_threads);

  for(i = 1; i < num_threads; i++) {
    child_num[i] = i;
    if(pthread_create(&children[i], NULL, thread_main, &child_num[i])) {
      printf("Couldn't spawn child thread\n");
      exit(1);
    }
  }

  recurse(1);

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

