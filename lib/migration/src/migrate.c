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

#ifdef _TIME_REWRITE
#include <time.h>
#endif

/* Architecture-specific assembly for migrating between architectures. */
#ifdef __aarch64__
# include <arch/aarch64/migrate.h>
#else
# include <arch/x86_64/migrate.h>
#endif

/* Pierre: due to the-fdata-sections flags, in combination with the way the
 * library is compiled for each architecture, global variables here end up
 * placed into sections with different names, making them difficult to link
 * back together from the alignment tool  perspective without ugly hacks.
 * So, the solution here is to force these global variables to be in a custom
 * section. By construction it will have the same name on both architecture.
 * However for some reason this doesn't work if the global variable is static so
 * I had to remove the static keyword for the concerned variables. They are:
 * - migrate_callback
 * - migrate_callback_data
 * - archs
 */

struct popcorn_thread_status {
	int current_nid;
	int proposed_nid;
	int peer_nid;
	int peer_pid;
} status;

#ifdef _ENV_SELECT_MIGRATE

/*
 * The user can specify at which point a thread should migrate by specifying
 * program counter address ranges via environment variables.
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
  int retval = -1;
# ifdef __aarch64__
  if(start_aarch64 && !pthread_getspecific(num_migrated_aarch64)) {
    if(start_aarch64 <= addr && addr < end_aarch64) {
      pthread_setspecific(num_migrated_aarch64, (void *)1);
      retval = 0;
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

#else /* _ENV_SELECT_MIGRATE */

char __thread __migrate_to_node=-1;
static inline int do_migrate(void *fn)
{
#if 0
	struct popcorn_thread_status status;
	if (syscall(SYSCALL_GET_THREAD_STATUS, &status)) return -1;

	return status.proposed_nid;
#endif
	/* TODO: use POSIX signal to set the variable */
	return __migrate_to_node;
}

#endif /* _ENV_SELECT_MIGRATE */

int origin_nid = -1;

int archs[MAX_POPCORN_NODES] __attribute__ ((section (".data.archs"))) = { 0 };

static void __attribute__((constructor)) __init_nodes_info(void)
{
	int ret;
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

#ifdef _DEBUG
/*
 * Flag indicating we should spin post-migration in order to wait until a
 * debugger can attach.
 */
static volatile int __hold = 1;
#endif

/* Check & invoke migration if requested. */
// Note: a pointer to data necessary to bootstrap execution after migration is
// saved by the pthread library.
static void inline __migrate_shim_internal(int nid,
    void (*callback)(void *), void *callback_data)
{
  struct shim_data data;
  struct shim_data *data_ptr = *pthread_migrate_args();

  if(data_ptr) // Post-migration
  {
#ifdef _DEBUG
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
    // TODO support flexible node configuration
    const int dst_arch = archs[nid];

    GET_LOCAL_REGSET;
	union {
		struct regset_aarch64 aarch;
		struct regset_x86_64 x86;
	} regs_dst;
	unsigned long sp = 0, bp = 0;

#ifdef _TIME_REWRITE
    struct timespec start, end;
    unsigned long start_ns, end_ns;

    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

	if (REWRITE_STACK) {
		fprintf(stderr, "Could not rewrite stack!\n");
		return;
	}

#ifdef _TIME_REWRITE
	clock_gettime(CLOCK_MONOTONIC, &end);
	start_ns = start.tv_sec * 1000000000 + start.tv_nsec;
	end_ns = end.tv_sec * 1000000000 + end.tv_nsec;
	printf("Stack transformation time: %ldns\n", end_ns - start_ns);
#endif

    data.callback = callback;
    data.callback_data = callback_data;
	data.regset = &regs_dst;
    *pthread_migrate_args() = &data;

	if (dst_arch == ARCH_X86_64) {
		regs_dst.x86.rip = __migrate_shim_internal;
		sp = (unsigned long)regs_dst.x86.rsp;
		bp = (unsigned long)regs_dst.x86.rbp;
	} else if (dst_arch == ARCH_AARCH64) {
		regs_dst.aarch.pc = __migrate_shim_internal;
		sp = (unsigned long)regs_dst.aarch.sp;
		bp = (unsigned long)regs_dst.aarch.x[29];
	} else {
		assert(0 && "Unsupported architecture!");
	}

	MIGRATE;
	assert(0 && "Couldn't migrate!");
  }
}

/* Check if we should migrate, and invoke migration. */
void check_migrate(void (*callback)(void *), void *callback_data)
{
  int nid = do_migrate(__builtin_return_address(0));
  if (nid >= 0)
    __migrate_shim_internal(nid, callback, callback_data);
}

/* Externally-visible function to invoke migration. */
void migrate(int nid, void (*callback)(void *), void *callback_data)
{
  __migrate_shim_internal(nid, callback, callback_data);
}

/* Callback function & data for migration points inserted via compiler. */
void (*migrate_callback)(void *) __attribute__ ((section(".bss.migrate_callback"))) = NULL;
void *migrate_callback_data __attribute__ ((section(".bss.migrate_callback_data"))) = NULL;

/* Register callback function for compiler-inserted migration points. */
void register_migrate_callback(void (*callback)(void*), void *callback_data)
{
  migrate_callback = callback;
  migrate_callback_data = callback_data;
}

/* Hook inserted by compiler at the beginning of a function. */
void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
  int nid = do_migrate(this_fn);
  if (nid >= 0)
    __migrate_shim_internal(nid, migrate_callback, migrate_callback_data);
}

/* Hook inserted by compiler at the end of a function. */
void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
  int nid = do_migrate(this_fn);
  if (nid >= 0)
    __migrate_shim_internal(nid, migrate_callback, migrate_callback_data);
}

