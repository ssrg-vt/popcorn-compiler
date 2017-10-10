#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* Users can override output data file by setting this environment variable. */
#define ENV_ABORT_PROF_FILE "ABORT_PROF_FN"

/* The LLVM pass *must* insert definitions for these */
extern uint64_t __abort_counters[];
extern uint32_t __num_abort_counters;

void __attribute__((destructor)) __dump_abort_loc_ctrs() {
  const char *fn = "htm-abort.ctr", *env;
  if((env = getenv(ENV_ABORT_PROF_FILE))) fn = env;
  FILE *fp = fopen(fn, "w");
  if(!fp) {
    fprintf(stderr, "WARNING: couldn't open '%s' to write"
                    "HTM abort counter data!\n", fn);
    return;
  }

  printf(" [ Printing %u counters to '%s' ]\n", __num_abort_counters, fn);

  uint32_t i;
  for(i = 0; i < __num_abort_counters; i++) {
    if(!fprintf(fp, "%lu ", __abort_counters[i])) {
      fprintf(stderr, "WARNING: couldn't write HTM abort counter data!\n");
      return;
    }
  }

  fclose(fp);
}

