#include <stdio.h>

void fizzbuzz(unsigned max)
{
  unsigned i;
  for(i = 0; i < max; i++)
  {
    if((i % 5) == 0 && (i % 3) == 0)
      printf("fizzbuzz\n");
    else if((i % 5) == 0)
      printf("fizz\n");
    else if((i % 3) == 0)
      printf("buzz\n");
  }
}
