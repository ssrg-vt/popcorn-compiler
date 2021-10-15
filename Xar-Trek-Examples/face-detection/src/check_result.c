/*===============================================================*/
/*                                                               */
/*                       check_result.cpp                        */
/*                                                               */
/*      Software evaluation of training and test error rate      */
/*                                                               */
/*===============================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image.h"
#include "typedefs.h"

void check_results(int result_size, 
                   int result_x[RESULT_SIZE], 
                   int result_y[RESULT_SIZE], 
                   int result_w[RESULT_SIZE],  
                   int result_h[RESULT_SIZE],
                   unsigned char Data[IMAGE_HEIGHT][IMAGE_WIDTH],
                   char* outFile)
{
  FILE *fp;
  if ((fp=fopen("outputs.txt","w"))!=NULL)
  {
    fprintf(fp,"\nresult_size = %d\n", result_size);

    MyRect result[RESULT_SIZE];

    for (int j = 0; j < RESULT_SIZE; j++){
      result[j].x = result_x[j];
      result[j].y = result_y[j];
      result[j].width = result_w[j];
      result[j].height = result_h[j];
    }

    for( int i=0 ; i < result_size ; i++ )
    {
      fprintf(fp,"\n [Test Bench (main) ] detected rects: %d %d %d %d\n", result[i].x, result[i].y, result[i].width, result[i].height);
    }
    fclose(fp);
   
    printf("\n-- saving output image [Start] --\r\n"); 

    // Draw the rectangles onto the images and save the outputs.
    for(int i = 0; i < result_size ; i++ )
    {
      MyRect r = result[i];
      drawRectangle(Data, r);
    }

    int flag = writePgm(outFile, Data); 

    printf("\n-- saving output image [Done] --\r\n");
  }
  else
  {
    printf("Failed to create output file!\n");
  }

}
