/*
 * scanBackward.c
 *
 * Proceed from the endpoint to the starting point of a match sequence.
 *
 * Russ Brown
 */

/* @(#)scanBackward.c	1.77 06/10/20 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "sequenceAlignment.h"

/* Function copyStringR - copy an unsigned char string. */

unsigned char *copyStringR(const unsigned char *inpString) {

  int c;
  unsigned char *outString;

  if ( ( c = strlen( (char*)inpString ) ) <= 0 ) {
    printf("copyStringR: input string length = %d\n", c);
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( ( outString = (unsigned char*)malloc( (c+1)*sizeof(unsigned char) ) )
       == NULL ) {
    printf("copyStringR: cannot allocate outString of length %d\n", c+1);
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  outString = (unsigned char*)strcpy( (char*)outString, (char*)inpString );
  return (outString);
}

/*
 * Function tracePathR - trace the matching paired sequences
 * and generate a typical matching pair.  The main and match
 * alignment sequences are stored in unsigned char strings
 * that are referenced via the ri and rj pointers, resepctively.
 * 
 * INPUT
 *   A              - [ASTR_T*] results from pairwiseAlign (hyphen is used)
 *   T              - [2D array uint8] scratch array for tracing sequence
 *   ei             - [integer] endpoint row coordinate
 *   ej             - [integer] endpoint column coordinate
 *   iBeg           - [integer] the first row of the grid rectangle
 *   jBeg           - [integer] the first column of the grid rectangle
 *   iEnd           - [integer] the final row of the augmented grid rectangle
 *   jEnd           - [integer] the final column of the augmented grid rectangle
 *   mainSeq        - [unsigned char string] main codon sequence
 *   matchSeq       - [unsigned char string] match codon sequence
 *   i              - [integer] subsequence row end coordinate used recursively
 *   j              - [integer] subsequence column end coordinate, used recurs.
 *   dir            - [integer] 1 when extending a gap in main (down),
 *                              2 when extending a gap in match (right),
 *                              else 0
 *   threadNum      - [integer] the OpenMP thread or MPI task
 *   ri             - [unsigned char pointer] main alignment sequence
 *   rj             - [unsigned char pointer] match alignment sequence
 *
 * OUTPUT
 *   rsi            - [integer] row coordinate of end of path
 *   rsj            - [integer] column coordinate of end of path
 *
 * RETURN VALUES
 *   0              - found a valid path
 *  -1              - did not find a path
 *  -2              - ei-i+1 underflow
 *  -3              - ei-i+1 overflow
 *  -4              - ej-j+1 underflow
 *  -5              - ej-j+1 overflow
 */

static
int tracePathR(const ASTR_T *A, unsigned char **T,
	       const int ei, const int ej,
	       const int iBeg, const int jBeg,
	       const int iEnd, const int jEnd,
	       const unsigned char *mainSeq, const unsigned char *matchSeq,
	       const int i, const int j, int dir, int threadNum,
	       int *rsi, int *rsj,
	       unsigned char *ri, unsigned char *rj) {

  /*
   * The default behavior is to return a termination character in
   * each of the main and match alignment sequences.  The "characters"
   * that are stored in the main and match alignment sequences are
   * integers in the range 1..64 (see genSimMatrix).  Hence, a viable
   * choice for a termination "character" is '\0' which has a value of 0.
   */

  *ri = '\0';
  *rj = '\0';

  /*
   * We just moved one step prior to the start of the sequence,
   * so return the termination characters.  Note that in the
   * test below, i is not transformed by iBeg-1, and j is not
   * transformed by jBeg-1 because i and j are calculated relative
   * to ei and ej, respectively, in the doScan function.
   */

  if ( (i == 0) || (j == 0) ) {
    *rsi = i + 1;
    *rsj = j + 1;

    return (0);
  }

  /* If not skipping, or start of gap, fan out from this point. */

  if ( (dir == 0) || ( (dir & T[i][j]) != 0 ) ) {
    dir = T[i][j] >> 2;
  }

  /*
   * We did not find a sequence, so return the termination characters
   * and flag an error.
   */

  if (dir == 0) {
    *rsi = 0;
    *rsj = 0;

#ifdef TRACEPATH_NORMAL_ERRORS
    printf("\ttracePathR : no sequence found for thread %d\n", threadNum);
#endif

    return (-1);
  }

  /*
   * From the Matlab code, we see that ei-i+1 and ej-j+1 are used as
   * indices to the mainSeq and matchSeq arrays.  However, as can be
   * seen from the pairwiseAlign and scanBackward functions of the C
   * code, these indices must first be transformed by adding 1-iBeg
   * and 1-jBeg, respectively.  Therefore, the transformed indices
   * are ei-iBeg-i+2 and ej-jBeg-j+2, respectively.
   *
   * Moreover, from the pairwiseAlign and scanBackward functions we
   * learn that the allowed ranges of these transformed indices are:
   *
   *              1 <= ei-iBeg-i+2 <= iEnd-iBeg+1
   *
   *              1 <= ej-jBeg-j+2 <= jEnd-jBeg+1
   *
   * These inequalities can be rearranged to yield:
   *
   *              iBeg <= ei-i+1 <= iEnd
   *
   *              jBeg <= ej-j+1 <= jEnd
   *
   * And these rearranged inequalities are used to test for overflow below.
   */

  if ( ei - i + 1 < iBeg ) {
    printf("\ttracePathR i-underflow: ei-i+1 = %d  iBeg = %d\n", ei-i+1, iBeg);
    return (-2);
  }
  if ( ei - i + 1 > iEnd ) {
    printf("\ttracePathR i-overflow: ei-i+1 = %d  iEnd = %d\n", ei-i+1, iEnd);
    return (-3);
  }
  if ( ej - j + 1 < jBeg ) {
    printf("\ttracePathR j-underflow: ej-j+1 = %d  jBeg = %d\n", ej-j+1, jBeg);
    return (-4);
  }
  if ( ej - j + 1 > jEnd ) {
    printf("\ttracePathR j-overflow: ej-j+1 = %d  jEnd = %d\n", ej-j+1, jEnd);
    return (-5);
  }

  /*
   * Use the first working alternative as the path to return.
   * Increment the ri and rj pointers to the main and match
   * alignment sequences prior to the recursive call to tracePath,
   * and decrement these pointers upon return from that call.
   * This action will build the alignment sequences in the
   * forward direction in the ri and rj arrays.
   */

  if ( (dir & 4) != 0) {                     /* diagonal */
    tracePathR(A, T, ei, ej, iBeg, jBeg, iEnd, jEnd,
	       mainSeq, matchSeq,
	       i-1, j-1, 0, threadNum,
	       rsi, rsj, ++ri, ++rj);

    if ( (*rsi > 0) || (*rsj > 0) ) {
      *(--ri) = mainSeq[ei - iBeg - i + 2];
      *(--rj) = matchSeq[ej - jBeg - j + 2];
      return (0);
    }
  }

  if ( (dir & 2) != 0) {                     /* down */
    tracePathR(A, T, ei, ej, iBeg, jBeg, iEnd, jEnd,
	       mainSeq, matchSeq,
	       i-1, j, 2, threadNum,
	       rsi, rsj, ++ri, ++rj);

    if ( (*rsi > 0) || (*rsj > 0) ) {
      *(--ri) = mainSeq[ei - iBeg - i + 2];
      *(--rj) = A->simMatrix->hyphen;
      return (0);
    }
  }

  if ( (dir & 1) != 0) {                     /* right */
    tracePathR(A, T, ei, ej, iBeg, jBeg, iEnd, jEnd,
	       mainSeq, matchSeq,
	       i, j-1, 1, threadNum,
	       rsi, rsj, ++ri, ++rj);

    if ( (*rsi > 0) || (*rsj > 0) ) {
      *(--ri) = A->simMatrix->hyphen;
      *(--rj) = matchSeq[ej - jBeg - j + 2];
      return (0);
    }
  }

  /*
   * Unclear what to do here, but I think that no sequence was found,
   * so return the termination characters and flag an error.
   */

  *rsi = 0;
  *rsj = 0;

#ifdef TRACEPATH_NORMAL_ERRORS
  printf("\ttracePathR : no sequence found for thread %d\n", threadNum);
#endif

  return (-1);
}
	      
/*
 * Function doScan - scan from the current end point to a start point
 * and return the sequence if the expected match is found.
 *
 * This variant of the Smith-Waterman algorithm starts at an end-point pair and
 * proceeds diagonally up and to the left, searching for the matching
 * start-point pair.  The outer loop goes right to left horizontally across the
 * matchSeq dimension.  The inner loop goes diagonally up and right, working
 * backward through the main sequence and simultaneously forward through the
 * match sequence.  The loops terminate at the edges of the square of possible
 * matches; the algorithm terminates at the first match with the specified
 * score.
 *
 * This variant differs from the Kernel 1 variant in that its initial point
 * is fixed (the end-point pair), and that its goal is known.  To find this
 * goal it may be necessary for intermediate values of the score to go
 * negative, which is impossible for Kernel 1.  However such matches are
 * rejected during traceback and may cause the end-point pair to be rejected.
 * For example the match
 *       ABCDEF------G
 *       ABCDEFXXXXXXG
 * might be found by Kernel 1 since its score stays positive scanning right to
 * left and its end-point pair is distinct from the F/F end-point pair.  But
 * scaning left to right the G/G match is not enough to prevent the gap from
 * taking the score negative.  (In this case, this match would also be rejected
 * since its start-point pair A/A matches the better ABCDEF/ABCDEF match.)
 *
 * INPUT
 * A                - [ASTR_T*] results from pairwiseAlign
 * T                - [2D array uint8] scratch array for tracing sequence
 * ei               - [integer] endpoint row coordinate
 * ej               - [integer] endpoint column coordinate
 * mainSeq          - [char string] main sequence
 * matchSeq         - [char string] match sequence
 * weights          - [2D array uint8] similarity matrix defined in genSimMatrix
 * gapFirst         - [integer] penalty for the first codon in the gap
 * gapExtend        - [integer] penalty to for each codon in the gap (>0)
 * minSeparation    - [integer] minimum startpoint separation in codons
 * report           - [integer] the number of reports
 * goal             - [long long] the score of the sequence to scan and trace
 * iBeg             - [integer] the first row of the grid rectangle
 * jBeg             - [integer] the first column of the grid rectangle
 * iFin             - [integer] the final row of the grid rectangle proper
 * jFin             - [integer] the final column of the grid rectangle proper
 * iEnd             - [integer] the final row of the augmented grid rectangle
 * jEnd             - [integer] the final column of the augmented grid rectangle
 * threadNum        - [integer] the OpenMP thread or MPI task
 *
 * OUTPUT
 * bestR            - [integer*] the number of sequences found thus far
 * bestStartsI      - [1D array integer] main startpoints
 * bestStartsJ      - [1D array integer] match startpoints
 * bestEndsI        - [1D array integer] main endpoints
 * bestEndsJ        - [1D array integer] match endpoints
 * bestSeqsI        - [1D array unsigned char string] main alignment sequences
 * bestSeqsJ        - [1D array unsigned char string] match alignment sequences
 * bestScores       - [1D array long long] the scores for the bestSeqs
 *
 * RETURN VALUES
 *  0               - found sequence and recorded it
 * -1               - quit in order to increase the size of the T array
 * -2               - sequence contains only one pair
 * -10              - sequence start within minSeparation of another sequence
 * -11              - sequence start not within grid square proper
 * -14              - unable to trace path for sequence
 * -15              - no sequence found
 * -16              - ei underflow
 * -17              - ei overflow
 * -18              - ej underflow
 * -19              - ej overflow
 */

static
int doScan(const ASTR_T *A, unsigned char **T, const int sizeT, int ei, int ej,
	   const unsigned char *mainSeq, const unsigned char *matchSeq,
	   char **weights, const int gapFirst, const int gapExtend,
	   const int minSeparation, const int report, const long long goal,
	   const int iBeg, const int jBeg,
	   const int iFin, const int jFin,
	   const int iEnd, const int jEnd,
	   const int threadNum,
	   int *bestR, int *bestStartsI, int *bestStartsJ,
	   int *bestEndsI, int *bestEndsJ,
	   unsigned char **bestSeqsI, unsigned char **bestSeqsJ,
	   long long *bestScores) {

  int i, j, r, v, m, e, f, di, dj, fi, fj, li, lj, rsi, rsj;
  long long **V, *E, *F, G, s;

  /* Check endpoints */

  if (ei < iBeg) {
#ifdef DOSCAN_NORMAL_ERRORS
    printf("\tdoScan: ei = %d  iBeg = %d for sequence %d and thread %d\n", 
	   ei, iBeg, report, threadNum);
#endif
    return (-16);
  }

  if (ei > iEnd) {
#ifdef DOSCAN_NORMAL_ERRORS
    printf("\tdoScan: ei = %d  iEnd = %d for sequence %d and thread %d\n", 
	   ei, iBeg, report, threadNum);
#endif
    return (-17);
  }

  if (ej < jBeg) {
#ifdef DOSCAN_NORMAL_ERRORS
    printf("\tdoScan: ej = %d  jBeg = %d for sequence %d and thread %d\n", 
	   ej, jBeg, report, threadNum);
#endif
    return (-18);
  }

  if (ej > jEnd) {
#ifdef DOSCAN_NORMAL_ERRORS
    printf("\tdoScan: ej = %d  jEnd = %d for sequence %d and thread %d\n", 
	   ej, jBeg, report, threadNum);
#endif
    return (-19);
  }

  /* The longest possible result. */

  m = MAX(ei - iBeg + 1, ej - jBeg + 1);

  /*
   * The diagonal best scores.  Two rows.  One extra point per row.
   * 1-based indexing for both indices.
   */

  if ( (V = (long long**)malloc( 3 * sizeof(long long*) ) ) == NULL ) {
    printf("doScan: cannot allocate V for thread %d\n", threadNum);
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }
  for (i = 1; i <= 2; i++) {
    V[i] = (long long *)malloc( (m+2) * sizeof(long long) );
    for (j = 1; j <= m+1; j++) {
      V[i][j] = MINUS_INFINITY;
    }
  }

  /*
   * The previous row and column if skipping horizontally/vertically.
   * 1-based indexing.
   */

  if ( (E = (long long*)malloc( (m+1) * sizeof(long long) ) ) == NULL ) {
    printf("doScan: cannot allocate E for thread %d\n", threadNum);
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (F = (long long *)malloc( (m+1) * sizeof(long long) ) ) == NULL ) {
    printf("doScan: cannot allocate F for thread %d\n", threadNum);
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  for (i = 1; i <= m; i++) {
    E[i] = F[i] = MINUS_INFINITY;
  }

  /* Special case the first point; discard length one sequences. */

  s = (long long)weights[ mainSeq[ei-iBeg+1] ] [ matchSeq[ej-jBeg+1] ];

  if (s == goal) {
#ifdef DOSCAN_NORMAL_MESSAGES
    printf("\tdoScan: length one sequence for report %d and thread%d\n",
	   report, threadNum);
#endif
    free(V[1]);
    free(V[2]);
    free(V);
    free(E);
    free(F);
    return (-2);
  }

  /* Here is some initialization to get to column 2 (I think). */

  V[1][2] = s;
  E[1] = F[1] = s - gapFirst;
  T[1][1] = 16+3;

  fj = ej - 1;           /* first point on the diagonal */
  fi = ei;
  lj = ej;               /* last point on the diagonal */
  v = 2;                 /* start with the second row of V */
  while (fi > 0) {       /* loop over diagonal starting points */
    dj = fj;
    di = fi;
    e = ei - di + 1;     /* subscript for E vector */
    f = ej - dj + 1;     /* subscript for F and V vectors */

    while ( (dj < lj+1) &&
	    (di >= iBeg) && (di <= iEnd) &&
	    (dj >= jBeg) && (dj <= jEnd) &&
	    (e >= 1) && (e <= m) && (f >= 1) && (f <= m) ) {

      /*
       * Match each main codon with each match codon, and
       * add the weight of the best match ending just after it.
       */

      G =
	(long long)weights[mainSeq[di-iBeg+1]][matchSeq[dj-jBeg+1]] + V[v][f];

      /* Find the very best weight ending with this pair. */

      s = MAX( E[e], MAX(F[f], G) );
      V[v][f+1] = s;

      /* If the score is ok, track this path; otherwise, eliminate it. */

      if (s > 0) {

	/* Check for address overflow before accessing the T matrix. */
 
	if (e > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 1 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}

	if (f > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 2 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}

	T[e][f] = (unsigned char)( 4*(s == E[e]) + 8*(s == F[f]) + 16*(s == G) );

      } else {

	/*
	 * Check for address overflow before accessing the tracking
	 * table T.  This particular problem appears to arise due to a
	 * terminal gap that is equal to the maximum size of the
	 * tracking table.
	 *
	 * I'm unsure what the proper action ought to be, but if the
	 * size of the tracking table is increased, this problem may
	 * re-occur for the larger size of the tracking table.  So
	 * I will allow the tracking table size to double a few times,
	 * and if the problem still occurs, sequence will be dropped
	 * instead of doubling the tracking table size once again in
	 * an attempt to avoid the problem.  The doubling test is
	 * performed in scanBackward.
	 */
 
	if (e > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 3 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}

	if (f > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 4 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}

	T[e][f] = 0;
      }

      /*
       * When the score equals the goal, the start point has been reached.
       * Discard this sequence if the start is too close to a better sequence,
       * or if the start is outside of the grid rectangle proper, i.e., within
       * the border of the rectangle such that the start lies within another
       * rectangle, and therefore the the sequence will be reported there.
       */

      if (s == goal) {
	for (r = 1; r <= *bestR; r++) {
	  if ( MAX( abs(di - bestStartsI[r]),
		    abs(dj - bestStartsJ[r]) ) < minSeparation ) {
#ifdef DOSCAN_NORMAL_MESSAGES
	    printf("\tdoScan: close starts; report %d discarded for thread %d\n",
		   report, threadNum);
#endif
	    free(V[1]);
	    free(V[2]);
	    free(V);
	    free(E);
	    free(F);
	    return (-10);
	  }
	}

	if ( (di > iFin) || (dj > jFin) ) {
#ifdef DOSCAN_NORMAL_MESSAGES
	  printf("\tdoScan: redundant start; report %d discarded for thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-11);
	}

	/* 
	 * The sequence wasn't discarded, so trace it.
	 *
	 * The e and f arguments to the tracePathR function can
	 * only decrease with recursive calls to tracePathR, so
	 * it is possible to to check them here for overflow.
	 */

	if (e > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 5 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}

	if (f > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 5 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}
	tracePathR(A, T, ei, ej, iBeg, jBeg, iEnd, jEnd,
		   mainSeq, matchSeq,
		   e, f, 0, threadNum,
		   &rsi, &rsj, bestSeqsI[*bestR+1], bestSeqsJ[*bestR+1]);
	if ( (rsi <= 0) && (rsj <= 0) ) {

	  /* No sequence was obtained via the trace. */

#ifdef DOSCAN_NORMAL_MESSAGES
	  printf("\tdoScan: can't trace path; report %d discarded for thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-14);
	}

	/*
	 * Tracing was successful, so record the result and return.
	 * Note that bestSeqsI[*bestR] and bestSeqsJ[*bestR] were
	 * passed to the tracePath function and used as pointers to
	 * store codon numbers in that function, so the alignment
	 * sequence result is already stored.
	 */

	(*bestR)++;
	bestStartsI[*bestR] = di;
	bestStartsJ[*bestR] = dj;
	bestEndsI[*bestR] = ei;
	bestEndsJ[*bestR] = ej;
	bestScores[*bestR] = goal;

#ifdef DOSCAN_ERROR_MESSAGES
	if (strlen( (char*)bestSeqsI[*bestR] == 0) {
	  printf("doScan: bestSeqsI[%d] has 0-length string for thread %d=n",
		 *best, threadNum);
	}
	if (strlen( (char*)bestSeqsJ[*bestR] == 0) {
	  printf("doScan: bestSeqsJ[%d] has 0-length string for thread %d=n",
		 *best, threadNum);
	}
#endif

	free(V[1]);
	free(V[2]);
	free(V);
	free(E);
	free(F);
	return (0);
      }

      /* Update the score. */

      s -= gapFirst;

      /* Find the best weight assuming a gap in matchSeq. */

      E[e] = MAX(E[e] - gapExtend, s);

      /* Find the weight assuming a gap in mainSeq. */

      F[f] = MAX(F[f] - gapExtend, s);

      /* Check for address overflow before accessing the T matrix. */
 
	if (e > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 6 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}

	if (f > sizeT) {
#ifdef DOSCAN_SIZET_OVERFLOW
	  printf("\tdoScan: sizeT overflow 7 for sequence %d and thread %d\n",
		 report, threadNum);
#endif
	  free(V[1]);
	  free(V[2]);
	  free(V);
	  free(E);
	  free(F);
	  return (-1);
	}

      T[e][f] += (unsigned char)( (E[e] == s) + 2 * (F[f] == s) );

      dj += 1;
      di -= 1;
      e += 1;
      f -= 1;
    }
 
   /* Toggle the row of the V vector. */

    v = 3 - v;

    if (fj != 1) {
      fj -= 1;
    } else {
      fi -= 1;
    }
  }

  /* No sequence was found. */

#ifdef DOSCAN_NORMAL_ERRORS
  printf("\tdoScan: report %d was not found for thread %d\n",
	 report, threadNum);
#endif
  free(V[1]);
  free(V[2]);
  free(V);
  free(E);
  free(F);
  return (-15);
}

/*
 * Function scanBackward - Kernel 2 - Find actual codon sequences
 *
 * This function uses a variant of the Smith-Waterman dynamic programming
 * algorithm to locate each actual aligned sequence from its end points and
 * score as reported by Kernel 1.  Some of end-points pairs may be rejected,
 * primarily because their matching start-points fall within a specified
 * interval of a better match.  Only the best maxReports matches are reported.
 *
 * While the start-points are being located, a record is kept of the
 * alternatives at each point.  Then the recursive tracepath function is
 * used to locate the match to report; there may be one or more equally
 * valid matches and if so the first one found is reported.
 *
 * For a detailed description of the SSCA #1 Optimal Pattern Matching problem, 
 * please see the SSCA #1 Written Specification.
 *
 * INPUT
 * A                - [ASTR_T*] results from pairwiseAlign
 *   seqData        - [SEQDATA_T*] data sequences created by genScalData
 *     main         - [char string] main codon sequence
 *     match        - [char string] match codon sequence
 *     maxValidation- [Integer] longest matching validation string.
 *   simMatrix      - [SIMMATRIX_T*] codon similarity created by genSimMatrix
 *     similarity   - [2D array int8] 1-based codon/codon similarity table
 *     aminoAcid    - [1D char vector] 1-based codon to aminoAcid table
 *     bases        - [1D char vector] 1-based encoding to base letter table
 *     codon        - [64 x 3 char array] 1-based codon to base letters table
 *     encode       - [uint8 vector] aminoAcid character to last codon number
 *     hyphen       - [uint8] encoding representing a hyphen (gap or space)
 *     exact        - [integer] value for exactly matching codons
 *     similar      - [integer] value for similar codons (same amino acid)
 *     dissimilar   - [integer] value for all other codons
 *     gapStart     - [integer] penalty to start gap (>=0)
 *     gapExtend    - [integer] penalty to for each codon in the gap (>0)
 *     matchLimit   - [integer] longest match including hyphens
 *   numThreads     - [integer] number of OpenMP threads
 *   numReports     - [numThreads][integer] number of reports per thread
 *   goodEndsI      - [numThreads][reports][integer] main endpoints
 *   goodEndsJ      - [numThreads][reports][integer] match endpoints
 *   goodScores     - [numThreads][reports][long long] scores for reports
 * maxReports       - [integer] maximum number of endpoints reported
 * minSeparation    - [integer] minimum startpoint separation in codons
 * maxDoublings     - [integer] maximum times to double sizeT
 *
 * OUTPUT
 * B                - [BSTR_T*] results from scanBackward
 *   numThreads     - [integer] number of OpenMP threads
 *   numReports     - [numThreads][integer] number of reports per thread
 *   bestStartsI    - [numThreads][reports][integer] main startpoints
 *   bestStartsJ    - [numThreads][reports][integer] match startpoints
 *   bestEndsI      - [numThreads][reports][integer] main endpoints
 *   bestEndsJ      - [numThreads][reports][integer] match endpoints
 *   bestSeqsI      - [numThreads][reports][unsigned char] main alignment sequences
 *   bestSeqsJ      - [numThreads][reports][unsigned char] match alignment sequences
 *   bestScores     - [numThreads][reports][long long] scores for reports
 *
 * The following is a comment that was included in the Matlab version 0.6
 * code from which this C code was derived:
 *
 * "NOTE: While this general technique is valid, and while this code seems
 * to work in the cases we have tested, there is a good chance that BUGS
 * still lurk here.  The problems come from the fact the back-scan is not
 * necessarily entirely symmetric with the forward scan, and in those cases
 * it may not get a correct result.  Corrections are welcome."
 */

BSTR_T *scanBackward(ASTR_T *A, int maxReports, int minSeparation, int maxDoublings) {

  int i, j, m, n, r, c, sizeT, bestR, ei, ej, doublings, matchLimit;
  int gapStart, gapExtend, gapFirst, maxThreads, threadNum, myTaskID;
  int npRow, npCol, myRow, myCol, iBeg, jBeg, iFin, jFin, iEnd, jEnd;
  int *bestStartsI, *bestStartsJ, *bestEndsI, *bestEndsJ;
  unsigned char *mainSeq, *matchSeq, **T;
  unsigned char **bestSeqsI, **bestSeqsJ;
  long long *bestScores, goal;
  char **weights, hyphen;
  BSTR_T *B=NULL;

  /*
   * Get the maximum number of OpenMP threads from the A structure.
   * For MPI, maxThreads==1 so that bestScores, bestStartsI, bestStartsJ,
   * bestEndsI, bestEndsJ, bestSeqsI and bestSeqsJ are essentially one
   * dimensional arrays (as is the case for single-threaded execution).
   */

  maxThreads = A->numThreads;

  /*
   * Get the maximum length of a biologically interesting sequence
   * from the A structure.
   */

  matchLimit = A->simMatrix->matchLimit;

  /* Allocate and initialize the B structure. */

  if ( (B = (BSTR_T*)malloc( sizeof(BSTR_T) ) ) == NULL ) {
    printf("scanBackward: cannot allocate B\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }
  B->numThreads = 0;
  B->numReports = NULL;
  B->bestScores = NULL;
  B->bestStartsI = B->bestStartsJ = NULL;
  B->bestEndsI = B->bestEndsJ = NULL;
  B->bestSeqsI = B->bestSeqsJ = NULL;

  /*
   * Allocate numReports, bestScores, bestStarts, bestEnds and bestSeqs
   * arrays for each thread, using 0-based indexing because thread numbers
   * start at 0.  Set defaults in case a thread does not lie on the
   * computing grid.
   */

  B->numThreads = A->numThreads;
  if ( (B->numReports = (int*)malloc(maxThreads * sizeof(int))) == NULL ) {
    printf("scanBackward: cannot allocate B->numReports\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (B->bestScores = 
	(long long**)malloc(maxThreads * sizeof(long long*))) == NULL ) {
    printf("scanBackward: cannot allocate B->bestScores\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (B->bestStartsI = (int**)malloc(maxThreads * sizeof(int*))) == NULL ) {
    printf("scanBackward: cannot allocate B->bestStartsI\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (B->bestStartsJ = (int**)malloc(maxThreads * sizeof(int*))) == NULL ) {
    printf("scanBackward: cannot allocate B->bestStartsJ\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (B->bestEndsI = (int**)malloc(maxThreads * sizeof(int*))) == NULL ) {
    printf("scanBackward: cannot allocate B->bestEndsI\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (B->bestEndsJ = (int**)malloc(maxThreads * sizeof(int*))) == NULL ) {
    printf("scanBackward: cannot allocate B->bestEndsJ\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (B->bestSeqsI =
	(unsigned char***)malloc(maxThreads * sizeof(unsigned char**)))
       == NULL ) {
    printf("scanBackward: cannot allocate B->bestSeqsI\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  if ( (B->bestSeqsJ =
	(unsigned char***)malloc(maxThreads * sizeof(unsigned char**)))
       == NULL ) {
    printf("scanBackward: cannot allocate B->bestSeqsJ\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  for (i = 0; i < maxThreads; i++) {
    B->numReports[i] = 0;
    B->bestScores[i] = NULL;
    B->bestStartsI[i] = B->bestStartsJ[i] = NULL;
    B->bestEndsI[i] = B->bestEndsJ[i] = NULL;
    B->bestSeqsI[i] = B->bestSeqsJ[i] = NULL;
  }

  /*
   * Create a parallel region to distribute the computing amongst multiple
   * OpenMP threads.  The approach to parallelization follows the theory
   * presented in the ssca1-0.6/doc/parallelization.txt document.  The
   * O(nm) conceptual matrix is decomposed into s squares whose size is
   * sqrt(nm/s) + (k-1) where k is the matchLimit parameter.  Kernel 2
   * treats each square independently for backtracking to the start points,
   * and so requires no communication between threads and no shared data.
   * However, the results of the backtracking must be combined so that
   * the best score from all threads be reported, and for this purpose
   * the B structure is provided to collect results from each thread.
   */

#pragma omp parallel \
  firstprivate(maxReports, matchLimit, minSeparation) \
  private (i, j, m, n, r, c, npRow, npCol, myRow, myCol, weights,\
           iBeg, jBeg, iFin, jFin, iEnd, jEnd, doublings,  hyphen, \
           gapStart, gapExtend, gapFirst, sizeT, bestR, goal, ei, ej, T, \
           bestScores, bestStartsI, bestStartsJ, bestEndsI, bestEndsJ, \
           bestSeqsI, bestSeqsJ, mainSeq, matchSeq, threadNum, myTaskID)
  {

    /* Map the OpenMP threads onto a square computing grid. */

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
      printf("scanBackward: cannot get myTaskID\n");
      MPI_Finalize();
      exit (1);
    }
#else
    myTaskID = threadNum;
#endif

    /* Do nothing if the thread does not lie on the process grid. */

    if (myRow >= 0 && myCol >= 0) {

      /* Make some private copies of input variables. */

      hyphen = A->simMatrix->hyphen;
      gapStart = A->simMatrix->gapStart;
      gapExtend = A->simMatrix->gapExtend;
      gapFirst = gapStart + gapExtend;     /* penalty for first codon in gap */

      /*
       * Allocate and zero the bestScores, bestStarts and bestEnds arrays.
       * Allocate the bestSeqs arrays and initialize the elements to unsigned
       * char strings that are long enough to hold a maximum-length alignment
       * sequence, including a '\0' termination character.
       *
       * Extend all indices by 1 to permit 1-based indexing, except for the
       * unsigned char strings that are accessed via 0-based indexing.
       */

      if ( (bestScores = (long long*)malloc((maxReports+1)*sizeof(long long)))
	   == NULL ) {
	printf("scanBackward: cannot allocate bestScores for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (bestStartsI = (int*)malloc((maxReports+1)*sizeof(int))) == NULL ) {
	printf("scanBackward: cannot allocate bestStartsI for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (bestStartsJ = (int*)malloc((maxReports+1)*sizeof(int))) == NULL ) {
	printf("scanBackward: cannot allocate bestStartsJ for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (bestEndsI = (int*)malloc((maxReports+1)*sizeof(int))) == NULL ) {
	printf("scanBackward: cannot allocate bestEndsI for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (bestEndsJ = (int*)malloc((maxReports+1)*sizeof(int))) == NULL ) {
	printf("scanBackward: cannot allocate bestEndsJ for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (bestSeqsI =
	    (unsigned char**)malloc((maxReports+1)*sizeof(unsigned char*)))
	   == NULL ) {
	printf("scanBackward: cannot allocate bestSeqsI for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

       if ( (bestSeqsJ =
	    (unsigned char**)malloc((maxReports+1)*sizeof(unsigned char*)))
	   == NULL ) {
	printf("scanBackward: cannot allocate bestSeqsJ for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (i = 1; i <= maxReports; i++) {
	bestScores[i] = 0L;
	bestStartsI[i] = bestStartsJ[i] = bestEndsI[i] = bestEndsJ[i] = 0;

	if ( (bestSeqsI[i] =
	      (unsigned char*)malloc((matchLimit+1)*sizeof(unsigned char)))
	     == NULL ) {
	  printf("scanBackward: cannot allocate bestSeqsI[%d] for thread %d\n",
		 i, myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
	}

	if ( (bestSeqsJ[i] =
	      (unsigned char*)malloc((matchLimit+1)*sizeof(unsigned char)))
	     == NULL ) {
	  printf("scanBackward: cannot allocate bestSeqsJ[%d] for thread %d\n",
		 i, myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
	}
      }

      /*
       * Determine the start and end indices for each thread, assuming
       * that the n*m (conceptual) matrix is subdivided into npRow*npCol
       * rectangles.  Each rectangle overlaps the next rectangle by matchLimit-1
       * in both the i and j directions, so that an alignment sequence
       * may start in the final position of a rectangle and have a maximum
       * length of matchLimit.
       *
       * In the calculations immediately below, iBeg and jBeg are the
       * offsets to the beginning of required main and match sequence
       * data respectively, iFin and jFin are the offsets to the end
       * of required main and match sequence data respectively (assuming
       * no overlap of rectangles), and iEnd and jEnd are the offsets
       * to the end of required main and match sequence data respectively
       * (assuming overlapping rectangles).
       */

      n = A->seqData->mainLen;
      m = A->seqData->matchLen;

      iBeg = 1 + (n*myRow)/npRow;
      jBeg = 1 + (m*myCol)/npCol;

      iFin = (n*(myRow+1))/npRow;
      jFin = (m*(myCol+1))/npCol;

      iEnd = MIN( n, iFin + matchLimit - 1 );
      jEnd = MIN( m, jFin + matchLimit - 1 );

      /*
       * For OpenMP and MPI, copy portions of the sequence arrays so as to have
       * private copies.  For these copies, the indexing will be 1-based.
       */

      if ( (mainSeq =
	    (unsigned char*)malloc((iEnd - iBeg + 2) * sizeof(unsigned char)))
	== NULL ) {
	printf("scanBackward: cannot allocate mainSeq for thread %d\n",
	       myTaskID);
	exit (1);
      }

      for (i = iBeg; i <= iEnd; i++) {
	mainSeq[i - iBeg + 1] = A->seqData->main[i];
      }

      if ( (matchSeq =
	    (unsigned char*)malloc((jEnd - jBeg + 2) * sizeof(unsigned char)))
	   == NULL) {
	printf("scanBackward: cannot allocate matchSeq for thread %d\n",
	       myTaskID);
	exit (1);
      }

      for (j = jBeg; j <= jEnd; j++) {
	matchSeq[j - jBeg + 1] = A->seqData->match[j];
      }

      /*
       * Copy the similarity matrix in order to have a private copy.
       * Allocate the matrix in the parallel region to ensure that
       * it is indeed a private copy.
       */

      if ( (weights = (char**)malloc( SIM_SIZE * sizeof(char*) ) ) == NULL ) {
	printf("scanBackward: cannot allocate weights for thread %d\n",
	       myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (i = 1; i < SIM_SIZE; i++) {
	if ( (weights[i] = (char*)malloc( SIM_SIZE * sizeof(char) ) )
	     == NULL ) {
	  printf("locateSimilar: cannot allocates weights[%d] for thread %d\n",
		 i, myTaskID);
	}
	for (j = 1; j < SIM_SIZE; j++) {
	  weights[i][j] = A->simMatrix->similarity[i][j];
	}
      }

      /*
       * Preallocate working storage (1-based indexing) for use in
       * tracing exact sequences.  The initial value can be doubled
       * MAX_DOUBLINGS times before giving up.  An initial value
       * equal to the matchLimit (which is the maximum length of
       * a biologically interesting sequence and which ought to
       * be larger than maxValidation) will allow a terminal gap
       * of this length before doScan quits due to a tracking
       * table overflow, allowing a doubling of the size of the
       * tracking table.
       */

      sizeT = MAX(A->simMatrix->matchLimit, A->seqData->maxValidation);
      doublings = 0;

    retry:
      if ( (T = (unsigned char**)malloc((sizeT+1) * sizeof(unsigned char*)))
	   == NULL ) {
	printf("scanBackward: cannot allocate T for thread %d\n", myTaskID);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (i = 1; i <= sizeT; i++) {
	if ( (T[i] = (unsigned char*)malloc((sizeT+1)*sizeof(unsigned char)))
	     == NULL ) {
	  printf("scanBackward: cannot allocate T[%d] for thread %d\n",
		 i, myTaskID);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	for (j = 1; j <= sizeT; j++) {
	  T[i][j] = 0;
	}
      }

      /* Go find all of the matches that end in goodEnds. */

      bestR = 0;
      for (r = 1; r <= A->numReports[threadNum]; r++) {
	goal = A->goodScores[threadNum][r];             /* predicted score */
	ei = A->goodEndsI[threadNum][r];                /* main endpoint */
	ej = A->goodEndsJ[threadNum][r];                /* match endpoint */
	for (i = 1; i <= sizeT; i++) {
	  for (j = 1; j <= sizeT; j++) {
	    T[i][j] = 0;                       /* Zero the working storage. */
	  }
	}

	/* Scan for matches.  Retry with a larger T matrix if necessary. */

	if ( doScan(A, T, sizeT, ei, ej, mainSeq, matchSeq, weights,
		    gapFirst, gapExtend, minSeparation, r, goal,
		    iBeg, jBeg, iFin, jFin, iEnd, jEnd, myTaskID,
		    &bestR, bestStartsI, bestStartsJ, bestEndsI, bestEndsJ,
		    bestSeqsI, bestSeqsJ, bestScores) == -1 ) {

	  /* Double the size of the matrix T if it is allowed. */

	  if (doublings >= maxDoublings) {
	    printf("\tscanBackward: sequence %d dropped for thread %d\n",
		   r, myTaskID);
	  } else {

	    /* Free the current matrix T. */

	    for (i = 1; i <= sizeT; i++) {
	      free(T[i]);
	    }
	    free(T);
	    sizeT *= 2;
#if !defined(SPEC)
	    printf("\tscanBackward: doubling sizeT to %d for thread %d\n",
		   sizeT, myTaskID);
#endif
	    doublings++;
	    goto retry;
	  }
	}

	/* Quit if the number of requested results is found. */

	if (bestR == maxReports) {
	  break;
	}
      }

      /* 
       * Copy the results to the B structure.  It is possible to copy
       * only the pointer to each array, but a copy of each element
       * is performed in order that the arrays of the structure B be
       * no larger than is necessary.
       */

      B->numReports[threadNum] = bestR;

      /* Allocate arrays and fill them if there are any matches (reports). */

      if (bestR != 0) {
	if ( (B->bestScores[threadNum] =
	      (long long*)malloc((bestR +1) * sizeof(long long))) == NULL ) {
	  printf("scanBackward: cannot allocate B->bestScores for thread %d\n",
		 myTaskID);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	if ( (B->bestStartsI[threadNum] =
	      (int*)malloc((bestR+1) * sizeof(int))) == NULL ) {
	  printf("scanBackward: cannot allocate B->bestStartsI for thread %d\n",
		 myTaskID);
	
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	if ( (B->bestStartsJ[threadNum] =
	      (int*)malloc((bestR+1) * sizeof(int))) == NULL ) {
	  printf("scanBackward: cannot allocate B->bestStartsJ for thread %d\n",
		 myTaskID);
	
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	if ( (B->bestEndsI[threadNum] =
	      (int*)malloc((bestR+1) * sizeof(int))) == NULL ) {
	  printf("scanBackward: cannot allocate B->bestEndsI for thread %d\n",
		 myTaskID);
	
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	if ( (B->bestEndsJ[threadNum] =
	      (int*)malloc((bestR+1) * sizeof(int))) == NULL ) {
	  printf("scanBackward: cannot allocate B->bestEndsJ for thread %d\n",
		 myTaskID);
	
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	if ( (B->bestSeqsI[threadNum] =
	      (unsigned char**)malloc((bestR + 1) * sizeof(unsigned char*)))
	     == NULL ) {
	  printf("scanBackward: cannot allocate B->bestSeqsI for thread %d\n",
		 myTaskID);
	
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	if ( (B->bestSeqsJ[threadNum] =
	      (unsigned char**)malloc((bestR + 1) * sizeof(unsigned char*)))
	     == NULL ) {
	  printf("scanBackward: cannot allocate B->bestSeqsJ for thread %d\n",
		 myTaskID);
	
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	/* Copy the elements of the arrays. */

	for (j = 1; j <= bestR; j++) {
	  B->bestScores[threadNum][j] = bestScores[j];
	  B->bestStartsI[threadNum][j] = bestStartsI[j];
	  B->bestStartsJ[threadNum][j] = bestStartsJ[j];
	  B->bestEndsI[threadNum][j] = bestEndsI[j];
	  B->bestEndsJ[threadNum][j] = bestEndsJ[j];
	  B->bestSeqsI[threadNum][j] = copyStringR(bestSeqsI[j]);
	  B->bestSeqsJ[threadNum][j] = copyStringR(bestSeqsJ[j]);
	}

	/* Check the validity of the sequence strings in B->bestSeqsI/J. */

	if (B->bestSeqsI == NULL) {
	  printf("scanBackward: B->bestSeqsI == NULL for thread %d\n",
		 myTaskID);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}
      
	if (B->bestSeqsI[threadNum] == NULL) {
	  printf("scanBackward: B->bestSeqsI[%d] = NULL for thread\n",
		 threadNum, myTaskID);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}
      
	c = 0;
	for (i = 1; i < B->numReports[threadNum]; i++) {
	  if ( strlen( (char*)(B->bestSeqsI[threadNum][i]) ) < 1 ) {
	    c++;
	  }
	}

	if (c) {
	  printf("scanBackward: 0-length bestSeqsI strings %d of %d reports for thread %d\n",
		 c, B->numReports[threadNum], myTaskID);
	}

	if (B->bestSeqsJ == NULL) {
	  printf("scanBackward: B->bestSeqsJ == NULL for thread %d\n",
		 myTaskID);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}

	if (B->bestSeqsJ[threadNum] == NULL) {
	  printf("scanBackward: B->bestSeqsJ[%d] = NULL for thread\n",
		 threadNum, myTaskID);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}
      
	c = 0;
	for (i = 1; i < B->numReports[threadNum]; i++) {
	  if ( strlen( (char *)(B->bestSeqsJ[threadNum][i]) ) < 1 ) {
	    c++;
	  }
	}
	if (c) {
	  printf("scanBackward: 0-length bestSeqsJ strings %d of %d reports for thread %d\n",
		 c, B->numReports[threadNum], myTaskID);
	}
      }

     /* Free the dynamic arrays that were allocated in this parallel region. */

      free(bestScores);
      free(bestStartsI);
      free(bestStartsJ);
      free(bestEndsI);
      free(bestEndsJ);

      for (i = 1; i <= maxReports; i++) {
	free(bestSeqsI[i]);
	free(bestSeqsJ[i]);
      }

      free(bestSeqsI);
      free(bestSeqsJ);

      for (i = 1; i < SIM_SIZE; i++) {
	free(weights[i]);
      }
      free(weights);

      for (i = 1; i <= sizeT; i++) {
	free(T[i]);
      }
      free(T);

      free(mainSeq);
      free(matchSeq);
    }
  }

  return (B);
}

/* Function freeB() - free the B structure and return NULL. */

BSTR_T *freeB(BSTR_T *B) {

  int i, j;

  if (B) {
    if (B->bestScores) {
      for (i = 0; i < B->numThreads; i++) {
	if (B->bestScores[i]) {
	  free(B->bestScores[i]);
	  B->bestScores[i] = NULL;
	}
      }
      free(B->bestScores);
      B->bestScores = NULL;
    }
    if (B->bestStartsI) {
      for (i = 0; i < B->numThreads; i++) {
	if (B->bestStartsI[i]) {
	  free(B->bestStartsI[i]);
	  B->bestStartsI[i] = NULL;
	}
      }
      free(B->bestStartsI);
      B->bestStartsI = NULL;
    }
    if (B->bestStartsJ) {
      for (i = 0; i < B->numThreads; i++) {
	if (B->bestStartsJ[i]) {
	  free(B->bestStartsJ[i]);
	  B->bestStartsJ[i] = NULL;
	}
      }
      free(B->bestStartsJ);
      B->bestStartsJ = NULL;
    }
    if (B->bestEndsI) {
      for (i = 0; i < B->numThreads; i++) {
	if (B->bestEndsI[i]) {
	  free(B->bestEndsI[i]);
	  B->bestEndsI[i] = NULL;
	}
      }
      free(B->bestEndsI);
      B->bestEndsI = NULL;
    }
    if (B->bestEndsJ) {
      for (i = 0; i < B->numThreads; i++) {
	if (B->bestEndsJ[i]) {
	  free(B->bestEndsJ[i]);
	  B->bestEndsJ[i] = NULL;
	}
      }
      free(B->bestEndsJ);
      B->bestEndsJ = NULL;
    }
    if (B->bestSeqsI) {
      for (i = 0; i < B->numThreads; i++) {
	if (B->bestSeqsI[i]) {
	  for (j = 1; j <= B->numReports[i]; j++) {
	    if (B->bestSeqsI[i][j]) {
	      free(B->bestSeqsI[i][j]);
	      B->bestSeqsI[i][j] = NULL;
	    }
	  }
	  free(B->bestSeqsI[i]);
	  B->bestSeqsI[i] = NULL;
	}
      }
      free(B->bestSeqsI);
      B->bestSeqsI = NULL;
    }
    if (B->bestSeqsJ) {
      for (i = 0; i < B->numThreads; i++) {
	if (B->bestSeqsJ[i]) {
	  for (j = 1; j <= B->numReports[i]; j++) {
	    if (B->bestSeqsJ[i][j]) {
	      free(B->bestSeqsJ[i][j]);
	      B->bestSeqsJ[i][j] = NULL;
	    }
	  }
	  free(B->bestSeqsJ[i]);
	  B->bestSeqsJ[i] = NULL;
	}
      }
      free(B->bestSeqsJ);
      B->bestSeqsJ = NULL;
    }
    if (B->numReports) {
      free(B->numReports);
      B->numReports = NULL;
    }
    free(B);
  }
  return (NULL);
}
