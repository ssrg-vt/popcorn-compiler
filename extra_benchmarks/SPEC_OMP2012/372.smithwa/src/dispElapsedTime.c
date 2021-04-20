/*
 * dispElapsedTime.c
 *
 * Display the elapsed time.
 *
 * Russ Brown
 */

/* @(#)dispElapsedTime.c	1.12 11/04/29 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sequenceAlignment.h"

/*
 * Here is the timer function. Resynchronize OpenMP threads
 * or MPI processes prior to measuring the time in order to
 * measure the slowest thread or process.  Use either the
 * gethrtime function or the clock function, depending upon
 * whether HI_RES_TIME is defined.
 */

double getSeconds( void )
{
#if !defined(SPEC)

#ifdef HR_TIME
  struct timespec hiResTime;
#endif

#pragma omp barrier
#ifdef MPI
  if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS) {
    printf("getSeconds: failure of call to MPI_Barrier\n");
    exit (1);
  }
#endif

#ifdef HR_TIME
  clock_gettime(CLOCK_REALTIME, &hiResTime);
  return ( (double)(hiResTime.tv_sec) + 1.0e-9*(double)(hiResTime.tv_nsec) );
#else
  return ( ( (double)clock()/CLOCKS_PER_SEC ) );
#endif
#else
  return ( ( (double) 0.0) );
#endif
}

/* Get the hi-res time and convert to hours, minutes and seconds. */

void dispElapsedTime(double startTime) {

#if !defined(SPEC)
#ifdef HR_TIME
  struct timespec hiResTime;
#endif

  double endTime, elapsedTime, seconds;
  int hours, minutes, myTaskID;

#ifdef HR_TIME
  clock_gettime(CLOCK_REALTIME, &hiResTime);
  endTime = (double)(hiResTime.tv_sec) + 1.0e-9*(double)(hiResTime.tv_nsec);
#else
  endTime = getSeconds();
#endif

#ifdef MPI
  if ( MPI_Comm_rank(MPI_COMM_WORLD, &myTaskID) != MPI_SUCCESS ) {
    printf("dispElapsedTime: cannot get myTaskID\n");
    MPI_Finalize();
    exit (1);
  }
#else
   myTaskID = 0;
#endif

  elapsedTime = endTime - startTime;

  hours = (int)(elapsedTime / 3600.0);
  minutes = (int)( (elapsedTime - 3600.0 * (double)hours) / 60.0 );
  seconds = elapsedTime - 3600.0 * (double)hours - 60.0 * (double)minutes;
  if (myTaskID == 0) {
    printf("\n\tElapsed time = %10.2f sec = %3d hour, %2d min, %5.2f sec\n",
	   elapsedTime, hours, minutes, seconds);
  }
#endif /* end of not defined(SPEC) */
}
