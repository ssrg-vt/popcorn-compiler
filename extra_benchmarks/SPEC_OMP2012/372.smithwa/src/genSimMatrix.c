/*
 * genSimMatrix.c
 *
 * Generate and copy the similarity matrix.
 *
 * Russ Brown
 */

/* @(#)genSimMatrix.c	1.13 07/02/01 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sequenceAlignment.h"

/*
 * Function genSimMatrix() - Generate the Kernel 1/2/3 Similarity Matrix
 *
 * The simMatrix data structure contains the data elements needed to define
 * codon-level similarity for later use by various kernels.  These include a
 * matrix which gives the similarity score for any pair of codons, a vector
 * which is used to map codons to amino acids, an array which is used to map
 * codons to base sequences, the various UI parameters, etc.
 *
 * For a detailed description of the SSCA #1 Optimal Pattern Matching problem, 
 * please see the SSCA #1 Written Specification.
 *
 * INPUT
 *
 *   exact        - [integer] value for exactly matching codons
 *   similar      - [integer] value for similar codons (same amino acid)
 *   dissimilar   - [integer] value for all other codons
 *   gapStart     - [integer] penalty to start gap (>=0)
 *   gapExtend    - [integer] penalty to for each codon in the gap (>0)
 *   matchLimit   - [integer] longest match including hyphens
 *   simSize      - [integer] number of characters in the alphabet
 *
 * OUTPUT
 *
 *   simMatrix    - [structure] holds the generated similarity parameters
 *                  See source file sequenceAlignment.h for details.
 *   similarity   - [2D array int8] 1-based codon/codon similarity table
 *   aminoAcid    - [1D char vector] 1-based codon to aminoAcid table
 *   bases        - [1D char vector] 1-based encoding to base letter table
 *   codon        - [64 x 3 char array] 1-based codon to base letters table
 *   encode       - [uint8 vector] aminoAcid character to last codon number
 *   encode_first - [uint8 vector] aminoAcid character to first codon number
 *   hyphen       - [uint8] encoding representing a hyphen (gap or space)
 *   exact        - [integer] value for exactly matching codons
 *   similar      - [integer] value for similar codons (same amino acid)
 *   dissimilar   - [integer] value for all other codons
 *   gapStart     - [integer] penalty to start gap (>=0)
 *   gapExtend    - [integer] penalty to for each codon in the gap (>0)
 *   matchLimit   - [integer] longest interesting biological sequence
 */
 
SIMMATRIX_T *genSimMatrix(int exact, int similar, int dissimilar,
			  int gapStart, int gapExtend, int matchLimit,
			  int simSize) {

  SIMMATRIX_T *simMatrix=NULL;
  char *aa, *codon, base;
  int i, j, k, ccode, ccode2;

  /*
   * Here is the Kernels 1/2/3 similarity matrix, extended by one index
   * to avoid 1-based (Matlab) to 0-based (C) mapping.  The first
   * element of each row is a NULL string.  The element following the
   * last valid element of each row is also the NULL string to signal
   * the end of valid data.  This similarity matrix should be static
   * because pointers to its strings are stored in the simMatrix->codon
   * array.
   */

  static char *similarities[22][9] =
  {
    {"", ""},
    {"", "A", "gct", "gcc", "gca", "gcg", ""},
    {"", "C", "tgt", "tgc", ""},
    {"", "D", "gat", "gac", ""},
    {"", "E", "gaa", "gag", ""},
    {"", "F", "ttt", "ttc", ""},
    {"", "G", "ggt", "ggc", "gga", "ggg", ""},
    {"", "H", "cat", "cac", ""},
    {"", "I", "att", "atc", "ata", ""},
    {"", "K", "aaa", "aag", ""},
    {"", "L", "ttg", "tta", "ctt", "ctc", "cta", "ctg", ""},
    {"", "M", "atg", ""},
    {"", "N", "aat", "aac", ""},
    {"", "P", "cct", "ccc", "cca", "ccg", ""},
    {"", "Q", "caa", "cag", ""},
    {"", "R", "cgt", "cgc", "cga", "cgg", "aga", "agg", ""},
    {"", "S", "tct", "tcc", "tca", "tcg", "agt", "agc", ""},
    {"", "T", "act", "acc", "aca", "acg", ""},
    {"", "V", "gtt", "gtc", "gta", "gtg", ""},
    {"", "W", "tgg", ""},
    {"", "Y", "tat", "tac", ""},
    {"", "*", "taa", "tag", "tga", ""}
  };

  /* Allocate a similarity matrix structure for return. */

  if ( (simMatrix = (SIMMATRIX_T *)malloc( sizeof(SIMMATRIX_T) ) ) == NULL ) {
    printf("genSimMatrix: cannot allocate simMatrix\n");
#ifdef MPI
    MPI_Finalize();
#endif
    exit (1);
  }

  /*
   * The encode array (1-based) is used for verification,
   * but I don't know why 49 is used.  It is not the ascii
   * code for '*'.
   */

  simMatrix->star = 49;
  for (i = 0; i < ENCODE_SIZE; i++) {
    simMatrix->encode[i] = simMatrix->star;
  }

  /*
   * simSize (65) is larger than the largest 1-based codon (64)
   * It is not the ascii code for '-'.
   */

  simMatrix->hyphen = simSize;
  simMatrix->codon[(int)(simMatrix->hyphen)] = "---";
  simMatrix->aminoAcid[(int)(simMatrix->hyphen)] = '-';

  /*
   * Here are the bases ordered according to their codon contribution.
   * The initial space in the string permits 1-based addressing.
   */

  simMatrix->bases = " agct";

  /*
   * Encode the codons from the similarity matrix.  The last entry
   * for a particular row contains a NULL string, of length 0.
   */

  for (i = 1; i <= 21; i++) {
    aa = similarities[i][1];
    for (j = 2; strlen(similarities[i][j]) != 0; j++) {
      codon = similarities[i][j];
      ccode = 0;
      for (k = 0; k < 3; k++) {
	base = codon[k];
	switch ( base ) {
	case 'a':
	  ccode = 0 + 4*ccode;
	  break;
	case 'g':
	  ccode = 1 + 4*ccode;
	  break;
	case 'c':
	  ccode = 2 + 4*ccode;
	  break;
	case 't':
	  ccode = 3 + 4*ccode;
	  break;
	default:
	  printf("unrecognized base[%d][%d][%d] = %c\n", i, j, k, codon[k]);
	}
      }
      ccode = ccode + 1;                 /* 1-based indexing */
      if (j == 2) {
	ccode2 = ccode;                  /* save first ccode */
      }
      simMatrix->codon[ccode] = codon;
      simMatrix->aminoAcid[ccode] = aa[0];
    }
    simMatrix->encode[(int)aa[0]] = (unsigned char)ccode;
    simMatrix->encode_first[(int)aa[0]] = (unsigned char)ccode2;
  }

  /* Initialize the similarity matrix with a dissimilar opcode. */

  for (i = 1; i < simSize; i++) {
    for (j = 1; j < simSize; j++) {
      simMatrix->similarity[i][j] = (char)dissimilar;
    }
  }

  /* Mark as similar all instances that equal simMatrix->aminoAcid[ccode] */

  for (ccode = 1; ccode < simSize; ccode++) {
    for (i = 1; i < simSize; i++) {
      if (simMatrix->aminoAcid[i] == simMatrix->aminoAcid[ccode]) {
	simMatrix->similarity[ccode][i] = (char)similar;
      }
    }
  }

  /* Mark as exact all diagonal elements of the similarity matrix. */

  for (i = 1; i < simSize; i++) {
    simMatrix->similarity[i][i] = (char)exact;
  }

  /* Record the calling parameters used to generate the similarity matrix. */

  simMatrix->exact = exact;
  simMatrix->similar = similar;
  simMatrix->dissimilar = dissimilar;
  simMatrix->gapStart = gapStart;
  simMatrix->gapExtend = gapExtend;
  simMatrix->matchLimit = matchLimit;

  return (simMatrix);
}

/* Function freeSimMatrix() - free the similarity matrix and return NULL. */

SIMMATRIX_T *freeSimMatrix(SIMMATRIX_T *M) {

  if (M) {
    free(M);
  }
  return (NULL);
}
