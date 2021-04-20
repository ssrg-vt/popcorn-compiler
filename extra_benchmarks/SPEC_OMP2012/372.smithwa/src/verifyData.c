/*
 * verifyData.c
 *
 * Verify correctness of similarity matrix and sequence data.
 *
 * Russ Brown
 */

/* @(#)verifyData.c	1.13 10/01/27 */

#include <stdio.h>
#include <stdlib.h>

#include "sequenceAlignment.h"

/*
 * Function verifyData() - verify genSimMatrix() and genScalData().
 *
 * First level checks on the functionality of the code in the Scalable
 * Data Generators.
 *
 * Prints results to stdout.
 *
 * For a detailed description of the SCCA #1 Graph Construction, 
 * please see the SCCA #2 Graph Analysis Written Specification.
 * 
 * INPUT
 * simMatrix      - [structure] holds the generated similarity parameters.
 * seqData        - [structure] holds the generated sequences.
 * minScore       - [integer] minimum score for a sequence
 * minSeparation  - [integer] minimum separation between start and end points
 */

void verifyData(SIMMATRIX_T *simMatrix, SEQDATA_T *seqData,
		int minScore, int minSeparation) {

  int myTaskID, matchLimit, globalNpRow, globalNpCol;
  int iBeg, iEnd, jBeg, jEnd, myRow, myCol, npRow, npCol, n, m;
  unsigned long long comparisons, reduced_comparisons;

  myTaskID = 0;
  comparisons = 0L;
  matchLimit = simMatrix->matchLimit;

#if defined(SPEC_OMP)
#pragma omp parallel reduction(+:comparisons) \
  private(iBeg, iEnd, jBeg, jEnd, myRow, myCol, npRow, npCol, n, m, myTaskID)
#endif

  {

    /*
     * Map the OpenMP threads or MPI processes onto a rectangular
     * computing grid that has the most square aspect ratio.
     */

    gridInfo(&npRow, &npCol, &myRow, &myCol);

    /* Store the number of rows and columns globally. */

#ifdef MPI
    if ( MPI_Comm_rank(MPI_COMM_WORLD, &myTaskID) != MPI_SUCCESS ) {
      printf("verifyData: cannot get myTaskID\n");
      MPI_Finalize();
      exit (1);
    }
#elif defined(SPEC_OMP)
    myTaskID = omp_get_thread_num();
#endif

    if (myTaskID == 0) {
      globalNpRow = npRow;
      globalNpCol = npCol;
    }

    /*
     * Determine the start and end indices for each thread, assuming
     * that the n*m (conceptual) matrix is subdivided into npRow*npCol
     * rectangles.  Each square overlaps the next square by matchLimit-1
     * in both the i and j direction.
     */

    n = seqData->mainLen;
    m = seqData->matchLen;

    iBeg = 1 + (n*myRow)/npRow;
    jBeg = 1 + (m*myCol)/npCol;

    iEnd = MIN( n, (n*(myRow+1))/npRow + (matchLimit-1) );
    jEnd = MIN( m, (m*(myCol+1))/npCol + (matchLimit-1) );

    /* Calculate the number of comparisons in the rectangle. */

    comparisons =
      ((unsigned long long)(iEnd - iBeg + 1)) *
      ((unsigned long long)(jEnd - jBeg + 1));

    /* Perform an explicit reduction for MPI. */

#ifdef MPI
    if ( MPI_Allreduce( &comparisons, &reduced_comparisons,
			1, MPI_UNSIGNED_LONG_LONG, MPI_SUM,
			MPI_COMM_WORLD ) != MPI_SUCCESS ) {
      printf("verifyData: cannot reduce comparisons\n");
      MPI_Finalize();
      exit (1);
    }
    comparisons = reduced_comparisons;
#endif
  }

  if (myTaskID == 0) {
    printf("\n");
    printf("         Length of main sequence in codons: %d\n",
	   seqData->mainLen);

    printf("        Length of match sequence in codons: %d\n",
	   seqData->matchLen);

    printf("        Weight for exactly matching codons: %d\n",
	   simMatrix->exact);

    printf("                 Weight for similar codons: %d\n",
	   simMatrix->similar);

    printf("              Weight for dissimilar codons: %d\n",
	   simMatrix->dissimilar);

    printf("                    Penalty to start a gap: %d\n",
	   simMatrix->gapStart);

    printf("           Penalty for each codon in a gap: %d\n",
	   simMatrix->gapExtend);

    printf("   Maximum length of a biological sequence: %d\n",
	   simMatrix->matchLimit);

    printf("   Minimum cumulative score for a sequence: %d\n",
	   minScore);

#if !defined(SPEC)
    printf("\n        Number of rows on the process grid: %d\n", globalNpRow);

    printf("     Number of columns on the process grid: %d\n", globalNpCol);

    printf("      Smith-Waterman character comparisons: %lld = %5.3e\n",
	   comparisons, (double)comparisons);
#endif
  }
}
