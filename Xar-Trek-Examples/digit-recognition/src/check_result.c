/*===============================================================*/
/*                                                               */
/*                       check_result.cpp                        */
/*                                                               */
/*      Software evaluation of training and test error rate      */
/*                                                               */
/*===============================================================*/

#include <stdio.h>
#include "typedefs.h"
#include <stdlib.h>

void check_results(LabelType* result, const LabelType* expected, int cnt)
{
  int correct_cnt = 0;

  FILE *fp;
  if ((fp=fopen("outputs.txt","w"))!=NULL)
  {
correct_cnt = 0;
    for (int i = 0; i < cnt; i ++ )
    {
      if (result[i] == expected[i])
        correct_cnt ++;
    }

    fprintf(fp,"\n\t %d / %d correct!\n", correct_cnt, cnt);
correct_cnt = 0;
    for (int i = 0; i < cnt; i ++ )
    {
      if (result[i] != expected[i])
        fprintf(fp,"Test %d: expected = %d, result = %d\n", i, (int)expected[i], (int)result[i]);
      else
        correct_cnt ++;
    }

    fclose(fp);
  }
  else
  {
    printf("Failed to create output file!\n");
  }


}
