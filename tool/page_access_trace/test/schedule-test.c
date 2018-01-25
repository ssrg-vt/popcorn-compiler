/**
 * Generate predictable page access patterns in order to sanity check the page
 * analyis trace & thread placement framework.  There are several access
 * patterms implemented, but each pattern has 4 threads sharing an individual
 * page and hence those 4 threads should be placed together by the partitioning
 * algorithm.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 1/24/2018
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <omp.h>
#include <sys/sysinfo.h>

#define XSTR( x ) STR(x)
#define STR( x ) #x

/*
 * Knobs controlling length of OpenMP sections & memory used.  The number of
 * iterations can be set at command line.
 */
#define ITERS 2048
#define PAGES 1024

/* Size definitions */
#define PAGESZ 4096
#define INTS_PER_PAGE (PAGESZ / sizeof(int))
#define ARRSIZE (PAGES * INTS_PER_PAGE)
#define CHUNKSZ (INTS_PER_PAGE / 4)

#define NS( ts ) ((ts.tv_sec * 1000000000LU) + ts.tv_nsec)

typedef int page_array_t [ARRSIZE];
static page_array_t thearray;

#define helptext \
"Generate a predictable page access pattern to sanity check the thread\
 placement framework.\n\n\
Usage: thread-schedule [ OPTIONS ]\n\
Options:\n\
  -h     : print help & exit\n\
  -i num : number of iterations to run each access pattern (default: " XSTR(ITERS) ")\n\
  -t num : number of threads to use\n"

void parse_args(int argc, char** argv, size_t *threads, size_t *iters) {
  int c;

  while((c = getopt(argc, argv, "hi:t:")) != -1) {
    switch(c) {
    default: printf("WARNING: Ignoring unknown argument '%c'\n", c); break;
    case 'h': printf(helptext); exit(0); break;
    case 'i': *iters = atoi(optarg); break;
    case 't': *threads = atoi(optarg); break;
    }
  }
}

void randomize(page_array_t array) {
  size_t i;
  #pragma omp parallel for
  for(i = 0; i < ARRSIZE; i++) array[i] = rand() % 1024;
}

/*
 * Pattern 1: groups of 4 consecutive threads should be mapped to the same
 * node.
 */
void add1(page_array_t array, const size_t iters) {
  size_t iter, i;

  printf("Region 1: consecutive threads access the same page...\n");

  // TODO make this region 1
  for(iter = 0; iter < iters; iter++) {
    #pragma omp parallel for schedule(static, CHUNKSZ)
    for(i = 0; i < ARRSIZE; i++) array[i] += 1;
  }
}

/*
 * Pattern 2: threads with the same parity should be mapped to the same node
 * (e.g., evens with evens, odds with odds).
 */
void add2(page_array_t array, const size_t iters) {
  size_t iter, i;

  printf("Region 2: threads with the same parity access the same page...\n");

  // TODO make this region 2
  for(iter = 0; iter < iters; iter++) {
    #pragma omp parallel
    {
      long offset;
      int thread = omp_get_thread_num() % 8;
      if(thread % 2) offset = (4 + (thread / 2) - thread) * CHUNKSZ;
      else offset = -((thread / 2) * CHUNKSZ);

      #pragma omp for schedule(static, CHUNKSZ)
      for(i = 0; i < ARRSIZE; i++) array[i + offset] += 2;
    }
  }
}

int main(int argc, char** argv) {
  size_t threads = get_nprocs_conf(), iters = ITERS;
  struct timespec start, end;

  parse_args(argc, argv, &threads, &iters);
  omp_set_num_threads(threads);
  randomize(thearray);

  printf("--------------------\nTHREAD SCHEDULE TEST\n--------------------\n");
  printf("Running %lu iterations with %lu threads...\n", iters, threads);

  clock_gettime(CLOCK_MONOTONIC, &start);
  add1(thearray, iters);
  add2(thearray, iters);
  clock_gettime(CLOCK_MONOTONIC, &end);

  printf("Total execution time: %lu ms\n", (NS(end) - NS(start)) / 1000000);
}

