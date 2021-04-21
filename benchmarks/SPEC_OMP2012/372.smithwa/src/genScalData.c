/*
 * genScalData.c
 *
 * Generate and copy the scalable data.
 *
 * Russ Brown
 */

/* @(#)genScalData.c	1.22 10/08/23 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sequenceAlignment.h"

#if defined(SPEC)
#include "specrand.h"
#endif 

#if defined(SPEC)
/*#define SW_RAND_MAX (1073741823)*/
/*#define SW_SHIFT    (2)*/
#define SW_RAND_MAX (32767)
#define SW_SHIFT    (17)
#else
#define SW_RAND_MAX (RAND_MAX)
#endif 

/* Here is the number of validation sequences plus 1. */

#define VALIDATION_LENGTH (7)

/*
 * Function insertValidation - insert validation sequences at random
 * points in a codon sequence.
 */

static
unsigned char *insertValidation(unsigned char *oldSeq, int *seqLen,
				char *insertionStrings[],
				SIMMATRIX_T *simMatrix,
				int firstLastSelect[]) {

  int i, j, k, len, start, startArray[VALIDATION_LENGTH];
  unsigned char *codSeq, *newSeq;

  /*
   * Create random start points for insertion,
   * even at the end of the sequence.
   */

  for (i = 1; i < VALIDATION_LENGTH; i++) {
#if defined(SPEC)
    startArray[i] = (int)ceil( (double)(*seqLen + 1) *
         (double) ((unsigned long) spec_genrand_int32() >> SW_SHIFT) 
         / (double)SW_RAND_MAX );
#else
    startArray[i] = (int)ceil( (double)(*seqLen + 1) *
			       (double)rand() / (double)SW_RAND_MAX );
#endif
  }

  /* Insert the random sequences. */

  for (i = 1; i < VALIDATION_LENGTH; i++) {

    /*
     * Allocate a 0-based array and fill it with numeric codons that
     * are translated from the insertionStrings character array.  The
     * firstLastSelect flag determines whether the first or last codon
     * that maps to the amino acid (see the genSimMatrix function) is
     * used for the fill. 
     */

    len = strlen(insertionStrings[i]);
    if ( (codSeq =
	  (unsigned char*)malloc(len*sizeof(unsigned char))) == NULL ) {
      printf("insertValidation: cannot allocate codSeq\n");
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }
    if (firstLastSelect[i] == 1) {
      for (j = 0; j < len; j++) {
	codSeq[j] = simMatrix->encode_first[ (int)insertionStrings[i][j] ];
      }
    } else {
      for (j = 0; j < len; j++) {
	codSeq[j] = simMatrix->encode[ (int)insertionStrings[i][j] ];
      }
  }

    /* Insert the codon sequence into the sequence at the random start point. */

    start = startArray[i];
    if ( (newSeq =
	  (unsigned char*)malloc((len+*seqLen+1)*sizeof(unsigned char)))
	 == NULL ) {
      printf("insertValidation: cannot allocate newSeq\n");
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }
    for (j = 1; j < start; j++) {
      newSeq[j] = oldSeq[j];
    }
    for (j = 0; j < len; j++) {
      newSeq[j + start] = codSeq[j];
    }
    for (j = start + len; j <= *seqLen + len; j++) {
      newSeq[j] = oldSeq[j - len];
    }

    /* Increment the sequence length. */

    *seqLen += len;

    /*
     * Free the old sequence and the codon sequence, and assign
     * the new (lengthened) sequence to the old sequence.
     */

    free(oldSeq);
    free(codSeq);
    oldSeq = newSeq;

    /*
     * Find and adjust entries in the startArray array that are >= start
     * so that any future insertions will occur at the intended position
     * of the unaltered (i.e., pre-insertion) sequence.
     */

    for (k = 1; k < VALIDATION_LENGTH; k++) {
      if (startArray[k] >= start) {
	startArray[k] = startArray[k] + len;
      }
    }
  }

  /* Return the old (now lengthened) sequence. */

  return (oldSeq);
}

/*
 * Function genScalData - Scalable Data Generator.
 *
 * This function generates scalable data for later use.  It produces two
 * uniformly-distributed random sequences of codons, then inserts validation
 * sequences at randomly-selected points within the sequences.
 *
 * For a detailed description of the SSCA #1 Optimal Pattern Matching* 
 * problem, please see the SSCA #1 Written Specification.
 *
 * INPUT
 * P               - [structure] global parameters, see initMatlabMPI.m
 * simMatrix       - [structure] codon similarity created by genSimMatrix()
 * mainLen         - [Integer] number of codons in main sequence.
 * matchLen        - [Integer] number of codons in match sequence.
 * simSize         - [Integer] number of characters in the alphabet
 *
 * OUTPUT
 * seqData         - [structure] holds the generated sequences.
 *   main          - [1D uint8 array] generated main codon sequence (1-64).
 *   match         - [1D uint8 array] generated match codon sequence (1-64).
 *   maxValidation - [Integer] longest matching validation string.
 *   mainLen       - [Integer] length of the main array.
 *   matchLen      - [Integer] length of the match array.
 */

SEQDATA_T *genScalData(unsigned int randomSeed,
		       SIMMATRIX_T *simMatrix,
		       int mainLen, int matchLen,
		       int simSize) {

  SEQDATA_T *seqData=NULL;
  FILE *fid;
  int i, input, maxLen;

  /*
   * Here are the validations arrays for 1-based indexing to obtain
   * a character string, and 0-based indexing to access individual
   * characters within the string.
   */

  char *mainValidations[VALIDATION_LENGTH] = {
    "",
    "ACDEFG*SIMILARTESTS*HIKLMN",
    "ACDEFG*PARTIALCASES*HIKLMN",
    "ACDEFG*IDENTICAL*HIKLMN",
    "ACDEFG*MISQRMATCHES*HIKLMN",
    "ACDEFG*STARTGAPMIDSTEND*HIKLMN",
    "ACDEFG*EVENLESSWKDPALIGNS*HIKLMN"
  };

  char *matchValidations[VALIDATION_LENGTH] = {
    "",
    "MNLKIH*SIMILARTESTS*GFEDCA",
    "MNLKIH*PARTIALCASES*GFEDCA",
    "MNLKIH*IDENTICAL*GFEDCA",
    "MNLKIH*MISRQMATCHES*GFEDCA",
    "MNLKIH*STARTMIDSTGAPEND*GFEDCA",
    "MNLKIH*EVENLESSTVMFALIGNS*GFEDCA"
  };

  /*
   * Here are the "select arrays" that specify whether the first or
   * the last codon that specifies an amino acid will be inserted into
   * the sequence.  These arrays may be used to provide perfect matches
   * or partial matches between the verification sequences.
   */

  int mainSelect[VALIDATION_LENGTH] = { 0, 0, 0,  0, 0, 0, 0 };

  int matchSelect[VALIDATION_LENGTH] = { 0, 1, 1, 0, 0, 0, 1 };

  /* Allocate the seqData structure. */

  if ( (seqData = (SEQDATA_T *)malloc( sizeof(SEQDATA_T) ) ) == NULL ) {
    printf("genScalData: cannot allocate seqData\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  /*
   * Generate the random sequences and insert the validation sequences.
   * The indexing is 1-based.
   */

  seqData->mainLen = mainLen;
  if ( (seqData->main =
	(unsigned char*)malloc((seqData->mainLen+1)*sizeof(unsigned char)))
       == NULL ) {
    printf("genScalData: cannot allocate seqData->main\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  seqData->matchLen = matchLen;
  if ( (seqData->match =
	(unsigned char*)malloc((seqData->matchLen+1)*sizeof(unsigned char)))
       == NULL ) {
    printf("genScalData: cannot allocate seqData->match\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  /* The kernels must be able to locate strings at least this long. */

  maxLen = 0;
  for (i = 1; i < VALIDATION_LENGTH; i++) {
    maxLen = MAX( maxLen,
		  MAX( strlen(mainValidations[i]),
		       strlen(matchValidations[i]) ) ) - 12;
  }
  seqData->maxValidation = maxLen;

  /* Generate uniformly distributed strings of the 64 possible codons. */

#if defined(SPEC)
  spec_init_genrand((unsigned long) (randomSeed + 10));
#else
  srand(randomSeed + 10);
#endif
  for (i = 1; i <= mainLen; i++) {
#if defined(SPEC)
    seqData->main[i] =
      (unsigned char)(ceil( (double)(simSize - 2) *
          (double) ((unsigned long) spec_genrand_int32() >> SW_SHIFT) / 
                       (double)SW_RAND_MAX) + 1);
#else
    seqData->main[i] =
      (unsigned char)(ceil( (double)(simSize - 2) *
			    (double)rand() / (double)SW_RAND_MAX ) + 1);
#endif
    if (seqData->main[i] >= simSize) {
      printf("genScalData 1: seqData->main[%d] = %d\n", i, seqData->main[i]);
    }
    if (seqData->main[i] <= 0) {
      printf("genScalData 1: seqData->main[%d] = %d\n", i, seqData->main[i]);
    }
  }

  /* Use different random seeds for the main and match sequences. */

#if defined(SPEC)
  spec_init_genrand((unsigned long) (randomSeed+11));
#else
  srand(randomSeed + 11);
#endif
  for (i = 1; i <= matchLen; i++) {
#if defined(SPEC)
    seqData->match[i] =
      (unsigned char)(ceil( (double)(simSize - 2) *
          (double) ((unsigned long) spec_genrand_int32() >> SW_SHIFT) / 
                       (double)SW_RAND_MAX) + 1);
#else
    seqData->match[i] =
      (unsigned char)(ceil( (double)(simSize - 2) *
			    (double)rand() / (double)SW_RAND_MAX ) + 1);
#endif
    if (seqData->match[i] >= simSize) {
      printf("genScalData 1: seqData->match[%d] = %d\n", i, seqData->match[i]);
    }
    if (seqData->match[i] <= 0) {
      printf("genScalData 1: seqData->match[%d] = %d\n", i, seqData->match[i]);
    }
  }

  /* Insert sequences for validation. */

  seqData->main = insertValidation(seqData->main, &(seqData->mainLen),
				   mainValidations, simMatrix, mainSelect);

  seqData->match = insertValidation(seqData->match, &(seqData->matchLen),
				    matchValidations, simMatrix, matchSelect);

  /* Check that all codons lie in the range 1..64 */

  for (i = 1; i <= mainLen; i++) {
    if (seqData->main[i] >= simSize) {
      printf("genScalData 2: seqData->main[%d] = %d\n", i, seqData->main[i]);
    }
    if (seqData->main[i] <= 0) {
      printf("genScalData 2: seqData->main[%d] = %d\n", i, seqData->main[i]);
    }
  }

  for (i = 1; i <= matchLen; i++) {
    if (seqData->match[i] >= simSize) {
      printf("genScalData 2: seqData->match[%d] = %d\n", i, seqData->match[i]);
    }
    if (seqData->match[i] <= 0) {
      printf("genScalData 2: seqData->match[%d] = %d\n", i, seqData->match[i]);
    }
  }

  return (seqData);
}

/* Function freeSeqData - free the sequence data and return NULL. */

SEQDATA_T *freeSeqData(SEQDATA_T *S) {

  if (S) {
    if (S->main) {
      free(S->main);
      S->main = NULL;
    }
    if (S->match) {
      free(S->match);
      S->match = NULL;
    }
    free(S);
  }
  return (NULL);
}
