#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <omp.h>

#define TO_NS( ts ) ((ts.tv_sec * 1000000000) + ts.tv_nsec)

static bool verbose = false;
static size_t nthreads = 8;
static size_t vecsize = 1048576;
static size_t niters = 100;

void vector_init(int *vec, size_t size) {
  size_t i;
  #pragma omp parallel for
  for(i = 0; i < size; i++)
    vec[i] = rand() % 256;
}

int vector_reduce(int *vec, size_t size) {
  size_t i;
  int reduced = 0;
  #pragma omp parallel for reduction(+:reduced)
  for(i = 0; i < size; i++) {
    reduced += vec[i];
    vec[i] = rand() % 256;
  }
  return reduced;
}

int main(int argc, char **argv) {
  size_t i;
  int c, ret, *vec;
  struct timespec start, iter_start, end;

  while((c = getopt(argc, argv, "t:s:i:vh")) != -1) {
    switch(c) {
    case 't': nthreads = strtoul(optarg, NULL, 10); break;
    case 's': vecsize = strtoul(optarg, NULL, 10); break;
    case 'i': niters = strtoul(optarg, NULL, 10); break;
    case 'v': verbose = true; break;
    case 'h':
    default:
      printf("Usage: vector_reduce -t THREADS -s VECSIZE -i ITERS\n");
      exit(0);
    }
  }

  omp_set_num_threads(nthreads);
#ifdef _ALIGN_LAYOUT
  c = posix_memalign(&vec, 4096, sizeof(int) * vecsize);
  if(c) {
    perror("Could not allocate aligned vector\n");
    exit(1);
  }
#else
  vec = (int *)malloc(sizeof(int) * vecsize);
  if(!vec) {
    fprintf(stderr, "Could not allocate vector\n");
    exit(1);
  }
#endif
  vector_init(vec, vecsize);

  clock_gettime(CLOCK_MONOTONIC, &start);
  for(i = 0; i < niters; i++) {
    if(verbose) clock_gettime(CLOCK_MONOTONIC, &iter_start);
    ret = vector_reduce(vec, vecsize);
    if(verbose) {
      clock_gettime(CLOCK_MONOTONIC, &end);
      printf("Iteration %lu: %lu ns\n", i, TO_NS(end) - TO_NS(iter_start));
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("Computation took %lu ns\n", TO_NS(end) - TO_NS(start));

  free(vec);

  return ret;
}

