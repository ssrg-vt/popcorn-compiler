#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <assert.h>
#include <omp.h>

#define NS( ts ) ((ts.tv_sec * 1000000000) + ts.tv_nsec)
static size_t nthreads = 8;
static size_t iters = 1000;

void parse_args(int argc, char **argv)
{
  int c;
  while((c = getopt(argc, argv, "ht:i:")) != -1)
  {
    switch(c)
    {
    case 't': nthreads = atoi(optarg); break;
    case 'i': iters = atoi(optarg); break;
    case 'h':
      printf("Usage: %s -t THREADS -i ITERS\n", argv[0]);
      exit(0);
      break;
    }
  }
  assert(nthreads > 1 && "Please specify > 1 thread");
  assert(iters > 0 && "Please specify > 0 iterations");
  printf("Running %lu barriers with %lu threads\n", iters, nthreads);
}

int main(int argc, char** argv)
{
  struct timespec start, end;

  parse_args(argc, argv);
  omp_set_num_threads(nthreads);
  clock_gettime(CLOCK_MONOTONIC, &start);
  #pragma omp parallel shared(iters)
  {
    size_t i;
    for(i = 0; i < iters; i++)
    {
      #pragma omp barrier
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("Took %lu ns\n", NS(end) - NS(start));
  return 0;
}
