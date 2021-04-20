/*
 * sequenceAlignment.c
 *
 * Here is the main program.
 *
 * Russ Brown
 */

/* @(#)sequenceAlignment.c	1.56 10/01/27 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "sequenceAlignment.h"

int main(int argc, char **argv) {

  int myTaskID, ierr;
  int randomSeed, scale, mainSeqLength, matchSeqLength;
  time_t timeVar;
  struct tm *timeStr;
  double startTime;
  SIMMATRIX_T *simMatrix;
  SEQDATA_T *seqData;
  ASTR_T *A;
  BSTR_T *B;
  CSTR_T *C;

  /* Check for both OMP and MPI defined. */

#if defined(SPEC_OMP) && defined(MPI)
  printf("Either OMP or MPI may be #define'd but not both!\n");
  exit (1);
#endif

  /* Initialize MPI and get the task number if required. */

#ifdef MPI
  if ( MPI_Init(&argc, &argv) != MPI_SUCCESS ) {
    printf("sequenceAlignment: cannot initialize MPI\n");
    exit (1);
  }
  if ( MPI_Comm_rank(MPI_COMM_WORLD, &myTaskID) != MPI_SUCCESS ) {
    printf("sequenceAlignment: cannot get myTaskID\n");
    MPI_Finalize();
    exit (1);
  }
#else
  myTaskID = 0;
#endif

  /* Get the scale variable as a command-line argument or by default. */

  if ( (argc != 1) && (argc != 2) ) {
    if (myTaskID == 0) {
      printf("Usage: %s <2*log2(sequence length)>\n", argv[0]);
    }
    exit (1);
  }

  if (argc == 1) {
    scale = SCALE;
  } else {
    scale = atoi(argv[1]);
    if (scale < 0) {
      if (myTaskID == 0) {
	printf("sequenceAlignment: 2*log2(sequence length) must be >= 0!\n");
      }
      exit(1);
    }
  }

  /* Set the sequence lengths from the scale variable. */

  mainSeqLength = matchSeqLength = 1 << (scale / 2);

  /*--------------------------------------------------------------------------
   * Preamble.
   *-------------------------------------------------------------------------*/

  /* Test the user parameters. */

  getUserParameters();

  if (myTaskID == 0) {
    printf("\nHPCS SSCA #1 Bioinformatics Sequence Alignment ");
    printf("Executable Specification:\nRunning...\n");
  }

#ifdef ENABLE_VERIF
  randomSeed = 1;           /* when debugging reproduce the results. */
#else
  time(&timeVar);
  timeStr = localtime(&timeVar);
  randomSeed = 100 * (timeStr->tm_sec +
		      timeStr->tm_min +
		      timeStr->tm_hour +
		      timeStr->tm_mday +
		      timeStr->tm_mon +
		      timeStr->tm_year +
		      timeStr->tm_wday +
		      timeStr->tm_yday); /* different results for each run. */
#endif

  /*-------------------------------------------------------------------------
   * Scalable Data Generator
   *-------------------------------------------------------------------------*/

  if (myTaskID == 0) {
    printf("\nScalable data generation beginning execution...\n");
  }

  startTime = getSeconds();                      /* Start performance timing. */

  /* Generate Seq data as per the Written Specification. */

  simMatrix = genSimMatrix(SIM_EXACT, SIM_SIMILAR, SIM_DISSIMILAR,
			   GAP_START, GAP_EXTEND, MATCH_LIMIT, SIM_SIZE);

  if (myTaskID == 0) {
    printf("\n\tgenSimMatrix() completed execution.\n");
  }

  seqData = genScalData(randomSeed, simMatrix, mainSeqLength, matchSeqLength, SIM_SIZE);

  if (myTaskID == 0) {
    printf("\n\tgenScalData() completed execution.\n");
  }

  dispElapsedTime(startTime);            /* End performance timing. */

  /* Display results outside of performance timing. */

#ifdef ENABLE_VERIF
  verifyData(simMatrix, seqData, K1_MIN_SCORE, K1_MIN_SEPARATION);
#endif

  /*-------------------------------------------------------------------------
   * Kernel 1 - Pairwise Local Alignment
   *-------------------------------------------------------------------------*/

  if (myTaskID == 0) {
    printf("\nKernel 1 - pairwiseAlign() beginning execution...\n");
  }

  startTime = getSeconds();

  /* Compute alignments for the two sequences. */

  A = pairwiseAlign(seqData, simMatrix,
		    K1_MIN_SCORE, K1_MAX_REPORTS, K1_MIN_SEPARATION);

  if (myTaskID == 0) {
    printf("\n\tpairwiseAlign() completed execution.\n");
  }

  dispElapsedTime(startTime);            /* End performance timing. */

  /*-------------------------------------------------------------------------
   * Kernel 2A - Find actual codon sequences from scores and endpoints.
   *-------------------------------------------------------------------------*/

  if (myTaskID == 0) {
    printf("\nKernel 2A - scanBackward() beginning execution...\n");
  }

  startTime = getSeconds();

  /* Find codon sequences. */

  B = scanBackward(A, K2_MAX_REPORTS, K2_MIN_SEPARATION, K2_MAX_DOUBLINGS);

  if (myTaskID == 0) {
    printf("\n\tscanBackward() completed execution.\n");
  }

  dispElapsedTime(startTime);

  /* Display results outside of performance timing. */

#ifdef ENABLE_VERIF
  verifyAlignment(simMatrix, B, K2A_DISPLAY);
#endif

  /*-------------------------------------------------------------------------
   * Kernel 2B - Merge results from all threads that executed Kernel 2A.
   *-------------------------------------------------------------------------*/

  if (myTaskID == 0) {
    printf("\nKernel 2B - mergeAlignment() beginning execution...\n");
  }

  startTime = getSeconds();

  /* Merge the alignments that were collected by separate threads. */

  C = mergeAlignment(B, K2_MAX_REPORTS, K2_MIN_SEPARATION);

  if (myTaskID == 0) {
    printf("\n\tmergeAlignment() completed execution.\n");
  }

  dispElapsedTime(startTime);

  /* Display results outside of performance timing. */

#ifdef ENABLE_VERIF
  verifyMergeAlignment(simMatrix, C, K2B_DISPLAY);
#endif

  /*-------------------------------------------------------------------------
   * Deallocate the structures.
   *-------------------------------------------------------------------------*/

  A = freeA(A);
  B = freeB(B);
  C = freeC(C);
  freeSimMatrix(simMatrix);
  freeSeqData(seqData);

  /*-------------------------------------------------------------------------
   * Postamble.
   *-------------------------------------------------------------------------*/

  if (myTaskID == 0) {
    printf("\nHPCS SSCA #1 Bioinformatics Sequence Alignment ");
    printf("Executable Specification:\nEnd of test.\n");
  }

#ifdef MPI
  if ( MPI_Finalize() != MPI_SUCCESS ) {
    printf("sequenceAlignment: cannot finalize MPI\n");
    exit (1);
  }
#endif

 return (0);
}
