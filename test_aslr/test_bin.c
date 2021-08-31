#include <sys/types.h>
#include <stdio.h>
#include "migrate.h"

int
main (int argc, char *argv[])
{
  int i;

  printf ("origin: tid = %d\n", gettid());
  migrate(1, NULL, NULL);

  printf ("remote: tid = %d\n", gettid());
  
  migrate(0, NULL, NULL);
  pause();
  return 0;
}
