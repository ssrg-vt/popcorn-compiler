#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <stack_transform.h>
#include <sys/prctl.h>
#include "migrate.h"

/* Architecture-specific macros for migrating between architectures. */
#ifdef __aarch64__
# include <arch/aarch64/migrate.h>
#else
# include <arch/x86_64/migrate.h>
#endif

/* Returns a CPU set for architecture AR. */
cpu_set_t arch_to_cpus(enum arch ar)
{
  cpu_set_t cpus;
  CPU_ZERO(&cpus);
  switch(ar) {
  case AARCH64: CPU_SET(0, &cpus); break;
  case X86_64: CPU_SET(8, &cpus); break;
  default: break;
  }
  return cpus;
}

/* Returns a CPU for the current architecture. */
cpu_set_t current_arch()
{
#ifdef __aarch64__
  return arch_to_cpus(AARCH64);
#elif defined __x86_64__
  return arch_to_cpus(X86_64);
#endif
}

/* Returns a CPU set for an architecture that we want to migrate to. */
cpu_set_t select_arch()
{
#ifdef __aarch64__
  return arch_to_cpus(X86_64);
#elif defined __x86_64__
  return arch_to_cpus(AARCH64);
#endif
}

#ifdef _TEST_MIGRATE

/*
 * The user can specify at which point a thread should migrate by specifying
 * function address ranges via environment variables.
 */

/* Environment variables specifying at which function to migrate */
static const char *env_start_aarch64 = "AARCH64_MIGRATE_START";
static const char *env_end_aarch64 = "AARCH64_MIGRATE_END";
static const char *env_start_x86_64 = "X86_64_MIGRATE_START";
static const char *env_end_x86_64 = "X86_64_MIGRATE_END";

/* Per-arch functions (specified via address range) at which to migrate */
static void *start_aarch64 = NULL;
static void *end_aarch64 = NULL;
static void *start_x86_64 = NULL;
static void *end_x86_64 = NULL;

/* TLS keys indicating if the thread has previously migrated */
static pthread_key_t num_migrated_aarch64 = 0;
static pthread_key_t num_migrated_x86_64 = 0;

/* Read environment variables to setup migration points. */
static void __attribute__((constructor))
__init_migrate_testing(void)
{
  const char *start;
  const char *end;

  start = getenv(env_start_aarch64);
  end = getenv(env_end_aarch64);
  if(start && end)
  {
    start_aarch64 = (void *)strtoll(start, NULL, 16);
    end_aarch64 = (void *)strtoll(end, NULL, 16);
    if(start_aarch64 && end_aarch64)
      pthread_key_create(&num_migrated_aarch64, NULL);
  }

  start = getenv(env_start_x86_64);
  end = getenv(env_end_x86_64);
  if(start && end)
  {
    start_x86_64 = (void *)strtoll(start, NULL, 16);
    end_x86_64 = (void *)strtoll(end, NULL, 16);
    if(start_x86_64 && end_x86_64)
      pthread_key_create(&num_migrated_x86_64, NULL);
  }
}

/*
 * Check environment variables to see if this call site is the function at
 * which we should migrate.
 */
static inline int do_migrate(void *addr)
{
  int retval = 0;
# ifdef __aarch64__
  if(start_aarch64 && !pthread_getspecific(num_migrated_aarch64)) {
    if(start_aarch64 <= addr && addr < end_aarch64) {
      pthread_setspecific(num_migrated_aarch64, (void *)1);
      retval = 1;
    }
  }
# elif defined __x86_64__
  if(start_x86_64 && !pthread_getspecific(num_migrated_x86_64)) {
    if(start_x86_64 <= addr && addr < end_x86_64) {
      pthread_setspecific(num_migrated_x86_64, (void *)1);
      retval = 1;
    }
  }
# endif
  return retval;
}

#else

/* Popcorn vDSO prctl code & page pointer. */
# define POPCORN_VDSO_CODE 41
static volatile long *popcorn_vdso = NULL;

/* Initialize Popcorn vDSO page */
static void __attribute__((constructor))
__init_migrate_vdso(void)
{
  unsigned long addr;
  if(prctl(POPCORN_VDSO_CODE, &addr) >= 0)
    popcorn_vdso = (long *)addr;
}

/*
 * Read Popcorn vDSO page to see if we should migrate.
 *
 * 0 -> process should be on x86
 * 1 -> process should be on aarch64
 */
static inline int do_migrate(void *addr)
{
  int ret = 0;
  if(popcorn_vdso)
  {
# ifdef __aarch64__
    if(*popcorn_vdso == 0) ret = 1;
# else /* x86_64 */
    if(*popcorn_vdso == 1) ret = 1;
# endif
  }
  return ret;
}

#endif /* _TEST_MIGRATE */

/* Data needed post-migration. */
struct shim_data {
  void (*callback)(void *);
  void *callback_data;
};

/* Check & invoke migration if requested. */
// Note: arguments are saved to this stack frame, and a pointer to them is
// saved by the pthread library.  Arguments can then be accessed post-migration
// by reading this pointer.  This method for saving/restoring arguments is
// necessary because saving argument locations in the LLVM backend is tricky.
static void __migrate_shim_internal(void (*callback)(void *),
                                    void *callback_data,
                                    void *pc)
{
  struct shim_data data;
  struct shim_data *data_ptr = *pthread_migrate_args();
  if(data_ptr) // Post-migration
  {
    if(data_ptr->callback) data_ptr->callback(data_ptr->callback_data);
    *pthread_migrate_args() = NULL;
  }
  else // Check & do migration if requested
  { 
    if(do_migrate(pc))
    {
      struct regset_aarch64 regs_aarch64;
      struct regset_x86_64 regs_x86_64;
      cpu_set_t cpus;

      data.callback = callback;
      data.callback_data = callback_data;
      *pthread_migrate_args() = &data;
      cpus = select_arch();
      if(REWRITE_STACK)
        MIGRATE(0, sizeof(cpu_set_t), (void *)&cpus, (void *)migrate_shim);
    }
  }
}

/* Externally visible shim, funnel to internal function to do dirty work. */
void migrate_shim(void (*callback)(void *), void *callback_data)
{
  __migrate_shim_internal(callback,
                          callback_data,
                          __builtin_return_address(0));
}

/* Callback function & data for migration points inserted via compiler. */
static void (*migrate_callback)(void *) = NULL;
static void *migrate_callback_data = NULL;

/* Register callback function for compiler-inserted migration points. */
void register_migrate_callback(void (*callback)(void*), void *callback_data)
{
  migrate_callback = callback;
  migrate_callback_data = callback_data;
}

/* Hook inserted by compiler at the beginning of a function. */
void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
  __migrate_shim_internal(migrate_callback,
                          migrate_callback_data,
                          __builtin_return_address(0));
}

/* Hook inserted by compiler at the end of a function. */
void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
  __migrate_shim_internal(migrate_callback,
                          migrate_callback_data,
                          __builtin_return_address(0));
}

