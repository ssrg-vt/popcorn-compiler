/*
 * verifyMerge.c
 *
 * Verify the sequences that were merged by mergeScores.
 *
 * Russ Brown
 */

/* @(#)verifyMergeAlignment.c	1.5 06/11/27 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sequenceAlignment.h"

/*
 * Function verifyMergeAlignment - Kernel 2 - Verify alignments that
 * remain after merging the alignments from different threads.
 *
 * INPUT
 * simMatrix        - the simMatrix structure
 * C
 *   numReports     - [integer] number of reports
 *   finalStartsI   - [reports][integer] main startpoints
 *   finalStartsJ   - [reports][integer] match startpoints
 *   finalEndsI     - [reports][integer] main endpoints
 *   finalEndsJ     - [reports][integer] match endpoints
 *   finalSeqsI     - [reports][unsigned char] main sequences
 *   finalSeqsJ     - [reports][unsigned char] match sequences
 *   finalScores    - [reports][long long] scores for reports
 * maxDisplay       - [integer] number of reports to display to the console
 */

void verifyMergeAlignment(SIMMATRIX_T *simMatrix, CSTR_T *C, int maxDisplay) {

  int c, r, numReports, displayReports, mainMatch, matchMatch, myTaskID;
  char *main, *match;
  long long score;

  /* Get the MPI task ID (default is 0). */

#ifdef MPI
  if ( MPI_Comm_rank(MPI_COMM_WORLD, &myTaskID) != MPI_SUCCESS ) {
    printf("verifyMergeAlignment: cannot get myTaskID\n");
    MPI_Finalize();
    exit (1);
  }
#else
  myTaskID = 0;
#endif

  /* Print a report for process 0 because for MPI only that report exists. */

  if (myTaskID == 0) {

    /* Get the number of reports and calculate the number to display. */

    numReports = C->numReports;
    displayReports = MIN(maxDisplay, numReports);

    if (numReports == 0) {
      printf("\n*** Found no acceptable alignments. ***\n");
    } else {
#if !defined(SPEC)
      printf("\n*** Found %d alignments with scores from %lld to %lld ***\n",
	     numReports, C->finalScores[1], C->finalScores[numReports]);
#endif
      if (displayReports > 0) {
	if (displayReports < numReports) {
	  printf("Displaying the first %d of them.\n", displayReports);
	}
	printf("\nStarting   Amino     Codon           Ending\n");
	printf("position   acids     bases           position\n");
      }
    }

    /* For each of the finalScores reported by Kernel 2... */

    for (r = 1; r <= numReports; r++) {

      main = (char *)(C->finalSeqsI[r]);
      match = (char *)(C->finalSeqsJ[r]);

      /* Initialize mainMatch and matchMatch although Matlab doesn't. */

      mainMatch = matchMatch = 1;

      /* 
       * Recompute the alignment score in the most obvious way,
       * although this algorithm assumes that the main and match
       * sequences are of the same length.
       */

      score = 0L;
      for (c = 0; c < strlen(main); c++) {
	if (main[c] == simMatrix->hyphen) {
	  if (mainMatch) {
	    mainMatch = 0;
	    score -= simMatrix->gapStart;
	  }
	  score -= simMatrix->gapExtend;
	  continue;
	}
	if (match[c] == simMatrix->hyphen) {
	  if (matchMatch) {
	    matchMatch = 0;
	    score -= simMatrix->gapStart;
	  }
	  score -= simMatrix->gapExtend;
	  continue;
	}
	mainMatch = 1;
	matchMatch = 1;
	score += (long long)(simMatrix->
			     similarity[ (int)main[c] ] [ (int)match[c] ]);
      }

      /*
       * Check the results and report success or failure.
       * In the case of success, print maxDisplay reports.
       */

      if (score != C->finalScores[r]) {
	printf("\nverifyMergeAlignment %d failed; reported %lld vs actual %lld:",
	       r, C->finalScores[r], score);
	printf("   ---------------------------\n");

	printf("%7d  ", C->finalStartsI[r]);
	for (c = 0; c < strlen(main); c++) {
	  printf("%c", simMatrix->aminoAcid[ (int)main[c] ]);
	}
	printf("  ");
	for (c = 0; c < strlen(main); c++) {
	  printf("%s", simMatrix->codon[ (int)main[c] ]);
	}
	printf("  %7d\n", C->finalEndsI[r]);

	printf("%7d  ", C->finalStartsJ[r]);
	for (c = 0; c < strlen(match); c++) {
	  printf("%c", simMatrix->aminoAcid[ (int)match[c] ]);
	}
	printf("  ");
	for (c = 0; c < strlen(match); c++) {
	  printf("%s", simMatrix->codon[ (int)match[c] ]);
	}
	printf("  %7d\n", C->finalEndsJ[r]);
      } else if (r <= maxDisplay) {
	printf("\nverifyMergeAlignment %d, succeeded; score %lld\n",
	       r, C->finalScores[r]);

	printf("%7d  ", C->finalStartsI[r]);
	for (c = 0; c < strlen(main); c++) {
	  printf("%c", simMatrix->aminoAcid[ (int)main[c] ]);
	}
	printf("  ");
	for (c = 0; c < strlen(main); c++) {
	  printf("%s", simMatrix->codon[ (int)main[c] ]);
	}
	printf("  %7d\n", C->finalEndsI[r]);

	printf("%7d  ", C->finalStartsJ[r]);
	for (c = 0; c < strlen(match); c++) {
	  printf("%c", simMatrix->aminoAcid[ (int)match[c] ]);
	}
	printf("  ");
	for (c = 0; c < strlen(match); c++) {
	  printf("%s", simMatrix->codon[ (int)match[c] ]);
	}
	printf("  %7d\n", C->finalEndsJ[r]);
      }
    }
  }
  fflush(stdout);
}
