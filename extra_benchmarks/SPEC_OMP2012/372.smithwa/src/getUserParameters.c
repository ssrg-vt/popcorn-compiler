/*
 * getUserParameters.c
 *
 * Tests the user parameters that are set in userParameters.h.
 *
 * Russ Brown
 */

/* @(#)getUserParameters.c	1.3 10/01/27 */

#include <stdio.h>
#include <stdlib.h>

#include "sequenceAlignment.h"

/*
 * WARNING: Code below this point is not to be modified by the user.
 */

void getUserParameters(void) {

  if (SIM_EXACT <= 0 || SIM_DISSIMILAR >= 0 || GAP_START < 0 || GAP_EXTEND <= 0) {
    fprintf(stderr,"Similarity parameters set in userParameters are invalid\n");
    exit(1);
  }
  if (K1_MAX_REPORTS <= 0) {
    fprintf(stderr,"Kernel 1 parameters set in userParameters are invalid\n");
  }
  if (K2_MAX_REPORTS <= 0) {
    fprintf(stderr,"Kernel 2 parameters set in userParameters are invalid\n");
  }
}
