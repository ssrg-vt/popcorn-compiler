/*===============================================================*/
/*                                                               */
/*                    digit_recognition.cpp                      */
/*                                                               */
/*   Main host function for the Digit Recognition application.   */
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
#include "training_data.h"
#include "testing_data.h"






int main(int argc, char ** argv)
{
  // timers
  struct timeval start, end;
  gettimeofday(&start, NULL);





  printf("Digit Recognition Application\n");

  // for this benchmark, data is already included in arrays:
  //   training_data: contains 18000 training samples, with 1800 samples for each digit class
  //   testing_data:  contains 2000 test samples
  //   expected:      contains labels for the test samples


    // create space for the result
    LabelType* result;
    result = malloc(NUM_TEST * sizeof(LabelType));



		DigitRec_sw(training_data, testing_data, result);


  // check results
  printf("Checking results:\n");
  check_results( result, expected, NUM_TEST );


  // cleanup
  free(result);



  gettimeofday(&end, NULL);
  // print time
  long long elapsed = (end.tv_sec - start.tv_sec) * 1000000LL + end.tv_usec - start.tv_usec;
  printf("DigRec2000 \t\telapsed time: %lld us\n", elapsed);



 return EXIT_SUCCESS;

}
