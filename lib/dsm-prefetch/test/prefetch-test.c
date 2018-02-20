#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dsm-prefetch.h"
#include "platform.h"

static char __attribute__((aligned(PAGESZ))) data[20][PAGESZ];

#define CHECK_NUM_REQUESTS( nid, type, num ) \
  ({ \
    size_t num_requests = popcorn_prefetch_num_requests(nid, type); \
    if(num_requests == num) { \
      printf("Passed: got %d request(s) (%s:%d)\n", \
             num, __FILE__, __LINE__); \
    } \
    else { \
      printf("\nERROR: invalid number of requests -- " \
             "expected %d but got %lu (%s:%d)\n", \
             num, num_requests, __FILE__, __LINE__); \
      exit(1); \
    } \
  })

int main()
{
  // Add some read requests for node 0
  popcorn_prefetch_node(0, READ, data[0], data[3]);
  popcorn_prefetch_node(0, READ, data[8], data[11]);
  popcorn_prefetch_node(0, READ, data[16], data[19]);
  CHECK_NUM_REQUESTS(0, READ, 3);

  // Add requests that merge with previous & next nodes
  popcorn_prefetch_node(0, READ, data[6], data[9]);
  popcorn_prefetch_node(0, READ, data[14], data[17]);
  CHECK_NUM_REQUESTS(0, READ, 3);

  // Add request that merge both previous & next nodes
  popcorn_prefetch_node(0, READ, data[11], data[14]);
  CHECK_NUM_REQUESTS(0, READ, 2);

  popcorn_prefetch_execute_node(0);
  CHECK_NUM_REQUESTS(0, READ, 0);

  // Add both read & write requests to the same node
  popcorn_prefetch_node(0, READ, data[0], data[1]);
  popcorn_prefetch_node(0, WRITE, data[2], data[3]);
  CHECK_NUM_REQUESTS(0, READ, 1);
  CHECK_NUM_REQUESTS(0, WRITE, 1);

  // Add requests for a different node
  popcorn_prefetch_node(1, READ, data[0], data[1]);
  popcorn_prefetch_node(1, WRITE, data[3], data[4]);
  CHECK_NUM_REQUESTS(0, READ, 1);
  CHECK_NUM_REQUESTS(0, WRITE, 1);
  CHECK_NUM_REQUESTS(1, READ, 1);
  CHECK_NUM_REQUESTS(1, WRITE, 1);

  popcorn_prefetch_execute_node(0);
  popcorn_prefetch_execute_node(1);
  CHECK_NUM_REQUESTS(0, READ, 0);
  CHECK_NUM_REQUESTS(0, WRITE, 0);
  CHECK_NUM_REQUESTS(1, READ, 0);
  CHECK_NUM_REQUESTS(1, WRITE, 0);

  printf("\nSUCCESS - All tests passed!\n");

  return 0;
}

