#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

int main (void)
{
  int i;
  struct rlimit limit;
  struct rusage usage;

  getrlimit (RLIMIT_STACK, &limit);
  printf ("\nStack Limit = %ld and %ld max\n", limit.rlim_cur, limit.rlim_max);

  getrusage(RUSAGE_SELF, &usage); 
  printf("\nUnshared Stack Size = %ld\n", usage.ru_isrss);

// the following are set at runtime  
//  printf("GC %p\n", GC_get_stack_base());
extern void * _dl_phdr;
printf ( "segment list %p\n", _dl_phdr);

extern void * __libc_stack_end;
printf("%p %p %lx\n", __libc_stack_end, &i, (long) __libc_stack_end - (long) &i);

  return 0;
}
