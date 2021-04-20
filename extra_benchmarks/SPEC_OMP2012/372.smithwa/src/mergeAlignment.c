/*
 * mergeAlignment.c
 *
 * Merge the sequences that were found by separate threads or processes
 * in scanBackward.
 *
 * Russ Brown
 */

/* @(#)mergeAlignment.c	1.2 06/08/04 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sequenceAlignment.h"

/*
 * Function mergeAlignment - merge scores found by separate threads
 * or processes in scanBackward.
 *
 * Merge the bestScores, bestStarts, bestEnds and bestSeqs arrays
 * by recursive binary combination, resulting in log2(n) performance.
 *
 * INPUT
 * B                - [BSTR_T*] results from scanBackwards
 *   numThreads     - [integer] number of OpenMP threads
 *   numReports     - [numThreads][integer] number of reports per thread
 *   bestStartsI    - [numThreads][reports][integer] main startpoints
 *   bestStartsJ    - [numThreads][reports][integer] match startpoints
 *   bestEndsI      - [numThreads][reports][integer] main endpoints
 *   bestEndsJ      - [numThreads][reports][integer] match endpoints
 *   bestSeqsI      - [numThreads][reports][unsigned char] main sequences
 *   bestSeqsJ      - [numThreads][reports][unsigned char] match sequences
 *   bestScores     - [numThreads][reports][long long] scores for reports
 * maxReports       - [integer] maximum number of start and end points reported
 * minSeparation    - [integer] minimum start or end point separation in codons
 *
 * OUTPUT
 * C                - [CSTR_T*] results from mergeAlignment
 *   numReports     - [integer] number of reports
 *   finalStartsI   - [reports][integer] main startpoints
 *   finalStartsJ   - [reports][integer] match startpoints
 *   finalEndsI     - [reports][integer] main endpoints
 *   finalEndsJ     - [reports][integer] match endpoints
 *   finalSeqsI     - [reports][unsigned char] main sequences
 *   finalSeqsJ     - [reports][unsigned char] match sequences
 *   finalScores    - [reports][long long] scores for reports
 */

CSTR_T *mergeAlignment(BSTR_T *B, int maxReports, int minSeparation) {

  long long *tempScores=NULL, *scores;
  int numReports, newReports, threadNum, numThreads, maxThreads;
  int *tempStartsI=NULL, *tempStartsJ=NULL;
  int *tempEndsI=NULL, *tempEndsJ=NULL;
  unsigned char **tempSeqsI=NULL, **tempSeqsJ=NULL, **sequences, *ptr;
  CSTR_T *C=NULL;
  int i, j, iter, mask, consumer, producer, totalReports;
  int flag, length, count, *index, indexB;
  int myRow, myCol, npRow, npCol;
  double beginTime, endTime;

#if defined(SPEC_OMP) && defined(SPEC_OMP_FLUSH)
  int *reqack;
#endif

#ifdef MPI
  MPI_Status status;
#else
  BSTR_T *P;
#endif

  /* Get the maximum number of OpenMP threads from the B structure. */

  maxThreads = B->numThreads;

  /*
   * Allocate a B structure to use to communicate results
   * from one OpenMP thread to another.  Allocate only the first
   * level arrays because the second-level arrays will be allocated
   * from within the parallel region.
   *
   * Note that MPI cannot use this type of structure to move data
   * between processes but instead must use MPI_Send and MPI_Recv.
   */

#ifndef MPI
  if ( (P = (BSTR_T*)malloc(sizeof(BSTR_T))) == NULL ) {
    printf("mergeAlignment: cannot allocate P\n");
    exit (1);
  }

  if ( (P->numReports = (int*)malloc(maxThreads*sizeof(int))) == NULL ) {
    printf("mergeAlignment: cannot allocate P->numReports\n");
    exit (1);
  }

  if ( (P->bestScores = (long long**)malloc(maxThreads*sizeof(long long*)))
       == NULL ) {
    printf("mergeAlignment: cannot allocate P->bestScores\n");
    exit (1);
  }

  if ( (P->bestStartsI = (int**)malloc(maxThreads*sizeof(int*))) == NULL ) {
    printf("mergeAlignment: cannot allocate P->bestStartsI\n");
    exit (1);
  }

  if ( (P->bestStartsJ = (int**)malloc(maxThreads*sizeof(int*))) == NULL ) {
    printf("mergeAlignment: cannot allocate P->bestStartsJ\n");
    exit (1);
  }

  if ( (P->bestEndsI = (int**)malloc(maxThreads*sizeof(int*))) == NULL ) {
    printf("mergeAlignment: cannot allocate P->bestEndsI\n");
    exit (1);
  }

  if ( (P->bestEndsJ = (int**)malloc(maxThreads*sizeof(int*))) == NULL ) {
    printf("mergeAlignment: cannot allocate P->bestEndsJ\n");
    exit (1);
  }

  if ( (P->bestSeqsI =
	(unsigned char***)malloc(maxThreads*sizeof(unsigned char**)))
       == NULL ) {
    printf("mergeAlignment: cannot allocate P->bestSeqsI\n");
    exit (1);
  }
    
  if ( (P->bestSeqsJ =
	(unsigned char***)malloc(maxThreads*sizeof(unsigned char**)))
       == NULL ) {
    printf("mergeAlignment: cannot allocate P->bestSeqsJ\n");
    exit (1);
  }
#endif

  /*
   * For OpenMP, if OMP_FLUSH is defined synchronize the threads via
   * '#pragma omp flush' instead of '#pragma omp barrier'.  This
   * approach is implemented via four cycle signaling that requires
   * a request and acknowledge array, which is initialized to zero.
   */

#if defined(SPEC_OMP) && defined(SPEC_OMP_FLUSH)
  if ( ( reqack = (int*) malloc( maxThreads * sizeof(int) ) ) == NULL ) {
    printf("mergeAlignment: can't allocate reqack\n");
    exit (1);
  }
  for (i = 0; i < maxThreads; i++) {
    reqack[i] = 0;
  }
#endif

  /* Everything else is done on a per-thread basis in this parallel region. */
  
#if defined(SPEC_OMP)
#pragma omp parallel \
  firstprivate(maxReports, minSeparation) \
  private(tempScores, tempStartsI, tempStartsJ, tempEndsI, tempEndsJ, \
          tempSeqsI, tempSeqsJ, sequences, threadNum, numThreads, numReports, \
          i, j, iter, mask, consumer, producer, totalReports, flag, indexB, \
          scores, index, myRow, myCol, npRow, npCol, beginTime, endTime, \
          length, count)
#endif
  {

    /* Map the OpenMP threads onto a square computing grid. */

    gridInfo(&npRow, &npCol, &myRow, &myCol);

    /* Get the OpenMP thread number or the MPI process number. */

#if defined(SPEC_OMP)
    threadNum = omp_get_thread_num();
#elif defined(MPI)
  if ( MPI_Comm_rank(MPI_COMM_WORLD, &threadNum) != MPI_SUCCESS ) {
    printf("mergeAlignment: cannot get myTaskID\n");
    MPI_Finalize();
    exit (1);
  }
#else
    threadNum = 0;
#endif

    /*
     * Allocate temp* arrays for all threads because data from the
     * B structure will be copied into these arrays.
     *
     * Initialize the tempSeqsI/J entries to NULL.
     */

    if ( (tempScores =
	  (long long*)malloc((2*maxReports+1)*sizeof(long long)))
	 == NULL ) {
      printf("mergeAlignment: cannot allocate tempScores for thread %d\n",
	     threadNum);
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }

    if ( (tempStartsI =
	  (int*)malloc((2*maxReports+1)*sizeof(int))) == NULL ) {
      printf("mergeAlignment: cannot allocate tempStartsI for thread %d\n",
	     threadNum);
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }

    if ( (tempStartsJ =
	  (int*)malloc((2*maxReports+1)*sizeof(int))) == NULL ) {
      printf("mergeAlignment: cannot allocate tempStartsJ for thread %d\n",
	     threadNum);
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }

    if ( (tempEndsI =
	  (int*)malloc((2*maxReports+1)*sizeof(int))) == NULL ) {
      printf("mergeAlignment: cannot allocate tempEndsI for thread %d\n",
	     threadNum);
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }

    if ( (tempEndsJ =
	  (int*)malloc((2*maxReports+1)*sizeof(int))) == NULL ) {
      printf("mergeAlignment: cannot allocate tempEndsJ for thread %d\n",
	     threadNum);
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }

    if ( (tempSeqsI =
	  (unsigned char**)malloc((2*maxReports+1)*sizeof(unsigned char*)))
	 == NULL ) {
      printf("mergeAlignment: cannot allocate tempSeqsI for thread %d\n",
	     threadNum);
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }

    if ( (tempSeqsJ =
	  (unsigned char**)malloc((2*maxReports+1)*sizeof(unsigned char*)))
	 == NULL ) {
      printf("mergeAlignment: cannot allocate tempSeqsJ for thread %d\n",
	     threadNum);
#ifdef MPI
      MPI_Finalize();
#endif
      exit (1);
    }

    for (i = 0; i <= 2*maxReports; i++) {
      tempSeqsI[i] = tempSeqsJ[i] = NULL;
    }

    /*
     * For OpenMP and single-threaded execution, point P
     * to the private temp* arrays so that data may be
     * copied between threads by using the P arrays.
     */

#ifndef MPI
    P->bestScores[threadNum] = tempScores;
    P->bestStartsI[threadNum] = tempStartsI;
    P->bestStartsJ[threadNum] = tempStartsJ;
    P->bestEndsI[threadNum] = tempEndsI;
    P->bestEndsJ[threadNum] = tempEndsJ;
    P->bestSeqsI[threadNum] = tempSeqsI;
    P->bestSeqsJ[threadNum] = tempSeqsJ;
#endif

    /*
     * Allocate sequences, best and index arrays for the sorting
     * by even threads.
     */

    if ( (threadNum & 1) == 0 ) {
      if ( (sequences =
	    (unsigned char**)malloc((2*maxReports+1)*sizeof(unsigned char*)))
	   == NULL ) {
	printf("mergeAlignment: cannot allocate sequences for thread %d\n",
	       threadNum);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (scores = (long long*)malloc((2*maxReports+1)*sizeof(long long)))
	   == NULL ) {
	printf("mergeAlignment: cannot allocate scores for thread %d\n",
	       threadNum);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (index = (int*)malloc((2*maxReports+1)*sizeof(int))) == NULL ) {
	printf("mergeAlignment: cannot allocate index for thread %d\n",
	       threadNum);
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }
    } else {

      /*
       * For MPI, allocate the index array because it is
       * used to communicate string lengths between processes.
       */

#ifdef MPI
      if ( (index = (int*)malloc((2*maxReports+1)*sizeof(int))) == NULL ) {
	printf("mergeAlignment: cannot allocate index for process %d\n",
	       threadNum);
	MPI_Finalize();
	exit (1);
      }
#endif
    }

    /* Measure the time for the reduction operation #ifdef MERGE_TIME. */

#ifdef MERGE_TIME
    beginTime = getSeconds();
#endif

    /*
     * Declare no reports if the thread does not lie on the process grid.
     * This step is probably unnecessary because pairwiseAlign() and
     * scanBackward() already initialize numReports to 0 for any thread
     * that does not lie on the grid, and because myRow and myCol will
     * be used to avoid reduction steps for threads that do not lie
     * on the grid (below).
     *
     * MPI does not use the structure P at all but must access the
     * arrays of the B structure for thread 0 only.
     */

#ifndef MPI
    if (myRow < 0 || myCol < 0) {
      P->numReports[threadNum] = numReports = 0;
    } else {
      P->numReports[threadNum] = numReports = B->numReports[threadNum];
    }
#else
    numReports = B->numReports[0];
#endif

    /*
     * For OpenMP and for single-threaded execution,
     * fill the private temp* arrays from the B best*
     * arrays for the corresponding thread.
     *
     * For MPI, the best* arrays of B have been allocated
     * for index 0 only
     *
     * Copy the strings that are referenced by the B->bestSeqsI/J
     * arrays because many of these strings will be subsequently
     * deallocated.  Therefore, copies of these strings will leave
     * the B structure and its strings intact.
     */

#ifndef MPI
    indexB = threadNum;
#else
    indexB = 0;
#endif

    for (i = 1; i <= numReports; i++) {
      tempScores[i] = B->bestScores[indexB][i];
      tempStartsI[i] = B->bestStartsI[indexB][i];
      tempStartsJ[i] = B->bestStartsJ[indexB][i];
      tempEndsI[i] = B->bestEndsI[indexB][i];
      tempEndsJ[i] = B->bestEndsJ[indexB][i];

      length = strlen((char*)(B->bestSeqsI[indexB][i]));
      if (length < 1) {
	printf("mergeAlignment: strlen(B->bestSeqsI[%d] = %d for thread %d\n",
	       i, length, indexB);
	exit (1);
      }
      length++;
      if ( (tempSeqsI[i] =
	    (unsigned char*)malloc(length * sizeof(unsigned char)))
	   == NULL ) {
	printf("mergeAlignment: cannot allocate tempSeqsI[%d] for thread %d",
	       i, indexB);
	exit (1);
      }
      tempSeqsI[i] =
	(unsigned char*)strcpy((char*)(tempSeqsI[i]),
			       (char*)(B->bestSeqsI[indexB][i]));

      length = strlen((char*)(B->bestSeqsJ[indexB][i]));
      if (length < 1) {
	printf("mergeAlignment: strlen(B->bestSeqsJ[%d] = %d for thread %d\n",
	       i, length, indexB);
	exit (1);
      }
      length++;
      if ( (tempSeqsJ[i] =
	    (unsigned char*)malloc(length * sizeof(unsigned char)))
	   == NULL ) {
	printf("mergeAlignment: cannot allocate tempSeqsJ[%d] for thread %d",
	       i, indexB);
	exit (1);
      }
      tempSeqsJ[i] =
	(unsigned char*)strcpy((char*)(tempSeqsJ[i]),
			       (char*)(B->bestSeqsJ[indexB][i]));
    }

    /*
     * For OpenMP, if OMP_FLUSH is not defined, synchronize the
     * threads via '#pragma omp barrier'.  If OMP_FLUSH is defined,
     * four-cycle signaling will be used to synchronize the threads,
     * as will be seen below, but here we need a '#pragma omp flush'
     * so that the request and acknowledge flags are read correctly
     * by all of the threads.
     *
     * MPI accomplishes synchronization via the MPI_Send/MPI_Receive
     * pair.
     */

#if defined(SPEC_OMP)
#ifndef SPEC_OMP_FLUSH
#pragma omp barrier
#else
#pragma omp flush
#endif
#endif

    /*
     * Calculate the iterations for the log2 reduction on the grid.
     * Note that each OpenMP thread or MPI process determines 'consumer'
     * and 'producer' from its thread or process number.  This ability
     * is important so that the OpenMP 'consumer' thread knows where
     * to obtain 'producer' data, and so that an MPI process knows
     * whether it is a 'consumer' process or a 'producer' process, and
     * the process number for its companion 'producer' or 'consumer' process. 
     */

    iter = npRow*npCol - 1;
    mask = 1;
    while (iter > 0) {
      consumer = threadNum & (~mask);
      producer = consumer | ((mask + 1) >> 1);

      /*
       * 'Consumer' designates a thread to which to append arrays from
       * an 'producer' thread.  Perform reduction only when both consumer
       * and producer are less than npRow*npCol.
       *
       * For successive iterations of the loop mask will have the values
       * 1, 3, 7, 15../
       *
       * The for the example of maxThreads=14 (numThreads=0..13), the
       * following threads will be chosen by consumer and producer:
       *
       * (iteration 1, consumer) - 0, 2, 4, 6, 8, 10, 12
       * (iteration 1, producer) - 1, 3, 5, 7, 9, 11, 13
       *
       * (iteration 2, consumer) - 0, 4, 8,  12
       * (iteration 2, producer) - 2, 6, 10, 13
       *
       * (iteration 3, consumer) - 0, 8
       * (iteration 3, producer) - 4, 12
       *
       * (iteration 4, consumer) - 0
       * (iteration 4, producer) - 8
       *
       * As the example shows, the final result is found in thread 0.
       *
       * Note that the following if statement uses myRow and myCol to
       * determine whether to perform a reduction step.  These variables
       * may not be used to determine whether to execute the while loop
       * (above) because all threads must execute the while loop in order
       * that the #pragma omp barrier within the loop not hang due to
       * threads that are not executing the loop.  The test for
       * (threadNum == consumer) guarantees that this thread will accept
       * data from a donor thread (which has threadNum == producer).  The
       * test for (producer < npRow*npCol) guarantees that the donor thread
       * exists on the grid.
       *
       * Note also that the following if statement controls whether an
       * OpenMP thread or an MPI process receives data.  An analogous
       * if statement (below) controls whether an MPI process sends data.
       */

      if ( ( threadNum == consumer ) && ( producer < npRow*npCol ) &&
	   ( myRow >= 0 ) && ( myCol >= 0) ) {

	/*
	 * For OpenMP, if OMP_FLUSH is defined, four-cycle signaling
	 * is used to synchronize the threads.  The 'consumer' thread
	 * raises its request flag then waits for the 'producer' thread
	 * to raise its acknowledge flag.
	 *
	 * The request flag is set by assigning the value of 'iter' to
	 * reqack[consumer].  The acknowledge flag is set by assigning
	 * the value of iter to reqack[producer].  The value of iter
	 * is used instead of 1 in order that each iteration of the loop
	 * have a unique value for the request and acknowledge flags.
	 * This approach avoids a race condition across iterations of
	 * the loop for access to the request and acknowledge flags.
	 *
	 * If OMP_FLUSH is not defined then no test is necessary
	 * because '#pragma omp barrier' is used to resynchronize
	 * all threads following the reduction iteration.
	 */

#if defined(SPEC_OMP) && defined(SPEC_OMP_FLUSH)
	reqack[consumer] = iter;
#pragma omp flush
	do {
#pragma omp flush
	} while (reqack[producer] != iter);
#endif

	/* Append reports contiguously above the existing reports. */

	totalReports = numReports;

	/*
	 * The test for OpenMP is accomplished via '#ifndef MPI'
	 * so that single-threaded code that is compiled with
	 * neither OMP nor MPI defined will execute the OpenMP
	 * code (without synchronization).
	 */

#ifndef MPI

	for (i = 1; i <= P->numReports[producer]; i++) {

	  /*
	   * Check the start and end points of each 'producer' report
	   * to the start and end points of all 'consumer' reports.
	   * If no proximity of points is detected, append the report;
	   * otherwise, reject it as a close duplicate and deallocate
	   * the P->bestSeqsI/J arrays.  Note that when a report is
	   * appended it is moved to a new address in the temp* arrays,
	   * so the P->bestSeqsI/J pointers should be set to NULL at
	   * the source address.
	   */

	  flag = 0;
	  for (j = 1; j <= numReports; j++) {
	    if ( ( MAX( abs(tempStartsI[j] - P->bestStartsI[producer][i]),
			abs(tempStartsJ[j] - P->bestStartsJ[producer][i]) )
		   < minSeparation ) ||
		 ( MAX( abs(tempEndsI[j] - P->bestEndsI[producer][i]),
			abs(tempEndsJ[j] - P->bestEndsJ[producer][i]) )
		   < minSeparation ) ) {
	      flag = 1;
	      break;
	    }
	  }

	  if (flag == 0) {
	    totalReports++;
	    tempScores[totalReports] = P->bestScores[producer][i];
	    tempStartsI[totalReports] = P->bestStartsI[producer][i];
	    tempStartsJ[totalReports] = P->bestStartsJ[producer][i];
	    tempEndsI[totalReports] = P->bestEndsI[producer][i];
	    tempEndsJ[totalReports] = P->bestEndsJ[producer][i];
	    tempSeqsI[totalReports] = P->bestSeqsI[producer][i];
	    tempSeqsJ[totalReports] = P->bestSeqsJ[producer][i];
	    P->bestSeqsI[producer][i] = P->bestSeqsJ[producer][i] = NULL;
	  } else {
	    if (P->bestSeqsI[producer][i]) {
	      free(P->bestSeqsI[producer][i]);
	      P->bestSeqsI[producer][i] = NULL;
	    }
	    if (P->bestSeqsJ[producer][i]) {
	      free(P->bestSeqsJ[producer][i]);
	      P->bestSeqsJ[producer][i] = NULL;
	    }
	    printf("\tmergeAlignment() thread %d found redundant report %d from thread %d\n",
		   threadNum, i, producer);
	  }
	}
#else

	/*
	 * For MPI the appended reports will come from the 'producer' process.
	 * Here is the protocol for receiving data.
	 *
	 * Begin by receiving the number of reports (tag = 8*iter+0).
	 */

	if ( MPI_Recv( &newReports, 1, MPI_INT, producer,
		       8*iter+0, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	  printf("mergeAlignment: cannot recv newReports\n");
	  MPI_Finalize();
	  exit (1);
	}

	/* If there are no reports, then receive no more data. */

	if (newReports != 0) {

	  /*
	   * The reports to be appended may be received into the upper half
	   * of the temp* arrays because the current number of reports
	   * (numReports) cannot exceed maxReports (see below) which is
	   * half of the number of elements in these arrays (see above).
	   *
	   * Receive the tempScores (tag = 8*iter+1).
	   */

	  if ( MPI_Recv( &(tempScores[maxReports+1]),
			 newReports, MPI_LONG_LONG_INT, producer,
			 8*iter+1, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot recv tempScores\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Receive the tempStartsI (tag = 8*iter+2). */
	
	  if ( MPI_Recv( &(tempStartsI[maxReports+1]),
			 newReports, MPI_INT, producer,
			 8*iter+2, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot recv tempStartsI\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Receive the tempStartsJ (tag = 8*iter+3). */
	
	  if ( MPI_Recv( &(tempStartsJ[maxReports+1]),
			 newReports, MPI_INT, producer,
			 8*iter+3, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot recv tempStartsJ\n");
	    MPI_Finalize();
	    exit (1);
	  }


	  /* Receive the tempEndsI (tag = 8*iter+4). */
	
	  if ( MPI_Recv( &(tempEndsI[maxReports+1]),
			 newReports, MPI_INT, producer,
			 8*iter+4, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot recv tempEndsI\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Receive the tempEndsJ (tag = 8*iter+5). */
	
	  if ( MPI_Recv( &(tempEndsJ[maxReports+1]),
			 newReports, MPI_INT, producer,
			 8*iter+5, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot recv tempEndsJ\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /*
	   * Receive the string length for each of tempSeqsI/J (tag = 8*iter+6).
	   * Use the lower half of the index array for tempSeqsI.
	   * Use the upper half of the index array for tempSeqsJ.
	   * Receive all of index, including element 0 that contains
	   * the length of the catenated string array.
	   */
	
	  if ( MPI_Recv( index,
			 2 * maxReports + 1, MPI_INT, producer,
			 8*iter+6, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot recv string lengths for tempSeqsI/J\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Allocate an array to receive the catenated strings. */

	  if ( (ptr = (unsigned char*)malloc(index[0]*sizeof(unsigned char)))
	       == NULL ) {
	    printf("mergeAlignment: cannot allocate recv ptr for process %d\n",
		   threadNum);
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Receive the catenated string array (tag = 8*iter+7). */

	  if ( MPI_Recv( ptr,
			 index[0], MPI_UNSIGNED_CHAR, producer,
			 8*iter+7, MPI_COMM_WORLD, &status ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot recv string array for process%d\n",
		   threadNum);
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Allocate tempSeqsI/J strings from the catenated string array. */

	  count = 0;
	  for (i = 1; i <= newReports; i++) {
	    if (index[i] < 1) {
	      printf("mergeAlignment: recv strlen(tempSeqsI[%d]) = %d for iter %d and process %d\n",
		     i, index[i], iter, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    if ( (tempSeqsI[maxReports + i] =
		  (unsigned char*)malloc(index[i]*sizeof(unsigned char)))
		 == NULL ) {
	      printf("mergeAlignment: cannot allocate recv tempSeqsI[%d] for process %d\n",
		     i, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    tempSeqsI[maxReports + i] =
	      (unsigned char *)strcpy( (char *)tempSeqsI[maxReports + i],
				       (char *)&(ptr[count]) );
	    count += index[i];

	    if (index[i + maxReports] < 1) {
	      printf("mergeAlignment: recv strlen(tempSeqsJ[%d]) = %d for iter %d and process %d\n",
		     i, index[i + maxReports], iter, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    if ( (tempSeqsJ[maxReports + i] =
		  (unsigned char*)malloc(index[i]*sizeof(unsigned char)))
		 == NULL ) {
	      printf("mergeAlignment: cannot allocate recv tempSeqsJ[%d] for process %d\n",
		     i, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    tempSeqsJ[maxReports + i] =
	      (unsigned char *)strcpy( (char *)tempSeqsJ[maxReports + i],
				       (char *)&(ptr[count]) );
	    count += index[i + maxReports];
	  }

	  /* Deallocate the catenated string array. */

	  free(ptr);

	  /* Now append the reports from the 'producer' process. */

	  for (i = maxReports + 1; i <= maxReports + newReports; i++) {

	    /*
	     * Check the start and end points of each 'producer' report
	     * to the start and end points of all 'consumer' reports.
	     * If no proximity of points is detected, append the report.
	     * Note that if (i == totalReports) then the report is at
	     * the correct position in the temp* arrays and doesn't need
	     * to be copied to the totalReports address.  Note that when
	     * a report is appended it is moved within the temp* arrays,
	     * so the tempSeqsI/J pointers should be set to NULL at the
	     * source address.
	     *
	     * If proximity of points is detected, reject the report
	     * as a close duplicate, and deallocate the tempSeqsI/J arrays
	     * and set their pointers to NULL.
	     */

	    flag = 0;
	    for (j = 1; j <= numReports; j++) {
	      if ( ( MAX( abs(tempStartsI[j] - tempStartsI[i]),
			  abs(tempStartsJ[j] - tempStartsJ[i]) )
		     < minSeparation ) ||
		   ( MAX( abs(tempEndsI[j] - tempEndsI[i]),
			  abs(tempEndsJ[j] - tempEndsJ[i]) )
		     < minSeparation ) ) {
		flag = 1;
		break;
	      }
	    }

	    if (flag == 0) {
	      totalReports++;
	      if (i != totalReports) {
		tempScores[totalReports] = tempScores[i];
		tempStartsI[totalReports] = tempStartsI[i];
		tempStartsJ[totalReports] = tempStartsJ[i];
		tempEndsI[totalReports] = tempEndsI[i];
		tempEndsJ[totalReports] = tempEndsJ[i];
		tempSeqsI[totalReports] = tempSeqsI[i];
		tempSeqsJ[totalReports] = tempSeqsJ[i];
		tempSeqsI[i] = tempSeqsJ[i] = NULL;
	      }
	    } else {
	      if (tempSeqsI[i]) {
		free(tempSeqsI[i]);
		tempSeqsI[i] = NULL;
	      }
	      if (tempSeqsJ[i]) {
		free(tempSeqsJ[i]);
		tempSeqsJ[i] = NULL;
	      }
	      printf("\tmergeAlignment() process %d found redundant report %d from process %d\n",
		     threadNum, i, producer);
	    }
	  }
	}
#endif

	/*
	 * The append is completed, so sort the tempScores array into 
	 * the scores array in ascending order, and fill as well the
	 * index array such that scores[k] = tempScores[ index[k] ].
	 * Then by traversing the index array from highest to lowest address,
	 * it is possible to retrieve elements of the tempScores array
	 * in descending order, which is also possible merely by traversing 
	 * the scores array in descending order.  However, the reason
	 * for the use of the index array is that it is necessary to
	 * retrieve elements of the tempStartsI, tempStartsJ, tempEndsI,
	 * tempEndsJ, tempSeqsI and tempSeqsJ arrays in the same order
	 * in which elements are retrieved from the tempScores array.
	 *
	 * Take as many reports as possible, up to a maximum of maxReports,
	 * so if totalReports > maxReports just take maxReports.
	 * Take the reports from the highest elements of the arrays.
	 *
	 * It is unclear that the arrays may be reordered in place,
	 * so use the scores and sequences arrays as intermediaries.
	 *
	 * For single-threaded execution (which actually doesn't
	 * execute this reduction loop) and for OpenMP, update the
	 * number of reports for the consumer and producer threads.
	 */

	qSort_both(scores, index, tempScores, totalReports);
	numReports = MIN(maxReports, totalReports);

#ifndef MPI
	P->numReports[consumer] = numReports;
	P->numReports[producer] = 0;
#endif

	for (i = 1; i <= numReports; i++) {
	  scores[i] = tempScores[ index[totalReports - i + 1] ];
	}
	for (i = 1; i <= numReports; i++) {
	  tempScores[i] = scores[i];
	}

	for (i = 1; i <= numReports; i++) {
	  scores[i] = tempStartsI[ index[totalReports - i + 1] ];
	}
	for (i = 1; i <= numReports; i++) {
	  tempStartsI[i] = scores[i];
	}

	for (i = 1; i <= numReports; i++) {
	  scores[i] = tempStartsJ[ index[totalReports - i + 1] ];
	}
	for (i = 1; i <= numReports; i++) {
	  tempStartsJ[i] = scores[i];
	}

	for (i = 1; i <= numReports; i++) {
	  scores[i] = tempEndsI[ index[totalReports - i + 1] ];
	}
	for (i = 1; i <= numReports; i++) {
	  tempEndsI[i] = scores[i];
	}

	for (i = 1; i <= numReports; i++) {
	  scores[i] = tempEndsJ[ index[totalReports - i + 1] ];
	}
	for (i = 1; i <= numReports; i++) {
	  tempEndsJ[i] = scores[i];
	}


	/*
	 * Free the sequence strings that will not be
	 * included in the next iteration because they will be
	 * overwritten.
	 */

	for (i = 1; i <= totalReports - numReports; i++) {
	  if (tempSeqsI[ index[i] ]) {
	    free(tempSeqsI[ index[i] ]);
	    tempSeqsI[ index[i] ] = NULL;
	  }
	  if (tempSeqsJ[ index[i] ]) {
	    free(tempSeqsJ[ index[i] ]);
	    tempSeqsJ[ index[i] ] = NULL;
	  }
	}

	/*
	 * Move from high addresses to low addresses those sequence
	 * strings that will included in the next iteration.
	 */

	for (i = 1; i <= numReports; i++) {
	  sequences[i] = tempSeqsI[ index[totalReports - i + 1] ];
	}
	for (i = 1; i <= numReports; i++) {
	  tempSeqsI[i] = sequences[i];
	}

	for (i = 1; i <= numReports; i++) {
	  sequences[i] = tempSeqsJ[ index[totalReports - i + 1] ];
	}
	for (i = 1; i <= numReports; i++) {
	  tempSeqsJ[i] = sequences[i];
	}


	/*
	 * Set to NULL the pointers to the sequence strings that
	 * will not be included in the next iteration, i.e., those
	 * strings that are located at the high addresses.  These
	 * sequence strings have already been moved (above) to
	 * low addresses.
	 */

	for (i = numReports+1; i <= totalReports; i++) {
	  tempSeqsI[i] = tempSeqsJ[i] = NULL;
	}

	/*
	 * For OpenMP, if OMP_FLUSH is defined, lower the request
	 * flag to indicate that the data has been copied, then
	 * wait for the acknowledge flag to be lowered.
	 */

#if defined(SPEC_OMP) && defined(SPEC_OMP_FLUSH)
	reqack[consumer] = 0;
#pragma omp flush
	do {
#pragma omp flush
	} while (reqack[producer] == iter);
#endif

      }

      /*
       * Here is the if statement that controls whether an OpenMP thread
       * or an MPI process sends data.  Note that because consumer never
       * equals producer, a thread or process cannot both send and receive
       * data.  For MPI, the data is literally sent via the MPI_Send
       * function.  For OpenMP, the data is copied by the 'consumer' thread,
       * so all that the producer thread needs to do is synchronize
       * via four-cycle signaling.
       *
       * It is necessary not only to check that (threadNum == producer)
       * but also that the process is on the computing grid.
       */

      if ( ( threadNum == producer ) && ( myRow >= 0 ) && ( myCol >= 0) ) {

	/*
	 * For OpenMP, if OMP_FLUSH is defined, four-cycle signaling
	 * is used to synchronize the threads.  The 'producer' thread
	 * waits for the 'consumer' thread to raise its request flag,
	 * then raises its acknowledge flag, then waits for the 'consumer'
	 * thread to lower its request flag (indicating that the 'consumer'
	 * thread has read the data), then lowers its acknowledge flag.
	 *
	 * Nothing is done here for single-threaded operation, i.e.,
	 * when neither OMP nor MPI is defined.
	 */

#if defined(SPEC_OMP) && defined(SPEC_OMP_FLUSH)
	do {
#pragma omp flush
	} while (reqack[consumer] != iter);
	reqack[producer] = iter;
#pragma omp flush

	do {
#pragma omp flush
	} while (reqack[consumer] == iter);
	reqack[producer] = 0;
#pragma omp flush
#endif

#ifdef MPI

	/*
	 * For MPI the appended reports will go to the 'consumer' process.
	 * Here is the protocol for sending data.  Begin by sending the
	 * number of reports (tag = 8*iter+0).
	 */

	if ( MPI_Send( &numReports, 1, MPI_INT, consumer,
		       8*iter+0, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	  printf("mergeAlignment: cannot send newReports\n");
	  MPI_Finalize();
	  exit (1);
	}

	/* If there are no reports, then send no more data. */

	if (numReports != 0) {

	  /* Send the tempScores (tag = 8*iter+1). */

	  if ( MPI_Send( &(tempScores[1]),
			 numReports, MPI_LONG_LONG_INT, consumer,
			 8*iter+1, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot send tempScores\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Send the tempStartsI (tag = 8*iter+2). */
	
	  if ( MPI_Send( &(tempStartsI[1]),
			 numReports, MPI_INT, consumer,
			 8*iter+2, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot send tempStartsI\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Send the tempStartsJ (tag = 8*iter+3). */
	
	  if ( MPI_Send( &(tempStartsJ[1]),
			 numReports, MPI_INT, consumer,
			 8*iter+3, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot temp tempStartsJ\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Send the tempEndsI (tag = 8*iter+4). */
	
	  if ( MPI_Send( &(tempEndsI[1]),
			 numReports, MPI_INT, consumer,
			 8*iter+4, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot send tempEndsI\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Send the tempEndsJ (tag = 8*iter+5). */
	
	  if ( MPI_Send( &(tempEndsJ[1]),
			 numReports, MPI_INT, consumer,
			 8*iter+5, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot send tempEndsJ\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /*
	   * Send the string length for each of tempSeqsI/J (tag = 8*iter+6).
	   * Use the lower half of the index array for tempSeqsI,
	   * and the upper half of the index array for tempSeqsJ.
	   * Send all of index, because the length of the catenated
	   * string array will be sent in element 0.
	   *
	   * Load the string lengths for tempSeqsI[i] into index.
	   */

	  count = 0;
	  for (i = 1; i <= numReports; i++) {
	    if ( tempSeqsI[i] == NULL ) {
	      printf("mergeAlignment: tempSeqsI[%d] == NULL for process = %d\n",
		     i, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    if ( ( length = strlen( (char*)tempSeqsI[i] ) ) < 1 ) {
	      printf("mergeAlignment: send strlen(tempSeqsI[%d]) = %d for iter %d and process %d\n",
		     i, length, iter, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    index[i] = length + 1;
	    count += index[i];

	    if ( tempSeqsJ[i] == NULL ) {
	      printf("mergeAlignment: tempSeqsJ[%d] == NULL for process %d\n",
		     i, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    if ( ( length = strlen( (char*)tempSeqsJ[i] ) ) < 1 ) {
	      printf("mergeAlignment: send strlen(tempSeqsJ[%d]) = %d for iter %d and process %d\n",
		     i, length, iter, threadNum);
	      MPI_Finalize();
	      exit (1);
	    }
	    index[i + maxReports] = length + 1;
	    count += index[i + maxReports];
	  }
	  index[0] = count;

	  if ( MPI_Send( index,
			 2 * maxReports + 1, MPI_INT, consumer,
			 8*iter+6, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot send string lengths for tempSeqsI/J\n");
	    MPI_Finalize();
	    exit (1);
	  }

	  /*
	   * Allocate the catenated string array and fill it from the
	   * tempSeqsI/J arrays.  Deallocate the tempSeqsI/J arrays
	   * as they are used to fill the catenated string array.
	   */

	  if ( (ptr = (unsigned char*)malloc(index[0]*sizeof(unsigned char)))
	       == NULL ) {
	    printf("mergeScore: cannot allocate send ptr for process %d\n",
		   threadNum);
	    MPI_Finalize();
	    exit (1);
	  }
	  count = 0;
	  for (i = 1; i <= numReports; i++) {
	    strcpy( (char *)&(ptr[count]), (char*)tempSeqsI[i] );
	    free(tempSeqsI[i]);
	    tempSeqsI[i] = NULL;
	    count += index[i];
	    strcpy( (char *)&(ptr[count]), (char*)tempSeqsJ[i] );
	    free(tempSeqsJ[i]);
	    tempSeqsJ[i] = NULL;
	    count += index[i + maxReports];
	  }

	  /* Send the catenated string array (tag = 8*iter+7). */

	  if ( MPI_Send( ptr,
			 index[0], MPI_UNSIGNED_CHAR, consumer,
			 8*iter+7, MPI_COMM_WORLD ) != MPI_SUCCESS ) {
	    printf("mergeAlignment: cannot send string array for process%d\n",
		   threadNum);
	    MPI_Finalize();
	    exit (1);
	  }

	  /* Deallocate the catenated string array. */

	  free(ptr);
	}
#endif
      }

      /*
       * For OpenMP, if OMP_FLUSH is not defined resynchronize via
       * '#pragma omp barrier'.
       */

#if defined(SPEC_OMP)
#ifndef SPEC_OMP_FLUSH
#pragma omp barrier
#endif
#endif

      /* Select the set of threads to perform the next reduction. */

      mask = (mask << 1) + 1;
      iter >>= 1;
    }

    /* Print the reduction time. */

#ifdef MERGE_TIME
    endTime = getSeconds();
    if (threadNum == 0) {
      printf("\n        Merge time = %10.5f seconds\n",
	     endTime - beginTime);
    }
#endif

    /*
     * Now that the reduction has finished, copy the results from
     * the private temp* arrays of thread 0 into the shared C structure
     * which is allocated for thread 0 only.  Deallocate the tempSeqsI/J
     * arrays as they are copied into the C structure.
     */

    if (threadNum == 0) {

      if ( (C = (CSTR_T*)malloc(sizeof(CSTR_T))) == NULL ) {
	printf("mergeAlignment: cannot allocate C\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }
      C->numReports = 0;
      C->finalScores = NULL;
      C->finalStartsI = C->finalStartsJ = NULL;
      C->finalEndsI = C->finalEndsJ = NULL;
      C->finalSeqsI = C->finalSeqsJ = NULL;

      C->numReports = numReports;

      if ( (C->finalScores =
	    (long long*)malloc((numReports+1)*sizeof(long long))) == NULL ) {
	printf("mergeAlignment: cannot allocate C->finalScores\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (C->finalStartsI =
	    (int*)malloc((numReports+1)*sizeof(int))) == NULL) {
	printf("mergeAlignment: cannot allocate C->finalStartsI\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (C->finalStartsJ =
	    (int*)malloc((numReports+1)*sizeof(int))) == NULL) {
	printf("mergeAlignment: cannot allocate C->finalStartsJ\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (C->finalEndsI =
	    (int*)malloc((numReports+1)*sizeof(int))) == NULL) {
	printf("mergeAlignment: cannot allocate C->finalEndsI\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      if ( (C->finalEndsJ =
	    (int*)malloc((numReports+1)*sizeof(int))) == NULL) {
	printf("mergeAlignment: cannot allocate C->finalEndsJ\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

     if ( (C->finalSeqsI =
	   (unsigned char**)malloc((numReports+1)*sizeof(unsigned char*)))
	  == NULL ) {
       printf("mergeAlignment: cannot allocate C->finalSeqsI\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

     if ( (C->finalSeqsJ =
	   (unsigned char**)malloc((numReports+1)*sizeof(unsigned char*)))
	  == NULL ) {
       printf("mergeAlignment: cannot allocate C->finalSeqsJ\n");
#ifdef MPI
	MPI_Finalize();
#endif
	exit (1);
      }

      for (i = 1; i <= numReports; i++) {
	C->finalScores[i] = tempScores[i];
	C->finalStartsI[i] = tempStartsI[i];
	C->finalStartsJ[i] = tempStartsJ[i];
	C->finalEndsI[i] = tempEndsI[i];
	C->finalEndsJ[i] = tempEndsJ[i];
	if ( ( length = strlen( (char*)tempSeqsI[i] ) ) < 1 ) {
	  printf("mergeAlignment: strlen(tempSeqsI[%d]) = %d for process %d\n",
		 i, length, threadNum);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}
	length++;
	if ( (C->finalSeqsI[i] =
	      (unsigned char*)malloc(length * sizeof(unsigned char)))
	     == NULL ) {
	  printf("mergeAlignment: cannot allocate C->finalSeqsI[%d]\n", i);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}
	C->finalSeqsI[i] =
	  (unsigned char*)strcpy((char*)C->finalSeqsI[i], (char*)tempSeqsI[i]);
	free(tempSeqsI[i]);
	tempSeqsI[i] = NULL;

	if ( ( length = strlen( (char*)tempSeqsJ[i] ) ) < 1 ) {
	  printf("mergeAlignment: strlen(tempSeqsJ[%d]) = %d for process %d\n",
		 i, length, threadNum);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}
	length++;
	if ( (C->finalSeqsJ[i] =
	      (unsigned char*)malloc(length * sizeof(unsigned char)))
	     == NULL ) {
	  printf("mergeAlignment: cannot allocate C->finalSeqsJ[%d]\n", i);
#ifdef MPI
	  MPI_Finalize();
#endif
	  exit (1);
	}
	C->finalSeqsJ[i] =
	  (unsigned char*)strcpy((char*)C->finalSeqsJ[i], (char*)tempSeqsJ[i]);
	free(tempSeqsJ[i]);
	tempSeqsJ[i] = NULL;
      }
    }

    /* Free the private temp* arrays for all threads. */

    if (tempScores) {
      free(tempScores);
      tempScores = NULL;
    }
    if (tempStartsI) {
      free(tempStartsI);
      tempStartsI = NULL;
    }
    if (tempStartsJ) {
      free(tempStartsJ);
      tempStartsJ = NULL;
    }
    if (tempEndsI) {
      free(tempEndsI);
      tempEndsI = NULL;
    }
    if (tempEndsJ) {
      free(tempEndsJ);
      tempEndsJ = NULL;
    }

    /*
     * In principle, all of the tempSeqsI/J[i] arrays have
     * already been deallocated, but the following code is
     * executed as a precaution against memory leaks.
     */

    count = 0;
    if (tempSeqsI) {
      for (i = 0; i <= 2*maxReports; i++) {
	if (tempSeqsI[i]) {
	  free(tempSeqsI[i]);
	  tempSeqsI[i] = NULL;
	  count++;
	}
      }
      free(tempSeqsI);
      tempSeqsI = NULL;
    }
    if (count != 0) {
      printf("mergeAlignment: freed %d tempSeqsI strings for thread %d\n",
	     count, threadNum);
    }

    count = 0;
    if (tempSeqsJ) {
      for (i = 0; i <= 2*maxReports; i++) {
	if (tempSeqsJ[i]) {
	  free(tempSeqsJ[i]);
	  tempSeqsJ[i] = NULL;
	  count++;
	}
      }
      free(tempSeqsJ);
      tempSeqsJ = NULL;
    }
    if (count != 0) {
      printf("mergeAlignment: freed %d tempSeqsJ strings for thread %d\n",
	     count, threadNum);
    }

    /*
     * Free the sequences, scores and index arrays for even-thread
     * execution and for MPI the index array for odd-thread execution.
     */

    if ( (threadNum & 1) == 0 ) {
      free(sequences);
      free(scores);
      free(index);
    } else {
#ifdef MPI
      free(index);
#endif
    }
  }

  /* Free the semaphores. */

#if defined(SPEC_OMP) && defined(SPEC_OMP_FLUSH)
  free(reqack);
#endif

  /*
   * Free the structure P, but do so explicitly instead of using
   * the freeB function because P contains pointers to temp* arrays
   * which in principle have already been deallocated.
   */

#ifndef MPI
  free(P->numReports);
  free(P->bestScores);
  free(P->bestStartsI);
  free(P->bestStartsJ);
  free(P->bestEndsI);
  free(P->bestEndsJ);
  free(P->bestSeqsI);
  free(P->bestSeqsJ);
  free(P);
#endif

  return (C);
}

/* Function freeC - free the C structure. */

CSTR_T *freeC(CSTR_T *C) {

  int i;

  if (C) {
    if (C->finalScores) {
      free(C->finalScores);
      C->finalScores = NULL;
    }
    if (C->finalStartsI) {
      free(C->finalStartsI);
      C->finalStartsI = NULL;
    }
    if (C->finalStartsJ) {
      free(C->finalStartsJ);
      C->finalStartsJ = NULL;
    }
    if (C->finalEndsI) {
      free(C->finalEndsI);
      C->finalEndsI = NULL;
    }
    if (C->finalEndsJ) {
      free(C->finalEndsJ);
      C->finalEndsJ = NULL;
    }
    if (C->finalSeqsI) {
      for (i = 1; i <= C->numReports; i++) {
	if (C->finalSeqsI[i]) {
	  free(C->finalSeqsI[i]);
	  C->finalSeqsI[i] = NULL;
	}
      }
      free(C->finalSeqsI);
      C->finalSeqsI = NULL;
    }
    if (C->finalSeqsJ) {
      for (i = 1; i <= C->numReports; i++) {
	if (C->finalSeqsJ[i]) {
	  free(C->finalSeqsJ[i]);
	  C->finalSeqsJ[i] = NULL;
	}
      }
      free(C->finalSeqsJ);
      C->finalSeqsJ = NULL;
    }
    free(C);
  }
  return (NULL);
}
