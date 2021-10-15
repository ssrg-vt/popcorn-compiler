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






// KNL HOST 0 - BEGIN - Include APIs and Migration Flag
#include "KNL_Function.h"
#include "KNL_Load_Exec.c"
extern int per_app_migration_flag;
// KNL HOST 0 - END - Include APIs and Migration Flag


int main(int argc, char ** argv)
{
  // timers
  struct timeval start, end;
  gettimeofday(&start, NULL);


// KNL HOST 1 - BEGIN - Start Scheduler Client + INIT Kernel 
popcorn_client(0);
while (per_app_migration_flag == -1);
if (per_app_migration_flag == 2) KNL_HW_INIT (context, commands, program, fpga_kernel);
// KNL HOST 1 - END - Start Scheduler Client + INIT Kernel + 




  printf("Digit Recognition Application\n");

  // for this benchmark, data is already included in arrays:
  //   training_data: contains 18000 training samples, with 1800 samples for each digit class
  //   testing_data:  contains 2000 test samples
  //   expected:      contains labels for the test samples


    // create space for the result
    LabelType* result;
    result = malloc(NUM_TEST * sizeof(LabelType));



// KNL HOST 2 - BEGIN - Popcorn Migration Options
#ifdef PRINT_INFO
printf("DBG---> Migration Flag: %d", per_app_migration_flag);
 #endif

if (per_app_migration_flag == 0) { 

		DigitRec_sw(training_data, testing_data, result);

} else if (per_app_migration_flag == 1) {
migrate (1,0,0);
		DigitRec_sw(training_data, testing_data, result);
migrate (0,0,0);
} else if (per_app_migration_flag == 2) {
KNL_HW_DigitRec_sw (context, commands, program, fpga_kernel, training_data, testing_data, result); 
}
// KNL HOST 2 - END - Popcorn Migration Options


  // check results
  printf("Checking results:\n");
  check_results( result, expected, NUM_TEST );


  // cleanup
  free(result);



  gettimeofday(&end, NULL);
  // print time
  long long elapsed = (end.tv_sec - start.tv_sec) * 1000000LL + end.tv_usec - start.tv_usec;
  printf("DigRec2000 \t\telapsed time: %lld us\n", elapsed);


// KNL HOST 3 - BEGIN - Stop Scheduler Client
popcorn_client(1);
// KNL HOST 3 - END - Stop Scheduler Client



 return EXIT_SUCCESS;

}
