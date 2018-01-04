#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <stack_transform.h>
#include <sys/prctl.h>
#include "migrate.h"
#include "config.h"
#include "arch.h"

#if _SIG_MIGRATION == 1
#include "trigger.h"
#endif

#if _TIME_REWRITE
#include "timer.h"
#endif

/* Thread migration status information. */
struct popcorn_thread_status {
	int current_nid;
	int proposed_nid;
	int peer_nid;
	int peer_pid;
} status;

#if _ENV_SELECT_MIGRATE == 1

/*
 * The user can specify at which point a thread should migrate by specifying
 * program counter address ranges via environment variables.
 */

/* Environment variables specifying at which function to migrate */
static const char *env_start_aarch64 = "AARCH64_MIGRATE_START";
static const char *env_end_aarch64 = "AARCH64_MIGRATE_END";
static const char *env_start_powerpc64 = "POWERPC64_MIGRATE_START";
static const char *env_end_powerpc64 = "POWERPC64_MIGRATE_END";
static const char *env_start_x86_64 = "X86_64_MIGRATE_START";
static const char *env_end_x86_64 = "X86_64_MIGRATE_END";

/* Per-arch functions (specified via address range) at which to migrate */
static void *start_aarch64 = NULL;
static void *end_aarch64 = NULL;
static void *start_powerpc64 = NULL;
static void *end_powerpc64 = NULL;
static void *start_x86_64 = NULL;
static void *end_x86_64 = NULL;

/* TLS keys indicating if the thread has previously migrated */
static pthread_key_t num_migrated_aarch64 = 0;
static pthread_key_t num_migrated_powerpc64 = 0;
static pthread_key_t num_migrated_x86_64 = 0;

/* Read environment variables to setup migration points. */
static void __attribute__((constructor))
__init_migrate_testing(void)
{
  const char *start;
  const char *end;

#ifdef __aarch64__
  start = getenv(env_start_aarch64);
  end = getenv(env_end_aarch64);
  if(start && end)
  {
    start_aarch64 = (void *)strtoll(start, NULL, 16);
    end_aarch64 = (void *)strtoll(end, NULL, 16);
    if(start_aarch64 && end_aarch64)
      pthread_key_create(&num_migrated_aarch64, NULL);
  }
#elif defined(__powerpc64__)
  start = getenv(env_start_powerpc64);
  end = getenv(env_end_powerpc64);
  if(start && end)
  {
    start_powerpc64 = (void *)strtoll(start, NULL, 16);
    end_powerpc64 = (void *)strtoll(end, NULL, 16);
    if(start_powerpc64 && end_powerpc64)
      pthread_key_create(&num_migrated_powerpc64, NULL);
  }
#else
  start = getenv(env_start_x86_64);
  end = getenv(env_end_x86_64);
  if(start && end)
  {
    start_x86_64 = (void *)strtoll(start, NULL, 16);
    end_x86_64 = (void *)strtoll(end, NULL, 16);
    if(start_x86_64 && end_x86_64)
      pthread_key_create(&num_migrated_x86_64, NULL);
  }
#endif
}

/*
 * Check environment variables to see if this call site is the function at
 * which we should migrate.
 */
static inline int do_migrate(void *addr)
{
  int retval = -1;
#ifdef __aarch64__
  if(start_aarch64 && !pthread_getspecific(num_migrated_aarch64)) {
    if(start_aarch64 <= addr && addr < end_aarch64) {
      pthread_setspecific(num_migrated_aarch64, (void *)1);
      retval = 0;
    }
  }
#elif defined(__powerpc64__)
  if(start_powerpc64 && !pthread_getspecific(num_migrated_powerpc64)) {
    if(start_powerpc64 <= addr && addr < end_powerpc64) {
      pthread_setspecific(num_migrated_powerpc64, (void *)1);
      retval = 1;
    }
  }
#else
  if(start_x86_64 && !pthread_getspecific(num_migrated_x86_64)) {
    if(start_x86_64 <= addr && addr < end_x86_64) {
      pthread_setspecific(num_migrated_x86_64, (void *)1);
      retval = 2;
    }
  }
#endif
  return retval;
}

#else /* _ENV_SELECT_MIGRATE */

static inline int do_migrate(void __attribute__((unused)) *fn)
{
	struct popcorn_thread_status status;
	if (syscall(SYSCALL_GET_THREAD_STATUS, &status)) return -1;

	return status.proposed_nid;
}

#endif /* _ENV_SELECT_MIGRATE */

static enum arch archs[MAX_POPCORN_NODES] = { 0 };

static void __attribute__((constructor)) __init_nodes_info(void)
{
	int ret;
	int origin_nid = -1;
	struct node_info {
		unsigned int status;
		int arch;
		int distance;
	} ni[MAX_POPCORN_NODES];

	for (int i = 0; i < MAX_POPCORN_NODES; i++) {
		archs[i] = ARCH_UNKNOWN;
	}

	ret = syscall(SYSCALL_GET_NODE_INFO, &origin_nid, ni);
	if (ret) {
		fprintf(stderr, "Cannot retrieve Popcorn node information, %d\n", ret);
		return;
	}
	for (int i = 0; i < MAX_POPCORN_NODES; i++) {
		if (ni[i].status == 1) {
			archs[i] = ni[i].arch;
		}
	}
}

int current_nid(void)
{
	struct popcorn_thread_status status;
	if (syscall(SYSCALL_GET_THREAD_STATUS, &status)) return -1;

	return status.current_nid;
}

enum arch current_arch(void)
{
	int nid = current_nid();
	if (nid < 0) return ARCH_UNKNOWN;

	return archs[nid];
}


/* Data needed post-migration. */
struct shim_data {
  void (*callback)(void *);
  void *callback_data;
  void *regset;
};

#if _DEBUG == 1
/*
 * Flag indicating we should spin post-migration in order to wait until a
 * debugger can attach.
 */
static volatile int __hold = 1;
#endif

/* Generate a call site to get rewriting metadata for outermost frame. */
static void* __attribute__((noinline))
get_call_site() { return __builtin_return_address(0); };

/* Check & invoke migration if requested. */
// Note: a pointer to data necessary to bootstrap execution after migration is
// saved by the pthread library.
static void inline __migrate_shim_internal(int nid, void (*callback)(void *),
                                           void *callback_data)
{
  struct shim_data data;
  struct shim_data *data_ptr = *pthread_migrate_args();

  if(data_ptr) // Post-migration
  {
#if _DEBUG == 1
    // Hold until we can attach post-migration
    while(__hold);
#endif

    if(data_ptr->callback) data_ptr->callback(data_ptr->callback_data);
    *pthread_migrate_args() = NULL;

    // Hack: the kernel can't set floating-point registers, so we have to
    // manually copy them over in userspace
    SET_FP_REGS;
  }
  else // Invoke migration
  {
#if _SIG_MIGRATION == 1
    clear_migrate_flag();
#endif
    const enum arch dst_arch = archs[nid];

    GET_LOCAL_REGSET;
    union {
       struct regset_aarch64 aarch;
       struct regset_powerpc64 powerpc;
       struct regset_x86_64 x86;
    } regs_dst;

    unsigned long sp = 0, bp = 0;

    data.callback = callback;
    data.callback_data = callback_data;
    data.regset = &regs_dst;
    *pthread_migrate_args() = &data;

#if _TIME_REWRITE == 1
    unsigned long long start, end;
    TIMESTAMP(start);
#endif
    if(REWRITE_STACK)
    {
#if _TIME_REWRITE == 1
      TIMESTAMP(end);
      printf("Stack transformation time: %lluns\n", TIMESTAMP_DIFF(start, end));
#endif

      if(dst_arch == ARCH_X86_64) {
        regs_dst.x86.rip = __migrate_shim_internal;
        sp = (unsigned long)regs_dst.x86.rsp;
        bp = (unsigned long)regs_dst.x86.rbp;
      } else if (dst_arch == ARCH_AARCH64) {
        regs_dst.aarch.pc = __migrate_shim_internal;
        sp = (unsigned long)regs_dst.aarch.sp;
        bp = (unsigned long)regs_dst.aarch.x[29];
      } else if (dst_arch == ARCH_POWERPC64) {
        regs_dst.powerpc.pc = __migrate_shim_internal;
        sp = (unsigned long)regs_dst.powerpc.r[1];
        bp = (unsigned long)regs_dst.powerpc.r[31];
      } else {
        assert(0 && "Unsupported architecture!");
      }

      MIGRATE;
      assert(0 && "Couldn't migrate!");
    }
  }
}

/* Check if we should migrate, and invoke migration. */
void check_migrate(void (*callback)(void *), void *callback_data)
{
  int nid = do_migrate(__builtin_return_address(0));
  if (nid >= 0 && nid != current_nid())
    __migrate_shim_internal(nid, callback, callback_data);
}

/* Externally-visible function to invoke migration. */
void migrate(int nid, void (*callback)(void *), void *callback_data)
{
  if (nid != current_nid())
    __migrate_shim_internal(nid, callback, callback_data);
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
void __cyg_profile_func_enter(void *this_fn, void __attribute__((unused)) *call_site)
{
  int nid = do_migrate(this_fn);
  if (nid >= 0 && nid != current_nid())
    __migrate_shim_internal(nid, migrate_callback, migrate_callback_data);
}

/* Hook inserted by compiler at the end of a function. */
void __attribute__((alias("__cyg_profile_func_enter")))
__cyg_profile_func_exit(void *this_fn, void *call_site);

