#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dsm-prefetch.h"

#define CHECK_NUM_REQUESTS( nid, type, num ) \
  ({ \
    size_t num_requests = prefetch_num_requests(nid, type); \
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
  prefetch_node(0, READ, (void*)0, (void*)20);
  prefetch_node(0, READ, (void*)30, (void*)35);
  prefetch_node(0, READ, (void*)40, (void*)60);
  CHECK_NUM_REQUESTS(0, READ, 3);

  // Add requests that merge with previous & next nodes
  prefetch_node(0, READ, (void*)18, (void*)25);
  prefetch_node(0, READ, (void*)39, (void*)42);
  CHECK_NUM_REQUESTS(0, READ, 3);

  // Add request that merge both previous & next nodes
  prefetch_node(0, READ, (void*)22, (void*)32);
  CHECK_NUM_REQUESTS(0, READ, 2);

  prefetch_execute(0);
  CHECK_NUM_REQUESTS(0, READ, 0);

  // Add both read & write requests to the same node
  prefetch_node(0, READ, (void*)0, (void*)20);
  prefetch_node(0, WRITE, (void*)0, (void*)20);
  CHECK_NUM_REQUESTS(0, READ, 1);
  CHECK_NUM_REQUESTS(0, WRITE, 1);

  // Add requests for a different node
  prefetch_node(1, READ, (void*)0, (void*)20);
  prefetch_node(1, WRITE, (void*)0, (void*)20);
  CHECK_NUM_REQUESTS(0, READ, 1);
  CHECK_NUM_REQUESTS(0, WRITE, 1);
  CHECK_NUM_REQUESTS(1, READ, 1);
  CHECK_NUM_REQUESTS(1, WRITE, 1);

  prefetch_execute(0);
  prefetch_execute(1);
  CHECK_NUM_REQUESTS(0, READ, 0);
  CHECK_NUM_REQUESTS(0, WRITE, 0);
  CHECK_NUM_REQUESTS(1, READ, 0);
  CHECK_NUM_REQUESTS(1, WRITE, 0);

  printf("\nSUCCESS - All tests passed!\n");

  return 0;
}

