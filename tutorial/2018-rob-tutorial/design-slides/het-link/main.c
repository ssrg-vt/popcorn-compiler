#include <stdlib.h>

extern void fizzbuzz(unsigned max);

int main(int argc, char** argv)
{
  unsigned max = 100;
  if(argc > 1) max = atoi(argv[1]);
  fizzbuzz(max);
  return 0;
}
