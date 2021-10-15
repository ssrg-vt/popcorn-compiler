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



// KNL HOST 0 - BEGIN - Include APIs and Migration Flag
#include "KNL_Function.h"
#include "KNL_Load_Exec.c"
extern int per_app_migration_flag;
// KNL HOST 0 - END - Include APIs and Migration Flag


int main(int argc, char ** argv)
{
// timers ON
  struct timeval start, end;
  gettimeofday(&start, 0);


// KNL HOST 1 - BEGIN - Start Scheduler Client + INIT Kernel 
popcorn_client(0);
while (per_app_migration_flag == -1);
if (per_app_migration_flag == 2) KNL_HW_INIT (context, commands, program, fpga_kernel);
// KNL HOST 1 - END - Start Scheduler Client + INIT Kernel + 


  char outFile[] = "op";
  printf("Face Detection Application\n");


  // for this benchmark, input data is included in array Data
  // these are outputs
  int result_x[RESULT_SIZE];
  int result_y[RESULT_SIZE];
  int result_w[RESULT_SIZE];
  int result_h[RESULT_SIZE];
  int res_size = 0;



// KNL HOST 2 - BEGIN - Popcorn Migration Options
#ifdef PRINT_INFO
printf("DBG---> Migration Flag: %d", per_app_migration_flag);
 #endif

if (per_app_migration_flag == 0) { 

  	face_detect_sw(Data, result_x, result_y, result_w, result_h, &res_size);

} else if (per_app_migration_flag == 1) {
migrate (1,0,0);
  	face_detect_sw(Data, result_x, result_y, result_w, result_h, &res_size);
migrate (0,0,0);
} else if (per_app_migration_flag == 2) {
KNL_HW_face_detect_sw (context, commands, program, fpga_kernel, Data, result_x, result_y, result_w, result_h, &res_size); 
}
// KNL HOST 2 - END - Popcorn Migration Options

  // check results
  printf("Checking results:\n");
  check_results(res_size, result_x, result_y, result_w, result_h, Data, outFile);

// timers OFF
  gettimeofday(&end, 0);
  long long elapsed = (end.tv_sec - start.tv_sec) * 1000000LL + end.tv_usec - start.tv_usec;
  printf("FaceDet320 \t\telapsed time: %lld us\n", elapsed);

// KNL HOST 3 - BEGIN - Stop Scheduler Client
popcorn_client(1);
// KNL HOST 3 - END - Stop Scheduler Client



  return EXIT_SUCCESS;

}




