/* Wrapper function of __libc_start_main.  */

#include <stdlib.h>

int __libc_start_main_popcorn (int (*main) (int, char **, char **),
			       int argc, char **argv, char **environ)
{
  exit (main (argc, argv, environ));

  return 0;
}
