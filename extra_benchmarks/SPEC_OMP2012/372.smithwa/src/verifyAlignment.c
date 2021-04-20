/*
 * verifyAlignment.c
 *
 * Verify the sequences that were found by pairwiseAlign and scanBackward.
 *
 * Russ Brown
 */

/* @(#)verifyAlignment.c	1.36 06/10/20 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sequenceAlignment.h"

/*
 * Function verifyAlignment() - Kernel 2 - Verify alignments that were
 * found by different threads..
 *
 * This functions rescores each match to verify that all reported matches
 * have the reported score.  It does not verify that they are the best
 * match.
 *
 * For a detailed description of the SSCA #1 Optimal Pattern Matching problem, 
 * please see the SSCA #1 Written Specification.
 *
 * INPUT
 * simMatrix        - the simMatrix structure
 * B
 *   numReports     - [maxThreads][integer] number of reports per thread
 *   bestStartsI    - [maxThreads][reports][integer] main startpoints
 *   bestStartsJ    - [maxThreads][reports][integer] match startpoints
 *   bestEndsI      - [maxThreads][reports][integer] main endpoints
 *   bestEndsJ      - [maxThreads][reports][integer] match endpoints
 *   bestSeqsI      - [maxThreads][reports][unsigned char] main sequences
 *   bestSeqsJ      - [maxThreads][reports][unsigned char] match sequences
 *   bestScores     - [maxThreads][reports][long long] scores for reports
 * maxDisplay       - [integer] number of reports to display to the console
 */

void verifyAlignment(SIMMATRIX_T *simMatrix, BSTR_T *B, int maxDisplay) {

  int t, r, c, numReports, maxThreads, mainMatch, matchMatch;
  int myTaskID, numTasks, thread;
  char *main, *match;
  long long score, minScore, maxScore, largestScore, smallestScore;

#ifdef MPI
  MPI_Status status;
#endif

  /*
   * Get the maximum number of OpenMP threads from the B structure.
   * For MPI, maxThreads==1 so that bestScores, bestStartsI, bestStartsJ,
   * bestEndsI, bestEndsJ, bestSeqsI and bestSeqsJ are essentially one
   * dimensional arrays (as is the case for single-threaded execution).
   */

  maxThreads = B->numThreads;

  /* Get the MPI taskID and number of tasks (defaults are 0 and 1). */

#ifdef MPI
  if ( MPI_Comm_rank(MPI_COMM_WORLD, &myTaskID) != MPI_SUCCESS ) {
    printf("verifyAlignment: cannot get myTaskID\n");
    MPI_Finalize();
    exit (1);
  }
  if ( MPI_Comm_size(MPI_COMM_WORLD, &numTasks) != MPI_SUCCESS ) {
    printf("verifyAlignment: cannot get numTasks\n");
    MPI_Finalize();
    exit (1);
  }
#else
  myTaskID = 0;
  numTasks = 1;
#endif

  /*
   * Prepare a short summary of the total number of reports generated.
   * For OpenMP, this involves indexing the numReports and bestScores
   * arrays by the thread number.  For MPI, this involves reductions
   * over the proper elements of these arrays from each task.
   * For OpenMP sum over the elements of the B->numReports array.
   * For single-threaded execution the array contains one element.
   * If B->numReports[t]==0 the corresponding B->bestScores[i] array
   * is not allocated.
   */

#ifndef MPI
  numReports = 0;
  maxScore = MINUS_INFINITY;
  minScore = -MINUS_INFINITY;
  for (t = 0; t < maxThreads; t++) {
    if (B->numReports[t] != 0) {
      numReports += B->numReports[t];
      maxScore = MAX(maxScore, B->bestScores[t][1]);
      minScore = MIN(minScore, B->bestScores[t][B->numReports[t]]);
    }
  }

  /*
   * For MPI the B->numReports array contains one element for
   * each task, so perform a reduction over that element from
   * each task.  If this element equals zero, the B->bestScores[0]
   * array is not allocated, so supply very small and very large
   * numbers to the max and min reductions, respectively, that
   * are performed over the scores.
   */

#else
  if ( MPI_Allreduce( &(B->numReports[0]), &numReports,
		      1, MPI_INT, MPI_SUM,
		      MPI_COMM_WORLD ) != MPI_SUCCESS ) {
    printf("verifyAlignment: cannot reduce numReports\n");
    MPI_Finalize();
    exit (1);
  }

  if (B->bestScores[0] == NULL) {
    largestScore = MINUS_INFINITY;
    smallestScore = -MINUS_INFINITY;
  } else {
    largestScore = B->bestScores[0][1];
    smallestScore = B->bestScores[0][B->numReports[0]];
  }
  if ( MPI_Allreduce( &largestScore, &maxScore,
		      1, MPI_LONG_LONG_INT, MPI_MAX,
		      MPI_COMM_WORLD ) != MPI_SUCCESS ) {
    printf("verifyAlignment: cannot reduce maxScore\n");
    MPI_Finalize();
    exit (1);
  }
  if ( MPI_Allreduce( &smallestScore, &minScore,
		      1, MPI_LONG_LONG_INT, MPI_MIN,
		      MPI_COMM_WORLD ) != MPI_SUCCESS ) {
    printf("verifyAlignment: cannot reduce minScore\n");
    MPI_Finalize();
    exit (1);
  }
#endif
  if (myTaskID == 0) {
    if (numReports == 0) {
      printf("\n*** Found no acceptable alignments. ***\n");
    } else {
#if !defined(SPEC)
      printf("\n*** Found %d alignments with scores from %lld to %lld ***\n",
	     numReports, maxScore, minScore);
#endif
#if !defined(K2A_SUMMARY) && !defined(K2A_REPORTS)
      printf("*** #define K2A_SUMMARY and K2A_REPORTS for more info ***\n");
#endif
    }
    fflush(stdout);
  }

  /*
   * Now output the results for each OpenMP thread or MPI process, ordered
   * by increasing thread or process.  For OpenMP, just access the results
   * by indexing the arrays with the thread number.  For MPI, there will
   * be only one pass through the 'for' loop (because maxThreads==1), and,
   * to complicate matters further, the results are contained in arrays
   * that are local to each MPI process.  So serialize the processes by
   * having process 1 wait for process 0, and having process 2 wait for
   * process 1, etc.  This serialization can be implemented using the blocking
   * MPI_Send and MPI_Recv functions.  Process 0 waits for no other process,
   * and the highest process sends to no other process.  In the calls to
   * MPI_Send and MPI_Recv, numReports is a dummy argument.
   */

#ifdef MPI
  if (myTaskID != 0) {
    if ( MPI_Recv( &numReports, 1, MPI_INT, myTaskID-1,
		   0, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
      printf("verifyAlignment: cannot reduce numReports\n");
      MPI_Finalize();
      exit (1);
    }
  }
#endif

  for (t = 0; t < maxThreads; t++) {

    /* "thread" indicates the OpenMP thread or the MPI process. */

#ifdef MPI
    thread = myTaskID;
#else
    thread = t;
#endif

    /*
     * Get the number of reports for this process.  Print the fact that
     * there are no reports only for processes that lie on the grid.
     * A process (or thread) that does not lie on the grid has zero
     * reports; however, a process (or thread) that does lie on the
     * grid but which generates no reports will appear not to lie on
     * the grid in response to the numReports==0 test below.
     */

    numReports = B->numReports[t];
    if (numReports == 0) {
#if defined(K2A_SUMMARY) || defined(K2A_REPORTS)
#ifndef MPI
      if (B->bestScores[t] != NULL) {
	printf("\n*** Thread/Task %d found no acceptable alignments. ***\n", thread);
      }
#else
      if (B->bestScores[0] != NULL) {
	printf("\n*** Thread/Task %d found no acceptable alignments. ***\n", thread);
      }
#endif
#endif
    } else {
#if defined(K2A_SUMMARY) || defined(K2A_REPORTS)
      printf("\n*** Thread/Task %d found %d alignments with scores from %lld to %lld ***\n",
	     thread, numReports, B->bestScores[t][1],
	     B->bestScores[t][numReports]);
#ifdef K2A_REPORTS
      if (maxDisplay < numReports) {
	printf("Displaying the first %d of them\n", maxDisplay);
      }
#endif
#endif

#ifdef K2A_REPORTS
      if (maxDisplay > 0) {
	printf("\nStarting   Amino     Codon           Ending\n");
	printf("position   acids     bases           position\n");
      }
#endif
    }

    /* For each of the bestScores reported by Kernel 2... */

    for (r = 1; r <= numReports; r++) {

      main = (char *)(B->bestSeqsI[t][r]);
      match = (char *)(B->bestSeqsJ[t][r]);

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
       * In the case of success, print one report only for
       * more than one thread, or print maxDisplay reports
       * for only one thread.
       */

#ifdef K2A_REPORTS
      if (score != B->bestScores[t][r]) {
        printf("\nverifyAlignment %d failed; reported %lld vs actual %lld:",
	       r, B->bestScores[t][r], score);
	printf("   -----------------------\n");
      } else if (r <= maxDisplay) {
	printf("\nverifyAlignment %d succeeded; score %lld:\n",
	       r, B->bestScores[t][r]);

	printf("%7d  ", B->bestStartsI[t][r]);
	for (c = 0; c < strlen(main); c++) {
	  printf("%c", simMatrix->aminoAcid[ (int)main[c] ]);
	}
	printf("  ");
	for (c = 0; c < strlen(main); c++) {
	  printf("%s", simMatrix->codon[ (int)main[c] ]);
	}
	printf("  %7d\n", B->bestEndsI[t][r]);

	printf("%7d  ", B->bestStartsJ[t][r]);
	for (c = 0; c < strlen(match); c++) {
	  printf("%c", simMatrix->aminoAcid[ (int)match[c] ]);
	}
	printf("  ");
	for (c = 0; c < strlen(match); c++) {
	  printf("%s", simMatrix->codon[ (int)match[c] ]);
	}
	printf("  %7d\n", B->bestEndsJ[t][r]);
      }
      fflush(stdout);
#endif
    }
    fflush(stdout);
  }

  /*
   * Here is the call to MPI_Send that matches the above call to MPI_Recv.
   * The highest process number sends a message to process zero, which
   * waits for that message below.  This synchronization of process zero
   * guarantees that any messages which are generated by process zero
   * will follow all of the above messages.
   */

#ifdef MPI
  if (myTaskID < numTasks-1) {
    if ( MPI_Send( &numReports, 1, MPI_INT, myTaskID+1,
		   0, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
      printf("verifyAlignment: cannot reduce numReports\n");
      MPI_Finalize();
      exit (1);
    }
  } else {
    if ( MPI_Send( &numReports, 1, MPI_INT, 0,
		   0, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
      printf("verifyAlignment: cannot reduce numReports\n");
      MPI_Finalize();
      exit (1);
    }
  }
  if (myTaskID == 0) {
    if ( MPI_Recv( &numReports, 1, MPI_INT, numTasks-1,
		   0, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
      printf("verifyAlignment: cannot reduce numReports\n");
      MPI_Finalize();
      exit (1);
    }
  }
#endif

  /* Print a final message. */

  if (myTaskID == 0) {
    printf("\n*** End of report for verifyAlignment ***\n");
    fflush(stdout);
  }
}
