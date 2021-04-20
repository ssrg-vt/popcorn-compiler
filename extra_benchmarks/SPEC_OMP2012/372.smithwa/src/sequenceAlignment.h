/*
 * sequenceAlignment.h
 *
 * Defines the various functions.
 *
 * Russ Brown
 */

/* @(#)sequenceAlignment.h	1.106 11/04/29 */

#ifndef SEQUENCEALIGNMENT_H
#define SEQUENCEALIGNMENT_H

#if defined(SPEC)
#if defined(MPI)
#undefine MPI
#endif
#endif


#if defined(SPEC_OMP)
#include <omp.h>
#endif

#ifdef MPI
#include <mpi.h>
#endif

/* Use clock_gettime() function for high resolution time. */

#if !defined(SPEC)
#define HR_TIME
#endif

/* Use '#pragma omp flush' in lieu of 'pragma omp barrier' in mergeScores() */

#define SPEC_OMP_FLUSH

/* Print the sequence match time from Kernel 1. */

#define MATCH_TIME

/* Print the reduction time from Kernel 2B. */

#define MERGE_TIME

/* These parameters control messages in doScan() and tracePath(). */

#define DOSCAN_NORMAL_ERRORS
#define DOSCAN_SIZET_OVERFLOW
#if defined(SPEC)
#undef DOSCAN_NORMAL_ERRORS
#undef DOSCAN_SIZET_OVERFLOW
#endif
#undef DOSCAN_NORMAL_MESSAGES
#undef TRACEPATH_NORMAL_ERRORS
#undef TRACEPATH_NORMAL_MESSAGES
#define CONSIDERADDING_NORMAL_MESSAGES

/*
 * Half of the largest-magnitude negative 64-bit number, which
 * gives the doScan function of scanBackward.c plenty of room
 * to accumulate scores in either the negative or positive direction.
 * Also used in the linkToBestForRow function of treeSortAlign.c.
 */

#define MINUS_INFINITY (0XB000000000000000L)

#define K2_MAX_DOUBLINGS (3)
/*
 * Structure for similarity matrix
 *
 * Note: all array indices are one longer than in the equivalent Matlab
 * code so that index mapping from 1-based to 0-based is not necessary.
 */

#define MULTIPLET_WORD_SIZE (64)
#define SIM_DIM (64)
#define SIM_MASK (SIM_DIM - 1)
#define SIM_SIZE (SIM_DIM + 1)
#define AMINO_SIZE (SIM_SIZE + 1)
#define CODON_SIZE (SIM_SIZE + 1)
#define ENCODE_SIZE (SIM_DIM + SIM_SIZE)

/***********************************************************************
                            SYMMATRIX_T
************************************************************************/

/* Structure for the similarity matrix. */

typedef struct simmat {
  char similarity[SIM_SIZE][SIM_SIZE];
  char aminoAcid[AMINO_SIZE];
  char *bases;
  char *codon[CODON_SIZE];
  unsigned char encode[ENCODE_SIZE];
  unsigned char encode_first[ENCODE_SIZE];
  char hyphen, star;
  int exact, similar, dissimilar, gapStart, gapExtend, matchLimit;
} SIMMATRIX_T;

/***********************************************************************
                            SEQDATA_T
************************************************************************/

/* Structure for scalable data */

typedef struct seqdat {
  unsigned char *main, *match;
  int mainLen, matchLen, maxValidation;
} SEQDATA_T;

/***********************************************************************
                            ASTR_T
************************************************************************/

/* Structure for the Kernel 1 return values */

typedef struct astr {
  SEQDATA_T *seqData;
  SIMMATRIX_T *simMatrix;
  long long **goodScores;
  int numThreads, *numReports, **goodEndsI, **goodEndsJ;
} ASTR_T;

/***********************************************************************
                            BSTR_T
************************************************************************/

/* Structure for the Kernel 2A return values */

typedef struct bstr {
  long long **bestScores;
  int numThreads, *numReports;
  int **bestStartsI, **bestStartsJ, **bestEndsI, **bestEndsJ;
  unsigned char ***bestSeqsI, ***bestSeqsJ;
} BSTR_T;

/***********************************************************************
                            CSTR_T
************************************************************************/

/* Structure for the Kernel 2B return values */

typedef struct cstr {
  long long *finalScores;
  int numReports;
  int *finalStartsI, *finalStartsJ, *finalEndsI, *finalEndsJ;
  unsigned char **finalSeqsI, **finalSeqsJ;
} CSTR_T;

/* The SCALE parameter adjusts the problem size in powers of two. */

#define SCALE (30)

/* Scalable Data Generator parameters */

#define MAIN_SEQ_LENGTH  (1 << (SCALE/2)) // Total main codon sequence length
#define MATCH_SEQ_LENGTH (1 << (SCALE/2)) // Total match codon sequence length

/* Kernel parameters */

/* Kernels 1 and 2 codon sequence similarity scoring function */
#define SIM_EXACT        (5)           // >0  Exact codon match
#define SIM_SIMILAR      (4)           //     Amino acid (or Stop) match
#define SIM_DISSIMILAR   (-3)          // <0  Different amino acids
#define GAP_START        (8)           // >=0 Gap-start penalty
#define GAP_EXTEND       (1)           // >0  Gap-extension penalty
#define MATCH_LIMIT      (60)          // Longest interesting match

/* Kernel 1 */
#define K1_MIN_SCORE      (20)      // >0  Minimum end-point score
#define K1_MIN_SEPARATION (5)       // >=0 Minimum end-point separation
#define K1_MAX_REPORTS    (200)     // >0  Maximum end-points reported to K2

/* Kernel 2 */
#define K2_MIN_SEPARATION K1_MIN_SEPARATION  // >=0 Minimum start-point sep.
#define K2_MAX_DOUBLINGS    (3)              // max sizeT doublings (scanBackward)
#define K2_MAX_REPORTS    (K1_MAX_REPORTS/2) // >0 Max seq. reported to K2B & K3
#if defined(SPEC)
#define K2A_DISPLAY        (6)              // >=0 #K2A reports to display
#define K2B_DISPLAY        (6)              // >=0 #K2B reports to display
#else
#define K2A_DISPLAY        (10)              // >=0 #K2A reports to display
#define K2B_DISPLAY        (15)              // >=0 #K2B reports to display
#endif
#undef K2A_SUMMARY                          // display K2A summary
#undef K2A_REPORTS                          // display K2A summary and reports

/*
 * Global verification program control parameters, which may not apply
 * to the C implementation.
 *
 * WARNING: THESE MODES BELOW ARE _NOT_ SCALABLE HENCE
 * SHOULD BE SET TO '0' WHEN TESTING WITH LARGE SCALE DATA 
 * OR WHEN DOING TIMING PERFORMANCE SCALABILITY STUDIES.
 */
#define ENABLE_PAUSE (0)   // Pauses used in demo mode. 
#define ENABLE_VERIF (1)   // Display built-in code verification.
#define ENABLE_DEBUG (0)   // Keep large data structures for later perusal.

/* Here are the function definitions. */

#define MAX(x, y) ( (x > y) ? x : y )
#define MIN(x, y) ( (x < y) ? x : y )

void getUserParameters(void);

SEQDATA_T *genScalData(unsigned int, SIMMATRIX_T*, int, int, int);

SEQDATA_T *freeSeqData(SEQDATA_T*);

SIMMATRIX_T *genSimMatrix(int, int, int, int, int, int, int);

SIMMATRIX_T *freeSimMatrix(SIMMATRIX_T*);

void verifyData(SIMMATRIX_T*, SEQDATA_T*, int, int);

int gridInfo(int*, int*, int*, int*);

void qSort(int*, const int*, const int, const int);

void qSort_both(long long*, int*, const long long*, const int);

ASTR_T *pairwiseAlign(SEQDATA_T*, SIMMATRIX_T*, int, int, int);

ASTR_T *freeA(ASTR_T*);

BSTR_T *scanBackward(ASTR_T*, int, int, int);

BSTR_T *freeB(BSTR_T*);

void verifyAlignment(SIMMATRIX_T*, BSTR_T*, int);

CSTR_T *mergeAlignment(BSTR_T*, int, int);

CSTR_T *freeC(CSTR_T*);

void verifyMergeAlignment(SIMMATRIX_T*, CSTR_T*, int);

double getSeconds(void);

void dispElapsedTime(double);

#endif
