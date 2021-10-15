/*===============================================================*/
/*                                                               */
/*                       face_detect.cpp                         */
/*                                                               */
/*     Main host function for the Face Detection application.    */
/*                                                               */
/*===============================================================*/

// standard C/C++ headers
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// other headers
#include "typedefs.h"
#include "check_result.h"

// data
#include "image0_320_240.h"



int main(int argc, char ** argv)
{
// timers ON
  struct timeval start, end;
  gettimeofday(&start, 0);



  char outFile[] = "op";
  printf("Face Detection Application\n");


  // for this benchmark, input data is included in array Data
  // these are outputs
  int result_x[RESULT_SIZE];
  int result_y[RESULT_SIZE];
  int result_w[RESULT_SIZE];
  int result_h[RESULT_SIZE];
  int res_size = 0;



  	face_detect_sw(Data, result_x, result_y, result_w, result_h, &res_size);

  // check results
  printf("Checking results:\n");
  check_results(res_size, result_x, result_y, result_w, result_h, Data, outFile);

// timers OFF
  gettimeofday(&end, 0);
  long long elapsed = (end.tv_sec - start.tv_sec) * 1000000LL + end.tv_usec - start.tv_usec;
  printf("FaceDet320 \t\telapsed time: %lld us\n", elapsed);


  return EXIT_SUCCESS;

}




