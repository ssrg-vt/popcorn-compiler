#include <stdio.h>

#include <stack_transform.h>

int main(int argc, char** argv)
{
  void* sp;
  void* fp;
  void* pc;
  char* str;
  st_handle st;

  printf("Reading registers...");
#ifdef __aarch64__
  GET_SP_MEM(sp);
  GET_X29_MEM(fp);
  GET_PC_MEM(pc);
#elif defined(__x86_64__)
  GET_RSP(sp);
  GET_RBP(fp);
  GET_RIP(pc);
#endif
  printf("SP=%p, FP=%p, PC=%p\n", sp, fp, pc);

  printf("Initializing stack transformation handle (%s)...\n", argv[0]);
  if((st = st_init(argv[0])))
  {
    str = st_get_cu_name(st, pc);
    printf("Compilation unit for pc=%p: %s\n", pc, str);
    st_free_str(st, str);

    str = st_get_func_name(st, pc);
    printf("Function for pc=%p: %s\n", pc, str);
    st_free_str(st, str);

    printf("\nPrinting information about the function:\n");
    st_print_func_info(st, pc);

    printf("\nPrinting detailed information about location descriptions:\n");
    st_print_func_loc_desc(st, main);

    st_destroy(st);
  }
  else
    printf("Couldn't open this file's ELF/DWARF info!\n");

  return 0;
}

