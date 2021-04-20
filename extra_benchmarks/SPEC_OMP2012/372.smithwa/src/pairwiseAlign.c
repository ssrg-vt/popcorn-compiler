/*
 * pairwiseAlign.c
 *
 * Perform the optimum pairwise alignment of two strings.
 *
 * Russ Brown
 */

/* @(#)pairwiseAlign.c	1.91 06/12/04 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "sequenceAlignment.h"

/*
 * Return information for an OpenMP or MPI rectangular computing grid, including
 * the number of threads or tasks in the grid.  If the thread doesn't lie on the
 * grid, return -1.
 *
 * For OpenMP, this function must be called from within a parallel region so that
 * valid values are obtained for threadnum and numthreads.
 */

int gridInfo(int *npRow, int *npCol, int *myRow, int *myCol)
{

  int i, threadNum, numThreads, minPer, per, row, col;

  /*
   * Get the OpenMP thread number and maximum number of threads,
   * or equivalently the MPI communication rank and size.
   * The defaults are 0 and 1, respectively.
   */

#ifdef SPEC_OMP
  threadNum = omp_get_thread_num();
  numThreads = omp_get_max_threads();
#elif defined(MPI)
  if ( MPI_Comm_rank(MPI_COMM_WORLD, &threadNum) != MPI_SUCCESS ) {
    printf("gridInfo: cannot get myTaskID\n");
    MPI_Finalize();
    exit (1);
  }
  if ( MPI_Comm_size(MPI_COMM_WORLD, &numThreads) != MPI_SUCCESS ) {
    printf("gridInfo: cannot get numTasks\n");
    MPI_Finalize();
    exit (1);
  }
#else
  threadNum = 0;
  numThreads = 1;
#endif

  /*
   * Map the OpenMP threads or MPI processes onto a rectangular
   * computing grid.  The dimensions of the grid should be as
   * close as possible to a square grid.  These dimensions are
   * found by dividing numThreads by all integers beginning with
   * 1 and ending with numThreads.  Any integer for which the
   * remainder is 0 becomes a candidate for the number of rows
   * on the computing grid.  The associated candidate for the
   * number of columns is the quotient.  Choose the (row, column)
   * pair that minimizes the perimeter (or hemi-perimeter) of the
   * rectangle.  The maximum possible value of this hemi-perimeter
   * occurs for a rectangle having dimensions numThreads by 1,
   * so set the minPer variable to more than this hemi-perimeter.
   */

  minPer = numThreads + 2;
  for (i = 1; i <= numThreads; i++) {
    if ( (numThreads % i) == 0 ) {
      row = i;
      col = numThreads / i;
      per = row + col;
      if (per < minPer) {
	*npRow = row;
	*npCol = col;
	minPer = per;
      }
    }
  }

  /*
   * Determine the (row, column) coordinates for this OpenMP thread
   * or MPI process assuming row-major mapping onto the computing
   * grid.  In principle, all threads will lie on the computing grid,
   * but check for this condition and return (-1, -1) for the
   * coordinates if this condition is false.  This test is performed
   * for compatibility with prior versions of the gridInfo function
   * that mapped the thread or process onto a square computing grid,
   * and therefore it was possible that a thread or process did not
   * lie on the grid.
  */

  if ( threadNum < ( (*npRow) * (*npCol) ) ) {
    *myRow = threadNum / (*npCol);
    *myCol = threadNum % (*npCol);
    return ( (*npRow) * (*npCol) );
  } else {
    *myRow = -1;
    *myCol = -1;
    return (-1);
  }
}
 
/*
 * The bubblesort function from Robert Sedgewick's "Algorithms in C++" p. 101.
 *
 * Calling parameters are as follows:
 *
 * b - data array
 * left - the starting index of items to be sorted
 * right - the ending index of items to be sorted
 */

static
void bubbleSort(int *b, int left, int right) {

  int i, j, t;

  for (i = right; i >= left; i--) {
    for (j = left+1; j <= i; j++) {
      if (b[j-1] > b[j]) {
	t = b[j-1];
	b[j-1] = b[j];
	b[j] = t;
      }
    }
  }
}

/*
 * A front door to bubbleSort.
 *
 * Calling parameters are as follows:
 *
 * b - output array
 * a - input array
 * left - the starting index of items to be sorted
 * right - the ending index of items to be sorted
 */

void qSort(int *b, const int *a, const int left, const int right) {

  int i;

  /*
   * Copy the input data to the output data and call quickSort.
   * Note that the data is copied to the same relative offset
   * in the output data array.
   */

  for (i = left; i <= right; i++) {
    b[i] = a[i];
  }
  bubbleSort(b, left, right);
}

/*
 * The bubblesort function from Robert Sedgewick's "Algorithms in C++" p. 101,
 * with the added feature that an index array is reordered along with
 * the data array that is copied from the input data array
 *
 * Calling parameters are as follows:
 *
 * y - output data array
 * a - output index array
 * x - input data array
 * n - the number of items to be sorted
 */

static
void bubbleSort_both(long long *y, int *a, int left, int right) {

  int i, j, t;
  long long lT;

  for (i = right; i >= left; i--) {
    for (j = left+1; j <= i; j++) {
      if (y[j-1] > y[j]) {
	t = a[j-1];
	a[j-1] = a[j];
	a[j] = t;
	lT = y[j-1];
	y[j-1] = y[j];
	y[j] = lT;
      }
    }
  }
}

/*
 * A front door to bubbleSort_both.
 *
 * Calling parameters are as follows:
 *
 * y - output data array
 * a - output index array
 * x - input data array
 * n - the number of items to sort
 */

void qSort_both(long long *y, int *a, const long long *x, const int n) {

  int i;

  /*
   * Copy the input data to the output data, load the index array
   * so that each element contains its address, and call quickSort_both.
   */

  for (i = 1; i <= n; i++) {
    y[i] = x[i];
    a[i] = i;
  }
  bubbleSort_both(y, a, 1, n);
}

/*
 * Function pairwiseAlign - Kernel 1 - Pairwise Local Sequence Alignment.
 *
 * This function uses a variant of the Smith-Waterman dynamic programming
 * algorithm to match subsequences of seqData->main against subsequences
 * of seqData->match, to score them using the 'local affine gap' codon matching
 * function defined by simMatrix, and to report an ordered set of best matches.
 * The best matches include only the score and the end points of each match.
 *
 * When selecting a set of 'best' local sequence alignments it is important to
 * chose only one match from a cluster of closely related matches.  Various
 * heuristics to accomplish are discussed in the literature.  This code reports
 * only the best match ending within a specified interval of each end point.
 * In addition, further filtering is performed by Kernel 2, which locates the
 * actual aligned sequences from their end points and scores.
 *
 * If you don't understand the Smith-Waterman algorithm you will have trouble
 * understanding this code.  The recurrences used are discussed in Gusfield,
 * Algorithms on Stings, Trees, and Sequences, pg 244.
 *
 * For a detailed description of the SSCA #1 Optimal Pattern Matching problem, 
 * please see the SSCA #1 Written Specification.
 *
 * INPUT
 * seqData          - [SEQDATA_T*] data sequences created by genScalData()
 *   main           - [char string] first codon sequence
 *   match          - [char string] second codon sequence
 *   maxValidation  - [Integer] longest matching validation string.
 * simMatrix        - [SIMMATRIX_T*] codon similarity created by genSimMatrix()
 *   similarity     - [2D array int8] 1-based codon/codon similarity table
 *   aminoAcid      - [1D char vector] 1-based codon to aminoAcid table
 *   bases          - [1D char vector] 1-based encoding to base letter table
 *   codon          - [64 x 3 char array] 1-based codon to base letters table
 *   encode         - [uint8 vector] aminoAcid character to last codon number
 *   hyphen         - [uint8] encoding representing a hyphen (gap or space)
 *   exact          - [integer] value for exactly matching codons
 *   similar        - [integer] value for similar codons (same amino acid)
 *   dissimilar     - [integer] value for all other codons
 *   gapStart       - [integer] penalty to start gap (>=0)
 *   gapExtend      - [integer] penalty to for each codon in the gap (>0)
 *   matchLimit     - [integer] longest match including hyphens
 * minScore         - [integer] minimum endpoint score
 * maxReports       - [integer] maximum number of endpoints reported
 * minSeparation    - [integer] minimum endpoint separation in codons
 *
 * OUTPUT
 * A                - [ASTR_T*] reporting the good alignments
 *   seqData        - copy of input argument
 *   simMatrix      - copy of input argument
 *   numThreads     - [integer] number of OpenMP threads
 *   numReports     - [numThreads][integer] number of reports per thread
 *   goodEndsI      - [numThreads][reports][integer] main endpoints
 *   goodEndsJ      - [numThreads][reports][integer] match endpoints
 *   goodScores     - [numThreads][reports][long long] scores for reports
 */

ASTR_T *pairwiseAlign(SEQDATA_T *seqData, SIMMATRIX_T *simMatrix,
		      int minScore, int maxReports, int minSeparation) {

  int i, j, k, r, n, m, sortReports, report;
  int *index, *best, *goodEndsI, *goodEndsJ, gapFirst, gapExtend, worst;
  int iBeg, iEnd, jBeg, jEnd, myRow, myCol, npRow, npCol;
  int myTaskID, threadNum, maxThreads, matchLimit;
  unsigned char *mainSeq, *matchSeq;
  char **weights;
  long long *goodScores, *scores;
  long long *V, *F, G, Vp, E, W, llMinScore;
  ASTR_T *A=NULL;
  double beginTime, endTime;

  /* Make a long long version of minScore. */

  llMinScore = (long long)minScore;

  /* Get the maximum length of a biologically interesting sequence. */

  matchLimit = simMatrix->matchLimit;

  /* Allocate and initialize the structure A. */

  if ( (A = (ASTR_T*)malloc( sizeof(ASTR_T) ) ) == NULL ) {
    printf("pairwiseAlign: cannot allocate A\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }
  A->numThreads = 0;
  A->numReports = NULL;
  A->goodScores = NULL;
  A->goodEndsI = A->goodEndsJ = NULL;

  /* Don't copy the input structures - just get the pointers to them. */

  A->seqData = seqData;
  A->simMatrix = simMatrix;

  /*
   * Allocate numReports, goodScores and goodEnds arrays for each
   * thread, using 0-based indexing because thread numbers start at 0.
   * For MPI, maxThreads==1 so that goodScores, goodEndsI and goodEndsJ
   * are essentially 1-dimensional arrays (as is the case for
   * single-threaded execution).
   */

#ifdef SPEC_OMP
  maxThreads = omp_get_max_threads();
#else
  maxThreads = 1;
#endif

  /*
   * Allocate the arrays for per-thread results.  Set defaults
   * in case a thread does not lie on the computing grid.
   */

  A->numThreads = maxThreads;

  if ( (A->numReports = (int*)malloc(maxThreads * sizeof(int))) == NULL ) {
    printf("pairwiseAlign: cannot allocate A->numReports\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (A->goodScores = (long long**)malloc(maxThreads * sizeof(long long*)))
       == NULL ) {
    printf("pairwiseAlign: cannot allocate A->goodScores\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (A->goodEndsI = (int**)malloc(maxThreads * sizeof(int*))) == NULL ) {
    printf("pairwiseAlign: cannot allocate A->goodEndsI\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (A->goodEndsJ = (int**)malloc(maxThreads * sizeof(int*))) == NULL ) {
    printf("pairwiseAlign: cannot allocate A->goodEndsJ\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  for (i = 0; i < maxThreads; i++) {
    A->numReports[i] = 0;
    A->goodScores[i] = 0L;
    A->goodEndsI[i] = A->goodEndsJ[i] = NULL;
  }

  /*
   * Create a parallel region to distribute the computing amongst multiple
   * OpenMP threads.  The approach to parallelization follows the theory
   * presented in the ssca1-0.6/doc/parallelization.txt document.  The
   * O(nm) conceptual matrix is decomposed into s rectangles whose size is
   * [n/r + (k-1)][m/c + (k-1)] where k is the matchLimit parameter, r
   * is the number of rows and c is the number of columns.  Kernel 1
   * treats each rectangle in a completely independent manner and hence
   * requires no communication between threads and no shared data.
   * However, the results of the kernel 1 computation, i.e., the goodScores
   * and goodEnds arrays, must be returned from this pairwiseAlign function
   * in order that kernel 2 receive the proper input data.  Hence, a copy
   * of each array for each thread will be returned in the A structure.
   */

#if defined(SPEC_OMP)
#pragma omp parallel \
  firstprivate(llMinScore, maxReports, matchLimit, minSeparation) \
  private (i, j, k, r, m, n, sortReports, report, worst, index, best, \
           goodEndsI, goodEndsJ, gapFirst, gapExtend, iBeg, iEnd, jBeg, jEnd, \
           myRow, myCol, npRow, npCol, mainSeq, matchSeq, weights, goodScores, \
           scores, V, F, G, Vp, E, W, threadNum, myTaskID, beginTime, endTime)
#endif
  {

    /* Map the OpenMP threads or MPI tasks onto a rectangular computing grid. */

    gridInfo(&npRow, &npCol, &myRow, &myCol);

    /*
     * Get the thread number.  For MPI threadNum==0 (as is the case
     * for single-threaded execution).  The thread number will be used
     * as an array index.  But the MPI process myTaskID will be used
     * in place of threadNum for all error messages.
     */

#ifdef SPEC_OMP
    threadNum = omp_get_thread_num();
#else
    threadNum = 0;
#endif

#ifdef MPI
    if ( MPI_Comm_rank(MPI_COMM_WORLD, &myTaskID) != MPI_SUCCESS ) {
      printf("pairwiseAlign: cannot get myTaskID\n");
      MPI_Finalize();
      exit (1);
    }
#else
    myTaskID = threadNum;
#endif

    /* Do nothing if the thread does not lie on the process grid. */

    if (myRow >= 0 && myCol >= 0) {

      /* Make some private copies of input variables. */

      sortReports = 3 * maxReports;   /* point at which to sort and discard */
      gapExtend = simMatrix->gapExtend;
      gapFirst = simMatrix->gapStart + gapExtend; /* Penalty for first gap */
    
      /*
       * Allocate and zero the goodScores and goodEnds arrays.
       * Extend all indices by 1 to permit 1-based indexing.
       */

      if ( (goodScores =
	    (long long*)malloc((sortReports+1)*sizeof(long long))) == NULL ) {
	printf("pairwiseAlign: cannot allocate goodScores for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (goodEndsI = (int*)malloc((sortReports+1)*sizeof(int))) == NULL ) {
	printf("pairwiseAlign: cannot allocate goodEndsI for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (goodEndsJ = (int*)malloc((sortReports+1)*sizeof(int))) == NULL ) {
	printf("pairwiseAlign: cannot allocate goodEndsJ for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (i = 1; i <= sortReports; i++) {
	goodScores[i] = 0L;
	goodEndsI[i] = goodEndsJ[i] = 0;
      }

      /*
       * Allocate some arrays to be used in sorting.
       * Extend all indices by 1 to permit 1-based indexing.
       */

      if ( (scores = (long long*)malloc((sortReports+1)*sizeof(long long)))
	== NULL ) {
	printf("pairwiseAlign: cannot allocate scores for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (index = (int*)malloc((sortReports+1)*sizeof(int))) == NULL ) {
	printf("pairwiseAlign: cannot allocate index for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if( (best = (int*)malloc((sortReports+1)*sizeof(int))) == NULL ) {
	printf("pairwiseAlign: cannot allocate best for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }


      report = 0;

      /*
       * Determine the start and end indices for each thread, assuming
       * that the n*m (conceptual) matrix is subdivided into npRow*npCol
       * rectangles.  Each square overlaps the next square by matchLimit-1
       * in both the i and j direction.
       *
       * Note that each OpenMP thread needs only a fraction of the
       * V and F arrays that is required by the single-threaded code. 
       */

      n = A->seqData->mainLen;
      m = A->seqData->matchLen;

      iBeg = 1 + (n*myRow)/npRow;
      jBeg = 1 + (m*myCol)/npCol;

      iEnd = MIN( n, (n*(myRow+1))/npRow + (matchLimit-1) );
      jEnd = MIN( m, (m*(myCol+1))/npCol + (matchLimit-1) );

      /*
       * Copy portions of the sequence arrays so as to have private copies.
       * For these copies, the indexing will be 1-based.
       *
       * For MPI, it is possible to replace A->seqData->main with mainSeq,
       * and to replace A->seqData->match with matchSeq after mainSeq and
       * matchSeq have been copied from A->seqData->main and A->seqData->match,
       * respectively.  This approach would avoid having to recopy into
       * mainSeq and matchSeq in scanBackward, and to recopy into mainSeq
       * in locateSimilar.  However, the overlap region calculation for
       * mainSeq differs in locateSimilar from the calculation in scanBackward,
       * so for the present the avoidance of recopying is not implemented.
       */

      if ( (mainSeq =
	    (unsigned char*)malloc((iEnd - iBeg + 2) * sizeof(unsigned char)))
	   == NULL ) {
	printf("pairwiseAlign: cannot allocate mainSeq for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (i = iBeg; i <= iEnd; i++) {
	mainSeq[i - iBeg + 1] = A->seqData->main[i];
      }

      if ( (matchSeq = 
	    (unsigned char*)malloc((jEnd - jBeg + 2) * sizeof(unsigned char)))
		       == NULL ) {
	printf("pairwiseAlign: cannot allocate matchSeq for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (j = jBeg; j <= jEnd; j++) {
	matchSeq[j - jBeg + 1] = A->seqData->match[j];
      }

      /*
       * Copy the similarity matrix in order to have a private copy.
       * Allocate the matrix in the parallel region to ensure that
       * it is indeed a private copy.  This matrix uses 1-based
       * indexing.
       */

      if ( (weights = (char**)malloc( SIM_SIZE*sizeof(char*))) == NULL ) {
	printf("pairwiseAlign: cannot allocate weights for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (i = 1; i < SIM_SIZE; i++) {
	if ( (weights[i] = (char*)malloc( SIM_SIZE*sizeof(char))) == NULL ) {
	printf("pairwiseAlign: cannot allocate weights[%d] for thread %d\n",
	       i, myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

	for (j = 1; j < SIM_SIZE; j++) {
	  weights[i][j] = simMatrix->similarity[i][j];
	}
      }

      /*
       * Now we initialize the V(i,j) and F(i,j) base conditions for
       * the local-affine Smith-Waterman algorithm with free end gaps.
       * From Dan Gusfield's text, "Algorithms on Strings, Trees and
       * Sequences" p. 244 we see that V(0,j) = 0 for free end gaps.
       * Because we store only one row, for row 0 we can initialize
       * by setting V(j) = V(0,j) = 0.
       *
       * We also see from p. 244 that F(0,j) = -Wg - jWs, and because
       * we store only one row, for row 0 we could initialize by setting
       * F(j) = F(0,j) = -Wg - jWs.  However, from the recurrence relations
       * we see that to calculate V(i,j) for row 1, we need F(i,j) for row 1
       * not for row 0, because the equation for V(i,j) from p. 244 (modified
       * for local alignment as explained on p. 233) is:
       *
       *       V(i,j) = max[0, E(i,j), F(i,j), G(i,j)]
       *
       * Therefore, we need to compute F(j) = F(i,j) for row 1 from the
       * recurrence relation from p. 244:
       *
       *       F(i,j) = max[F(i-1,j), V(i-1,j) - Wg] - Ws
       *
       * which for row 1 becomes:
       *
       *       F(1,j) = max[F(1-1,j), V(1-1,j) - Wg] - Ws
       *
       *              = max[F(0,j), V(0,j) - Wg] - Ws
       *
       *              = max[ -Wg - jWs, 0 - Wg] - Ws
       *
       *              = -Wg - Ws = -gapFirst
       */

      if ( (V = (long long*)malloc( (jEnd - jBeg + 2) * sizeof(long long)))
	   == NULL ) {
	printf("pairwiseAlign: cannot allocate V for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (j = jBeg; j <= jEnd; j++) {
	V[j - jBeg + 1] = 0L;
      }

      if ( (F = (long long*)malloc( (jEnd - jBeg + 2) * sizeof(long long)))
	   == NULL ) {
	printf("pairwiseAlign: cannot allocate F for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (j = jBeg; j <= jEnd; j++) {
	F[j - jBeg + 1] = (long long)(-gapFirst);
      }

      /* Measure the time for this computational loop #ifdef MATCH_TIME. */

#ifdef MATCH_TIME
      beginTime = getSeconds();
#endif

      /*
       * Loop over each codon in the mainSeq sub-sequence that is to be
       * processed by this thread, matching it with each codon in the
       * matchSeq sub-sequence that is to be processed by this thread.
       */

      for (i = iBeg; i <= iEnd; i++) {

	/*
	 * For each codon in the mainSeq sub-sequence, match with each
	 * codon in the matchSeq sub-sequence that is to be processed
	 * by this thread.  However, probably because it is assumed
	 * that the first point in the match can't end a good match,
	 * update to the second point in the matchSeq sub-sequence.
	 *
	 * It is necessary to store only one element of each of
	 * G(i,j) and E(i,j) because processing occurs along rows.
	 *
	 * Get G = G(i,1) from the recurrence relation (p. 244):
	 *
	 *       G(i,j) = V(i-1,j-1) + Wm
	 *
	 * where Wm = W(i,j) is obtained from the similarity matrix.  Hence,
	 *
	 *       G(i,1) = V(i-1,1-1) + W(i,1) = V(i-1,0) + W(i,j) = W(i,j)
	 *
	 * because V(i-1,0) equals 0 for free end gaps (all entries in
	 * column 0 equal 0).
	 */

	G = (long long)(weights[mainSeq[i-iBeg+1]][ matchSeq[1] ]);

	/*
	 * Store Vp = V(i-1,1) = V(1) from the previous row, then calculate
	 * V(1) = V(i,1) for this row from the recurrence relation (p. 244):
	 *
	 *       V(i,1) = max[0, E(i,1), F(i,1), G(i,1)]
	 *
	 * where F(i,1) = F(1) and G(i,1) = G are both known (see above)
	 * and E = E(i,1) must be calculated from the recurrence relation
	 * (p. 244):
	 *
	 *       E(i,1) = max[E(i,1-1), V(i,1-1) - Wg] - Ws
	 *
	 *              = max[E(i,0), V(i,0) - Wg] - Ws
	 *
	 *              = max[-Wg - iWs, -Wg] - Ws
	 *
	 *              = -Wg - Ws
	 *
	 * And because E(i,1) is clearly < 0 there is no requirement to
	 * include it in the recurrent relation for V(i,1) which becomes:
	 *
	 *       V(i,1) = max[0, F(i,1), G(i,1)]
	 *
	 * or, because V(j) and F(j) store values for one row only, and
	 * because only one value is stored for G:
	 *
	 *       V(1) = max[0, F(1), G]
	 *
	 * This concludes the calculation for column 1 of the present row.
	 */

	Vp = V[1];
	V[1] = MAX( 0L, MAX(F[1], G) );

	/*
	 * F(1) = F(i,1) and E = E(i,1) have been used in the above recurrence
	 * relation for V(1) = V(i,1), so now calculate F(1) = F(i+1,1) and
	 * E = E(i,2) from their recurrence relations (p. 244):
	 *
	 *       F(i+1,1) = max[F(i,1), V(i,1) - Wg] - Ws
	 *
	 *                = max[F(1) - Ws, V(1) - Wg - Ws]
	 *
	 *                = max[F(1) - gapExtend, V(1) - gapFirst]
	 *
	 *
	 *       E(i,2) = max[E(i,1), V(i,1) - Wg] - Ws
	 *
	 *              = max[-Wg - Ws, V(1) - Wg] - Ws
	 *
	 *              = V(1) - Wg - Ws
	 *
	 *              = V(1) - gapFirst
	 *
	 * because V(1) >= 0 as can be seen from its recurrence relation,
	 * so clearly V(1) - Wg > -Wg - Ws in the above max function.
	 */

	F[1] = MAX( F[1] - gapExtend, V[1] - gapFirst );
	E = V[1] - gapFirst;

	/*
	 * Loop over each codon in the matchSeq sub-sequence that is to be
	 * processed by this thread.
	 */

	for (j = jBeg + 2; j <= jEnd; j++) {

	  /* Get a similarity weight for each mainSeq, matchSeq codon pair. */

	  W = (long long)weights[ mainSeq[i-iBeg+1] ][ matchSeq[j-jBeg+1] ];

	  /*
	   * Add that weight to the best match ending just before it.
	   * This accumulation requires the use of long long types
	   * to avoid overflow for long sequences.
	   */

	  G = W + Vp;

	  /*
	   * Store Vp = V(i-1,j) = V(j) from the previous row, then calculate
	   * V(j) = V(i,j) for this row from the recurrence relation (p. 244):
	   */

	  Vp = V[j - jBeg + 1];
	  V[j - jBeg + 1] = MAX( MAX(0L, E), MAX(F[j - jBeg + 1], G) );

	  /*
	   * If the score is good enough (V[j - jBeg + 1] >= llMinScore),
	   * and this match is an improvement (W > 0) and is not a skip
	   * (V[j - jBeg + 1] == G instead of E or F[j - jBeg + 1]), and either
	   * this is the last iteration of a row (j == jEnd) or column
	   * (i == iEnd), or the next match doesn't improve the match
	   * (weights[ mainSeq[i-iBeg+2] ][ matchSeq[j-jBeg+2] ] <= 0), then
	   * consider adding this endpoint to the list of endpoints.
	   */

	  if ( V[j-jBeg+1] >= llMinScore && W > 0L && V[j-jBeg+1] == G &&
	       ( j == jEnd || i == iEnd ||
		 weights[mainSeq[i-iBeg+2]][matchSeq[j-jBeg+2]] <= 0 ) ) {

	    /*
	     * Find any nearby endpoints -- use only the best one.
	     * The end points are added in a row-by-row manner with
	     * increasing row numbers, and also with increasing scores,
	     * so the approach to adding a new point is as follows.
	     * First, check the vertical (row) distance to the previously-
	     * recorded endpoint; if this distance equals or exceeds the
	     * minimum allowed separation between endpoints, then keep the
	     * previously-recorded endpoint and record the new endpoint.
	     * If this first check fails, then check the horizontal (column)
	     * distance to the previously-recorded endpoint; if this distance
	     * equals or exceeds the minimum allowed separation between
	     * endpoints, then keep the previously-recorded endpoint and
	     * record the new endpoint.  If this second check fails, then
	     * check that the sequence score of the previously-recorded
	     * endpoint exceeds that of the new endpoint, and if so, discard
	     * the new endpoint.  If, on the other hand, the score of the
	     * previously-recorded endpoint is less than or equal to that
	     * of the new endpoint, discard the previously-recorded endpoint,
	     * and repeat the checking process with the penultimately-recorded
	     * endpoint.  Eventually, the new endpoint will either be
	     * discarded or appended to the endpoints.  The net effect of
	     * this algorithm is to keep only the last endpoint from a set of
	     * endpoints in the same (row, column) region that all have the
	     * same sequence score.  But why keep the last such endpoint
	     * instead of the first?
	     *
	     * Note that the second check, as well as the discarding of
	     * an endpoint, both exhibit a quadratic growth law as coded
	     * at present.  For a small list of endpoints this growth law
	     * probably doesn't degrade performance substantially, but
	     * some form of sorting might be advantageous, particularly
	     * for a larger list of endpoints.
	     */

	    for (r = report; r >= 1; r--) {
	      if (i - goodEndsI[r] >= minSeparation) {
		break;         /* Passed row distance check: retain point r. */
	      }
	      if ( abs(j - goodEndsJ[r]) >= minSeparation) {
		continue;   /* Passed column distance check: retain point r. */
	      }
	      if (goodScores[r] > V[j - jBeg + 1]) {
		goto discardPoint;  /* Discard new point (and maybe others). */
	      }

	      /* Discard point r. */

	      for (k = r; k < report; k++) {
		goodScores[k] = goodScores[k+1];
		goodEndsI[k] = goodEndsI[k+1];
		goodEndsJ[k] = goodEndsJ[k+1];
	      }
	      report--;
	    }

	    /* Add a new point. */

	    report++;
	    goodScores[report] = V[j - jBeg + 1];
	    goodEndsI[report] = i;
	    goodEndsJ[report] = j;

	    /*
	     * When the table is full, sort and discard all but the best end
	     * points.  Keep the table in entry order, just compact-out the
	     * discarded entries.
	     *
	     * In what follows, qSort_both sorts the goodScores array
	     * in ascending order and places the result in the scores
	     * array, and also fills the index array such that
	     * scores[k] = goodScores[index[k]].  Then by traversing
	     * the index array in order it is possible to retrieve
	     * elements from the goodScores, goodEndsI and goodEndsJ
	     * arrays in order.  However, what is wanted is the subset
	     * of these arrays that corresponds to the best scores,
	     * but kept in the original order in which entries were
	     * added.  To accomplish this ordering, the entries from
	     * the index array that correspond to the best scores are
	     * sorted to produce the best array.  Because the elements
	     * in the index array represent the order in which entries
	     * were added to the goodScores array, sorting a subset of
	     * these elements places that subset in the original relative
	     * order. Then by traversing  best  in order it is pssible
	     * to traverse goodScores in order via goodScores[best[k]].
	     * And more importantly, one can traverse goodEndsI and
	     * goodEndsJ in the same manner because these arrays were
	     * initially ordered according to goodScores.  Hence one
	     * can cull the best of the scores from goodScores, and
	     * cull as well the best of the endpoints from goodEndsI/J.
	     *
	     * Note that qSort() does not place the result into the beginning
	     * elements of the output array best, but rather into the elements
	     * indexed by the range worst..sortReports.  This approach differs
	     * from that of the Matlab sort() function.
	     *
	     * Also, it is unclear that it is possible to copy from the
	     * goodScores and goodEnds arrays directly into those arrays
	     * without overwriting some data, so the scores array is used
	     * as an intermediary.
	     */

	    if (report == sortReports) {
	      qSort_both(scores, index, goodScores, sortReports);
	      worst = sortReports - maxReports + 1; /* index of worst score */
	      llMinScore = scores[worst] + 1L;       /* beat this score */
	      qSort(best, index, worst, sortReports); /* pick the best score */

	      for (k = worst; k <= sortReports; k++) {
		scores[k - worst + 1] = goodScores[best[k]];
	      }
	      for (k = 1; k <= maxReports; k++) {
		goodScores[k] = scores[k];
	      }

	      for (k = worst; k <= sortReports; k++) {
		scores[k - worst + 1] = goodEndsI[ best[k] ];
	      }
	      for (k = 1; k <= maxReports; k++) {
		goodEndsI[k] = scores[k];
	      }
	      for (k = worst; k <= sortReports; k++) {
		scores[k - worst + 1] = goodEndsJ[ best[k] ];
	      }
	      for (k = 1; k <= maxReports; k++) {
		goodEndsJ[k] = scores[k];
	      }
	      report = maxReports;
	    }
	  }
	discardPoint:

	  /*
	   * F(j) = F(i,j) and E = E(i,j) have been used in the last recurrence
	   * relation for V(j) = V(i,j), so now calculate F(j) = F(i+1,j) and
	   * E = E(i,j+1) from their recurrence relations (p. 244).
	   *
	   * F represents the best weight assuming a gap in matchSeq, and
	   * E represents the best weight assuming a gap in mainSeq.
	   */

	  F[j-jBeg+1] = MAX( F[j-jBeg+1] - gapExtend, V[j-jBeg+1] - gapFirst );
	  E = MAX( E - gapExtend, V[j - jBeg + 1] - gapFirst );
	}
      }

      /* Print the sequence match time. */

#ifdef MATCH_TIME
      endTime = getSeconds();
#ifdef SPEC_OMP
      if (threadNum == 0) {
	printf("\n        Match time = %10.5f seconds\n",
	       endTime - beginTime);
      }
#else
      if (myTaskID == 0) {
	printf("\n        Match time = %10.5f seconds\n",
	       endTime - beginTime);
      }
#endif
#endif

      /* Check whether any reports were generated, and if so store them. */

      if (report == 0) {
	A->numReports[threadNum] = report;
      } else {

	/* Decide whether all of the reports can be kept. */

	if (report <= maxReports) {

	  /* Yes, so allocate the score arrays (extend indices by 1). */

	  if ( (A->goodScores[threadNum] =
		(long long*)malloc((report+1)*sizeof(long long))) == NULL ) {
	    printf("pairwiseAlign: can't allocate A->goodScores for thread %d\n",
		   myTaskID);
#ifdef MPI
	    MPI_Finalize();
#endif
	    exit (1);
	  }

	  if ( (A->goodEndsI[threadNum] =
		(int*)malloc((report+1)*sizeof(int))) == NULL ) {
	    printf("pairwiseAlign: cannot allocate A->goodEndsI for thread %d\n",
		   myTaskID);
#ifdef MPI
	    MPI_Finalize();
#endif
	    exit (1);
	  }

	  if ( (A->goodEndsJ[threadNum] =
		(int*)malloc((report+1)*sizeof(int))) == NULL ) {
	    printf("pairwiseAlign: cannot allocate A->goodEndsJ for thread %d\n",
		   myTaskID);
#ifdef MPI
	    MPI_Finalize();
#endif
	    exit (1);
	  }

	  /*
	   * Sort the goodScores array and use the index array to load
	   * A->goodScores and A->goodEndsI/J.  This approach guarantees
	   * that a sort on goodScores applies to goodEnds as well.
	   *
	   * Matlab fills the A arrays in reverse direction while traversing
	   * the index array in forward direction, which sorts the scores
	   * in descending order.  Mimic that approach.
	   */

	  qSort_both(scores, index, goodScores, report);
	  j = 1;
	  for (i = report; i >= 1; i--) {
	    A->goodScores[threadNum][j] = goodScores[ index[i] ];
	    A->goodEndsI[threadNum][j] = goodEndsI[ index[i] ];
	    A->goodEndsJ[threadNum][j] = goodEndsJ[ index[i] ];
	    j++;
	  }

	  /* Store the number of reports for this thread. */

	  A->numReports[threadNum] = report;

	} else {

	  /*
	   * No, all of the reports cannot be kept, so calculate
	   * the number of reports to keep.
	   */

	  worst = MAX(report - maxReports + 1, 1);

	  /* Allocate the score arrays (extend indices by 1). */

	  if ( (A->goodScores[threadNum] =
		(long long*)malloc((maxReports+1)*sizeof(long long))) ==
	       NULL ) {
	    printf("pairwiseAlign: cannot allocate A->goodScores for thread %d\n",
		   myTaskID);
#ifdef MPI
	    MPI_Finalize();
#endif
	    exit (1);
	  }

	  if ( (A->goodEndsI[threadNum] =
		(int*)malloc((maxReports+1)*sizeof(int))) == NULL ) {
	    printf("pairwiseAlign: cannot allocate A->goodEndsI for thread %d\n",
		   myTaskID);
#ifdef MPI
	    MPI_Finalize();
#endif
	    exit (1);
	  }

	  if ( (A->goodEndsJ[threadNum] =
		(int*)malloc((maxReports+1)*sizeof(int))) == NULL ) {
	    printf("pairwiseAlign: cannot allocate A->goodEndsJ for thread %d\n",
		   myTaskID);
#ifdef MPI
	    MPI_Finalize();
#endif
	    exit (1);
	  }

	  /*
	   * Sort the goodScores array and use the index array to load
	   * A->goodScores and A->goodEndsI/J.  This approach guarantees
	   * that a sort on goodScores applies to goodEnds as well.
	   *
	   * Matlab fills the A arrays in reverse direction while traversing
	   * the index array in forward direction, which sorts the scores
	   * in descending order.  Mimic that approach.
	   */

	  qSort_both(scores, index, goodScores, report);
	  j = 1;
	  for (i = report; i >= worst; i--) {
	    A->goodScores[threadNum][j] = goodScores[ index[i] ];
	    A->goodEndsI[threadNum][j] = goodEndsI[ index[i] ];
	    A->goodEndsJ[threadNum][j] = goodEndsJ[ index[i] ];
	    j++;
	  }

	  /* Store the number of reports for this thread. */

	  A->numReports[threadNum] = maxReports;
	}
      }

      /* Deallocate the dynamic arrays. */

      for (i = 1; i < SIM_SIZE; i++) {
	free(weights[i]);
      }
      free(weights);
      free(goodScores);
      free(goodEndsI);
      free(goodEndsJ);
      free(F);
      free(V);
      free(scores);
      free(index);
      free(best);
#ifndef MPI
      free(mainSeq);
      free(matchSeq);
#endif
    }
  }

  return (A);
}

/* Function freeA() - free the structure A including sub-structures and return NULL. */

ASTR_T *freeA(ASTR_T *A) {

  int i;

  if (A) {
    if (A->numReports) {
      free(A->numReports);
      A->numReports = NULL;
    }
    if (A->goodScores) {
      for (i = 0; i < A->numThreads; i++) {
	if (A->goodScores[i]) {
	  free(A->goodScores[i]);
	  A->goodScores[i] = NULL;
	}
      }
      free(A->goodScores);
      A->goodScores = NULL;
    }
    if (A->goodEndsI) {
      for (i = 0; i < A->numThreads; i++) {
	if (A->goodEndsI[i]) {
	  free(A->goodEndsI[i]);
	  A->goodEndsI[i] = NULL;
	}
      }
      free(A->goodEndsI);
      A->goodEndsI = NULL;
    }
    if (A->goodEndsJ) {
      for (i = 0; i < A->numThreads; i++) {
	if (A->goodEndsJ[i]) {
	  free(A->goodEndsJ[i]);
	  A->goodEndsJ[i] = NULL;
	}
      }
      free(A->goodEndsJ);
      A->goodEndsJ = NULL;
    }
    free(A);
  }
  return (NULL);
}
