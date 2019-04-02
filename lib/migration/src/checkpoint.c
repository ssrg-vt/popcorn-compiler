#if _GBL_VARIABLE_MIGRATE == 1
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stack_transform.h>
#include "platform.h"
#include "migrate.h"
#include "config.h"
#include "arch.h"
#include "internal.h"
#include "mapping.h"
#include "debug.h"


/************************************************/
volatile long __migrate_gb_variable = -1;
static inline int do_migrate(void __attribute__ ((unused)) * fn)
{
	return __migrate_gb_variable;
}

static inline void clear_migrate_flag()
{
	__migrate_gb_variable = -1;
	asm volatile("": : :"memory");
}

static volatile long __restore_context=0;
static inline int get_restore_context()
{
	return __restore_context;
}

static inline void set_restore_context(int val)
{
	__restore_context=val;
}

#define MUSL_PTHREAD_DESCRIPTOR_SIZE 288

/* musl-libc's architecture-specific function for setting the TLS pointer */
int __set_thread_area(void *);

/*
 * Convert a pointer to the start of the TLS region to the
 * architecture-specific thread pointer.  Derived from musl-libc's
 * per-architecture thread-pointer locations -- see each architecture's
 * "pthread_arch.h" file.
 */
static inline void *get_thread_pointer(void *raw_tls, enum arch dest)
{
	switch (dest) {
	case ARCH_AARCH64:
		return raw_tls - 16;
	case ARCH_POWERPC64:
		return raw_tls + 0x7000;	// <- TODO verify
	case ARCH_X86_64:
		return raw_tls - MUSL_PTHREAD_DESCRIPTOR_SIZE;
	default:
		assert(0 && "Unsupported architecture!");
		return NULL;
	}
}

//TODO: per thread
union {
	struct regset_aarch64 aarch;
	struct regset_powerpc64 powerpc;
	struct regset_x86_64 x86;
} regs_dst;

void* tls_dst=0x0;

/* Generate a call site to get rewriting metadata for outermost frame. */
static void* __attribute__((noinline))
get_call_site() { return __builtin_return_address(0); };

static void dummy(){printf("%s: called\n", __func__);};

void
__migrate_shim_internal(enum arch dst_arch, void (*callback) (void *), void *callback_data)
{
	int err;

	if (!get_restore_context())		// Invoke migration
	{
		unsigned long sp = 0, bp = 0;
		union {
			struct regset_aarch64 aarch;
			struct regset_powerpc64 powerpc;
			struct regset_x86_64 x86;
		} regs_src;
		GET_LOCAL_REGSET(regs_src);

		err = 0;
		switch (dst_arch) {
			case ARCH_AARCH64:
				err = !REWRITE_STACK(regs_src, regs_dst, dst_arch);
				regs_dst.aarch.__magic = 0xAABCBDEADBEAF; 
        			dump_regs_aarch64(&regs_dst.aarch, LOG_FILE);
				break;
			case ARCH_X86_64:
				err = !REWRITE_STACK(regs_src, regs_dst, dst_arch);
				regs_dst.x86.__magic = 0xA8664DEADBEAF; 
        			dump_regs_x86_64(&regs_dst.x86, LOG_FILE);
				break;
			case ARCH_POWERPC64:
				err = !REWRITE_STACK(regs_src, regs_dst, dst_arch);
        			dump_regs_powerpc64(&regs_dst.powerpc, LOG_FILE);
				break;
			default: assert(0 && "Unsupported architecture!");
		}
		if (err) {
			fprintf(stderr, "Could not rewrite stack!\n");
			return;
		}
		//fprintf(stdout, "dest arch is %d\n", dst_arch);
      		tls_dst = get_thread_pointer(GET_TLS_POINTER, dst_arch);
		//fprintf(stdout, "%s %d\n", __func__, __LINE__);
		set_restore_context(1);
		//fprintf(stdout, "%s %d\n", __func__, __LINE__);
		clear_migrate_flag();
		signal(SIGALRM, dummy);
		sigset_t old_sig_set;
		sigset_t new_sig_set;
		sigemptyset(&new_sig_set);
		//sigfillset(&new_sig_set, SIGALRM); sigdelset(&new_sig_set, SIGALRM);
		sigaddset(&new_sig_set, SIGALRM);
		sigprocmask(SIG_UNBLOCK, &new_sig_set, &old_sig_set);
		raise(SIGALRM); /* wil be catched by ptrace */
		sigprocmask(SIG_SETMASK, &old_sig_set, NULL);
		//fprintf(stdout, "%s raising done %d\n", __func__, __LINE__);
		//while(1);
		return;
	}
	// Post-migration

	// Translate between architecture-specific thread descriptors
	// Note: TLS is now invalid until after migration!
	//__set_thread_area(get_thread_pointer(GET_TLS_POINTER, CURRENT_ARCH));
	set_restore_context(0);

}

/* Check if we should migrate, and invoke migration. */
void check_migrate(void (*callback) (void *), void *callback_data)
{
	enum arch dst_arch = do_migrate(NULL);
	if (dst_arch >= 0)
	{
#if _DEBUG == 1
		fprintf(stderr, "Starting migration to node %d\n", dst_arch);
#endif
		__migrate_shim_internal(dst_arch, callback, callback_data);
	}
}

/* Invoke migration to a particular node if we're not already there. */
void migrate(enum arch dst_arch, void (*callback) (void *), void *callback_data)
{
	__migrate_shim_internal(dst_arch, callback, callback_data);
}

#endif
