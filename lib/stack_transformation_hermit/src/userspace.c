/*
 * User-space stack rewriting implementation.  Includes all APIs to boostrap
 * and re-write the stack for a currently-executing program, all in userspace.
 *
 * Author: Rob Lyerly
 * Date: 3/1/2016
 */

//#ifdef PTHREAD_TLS
//#include <pthread.h>
//#endif
#include <unistd.h>
#include <sys/resource.h>
#include <stdio.h>

#include "stack_transform.h"
#include "definitions.h"
#include "util.h"
#include "arch/x86_64/regs.h"
#include "arch/aarch64/regs.h"

// Hermit syscalls TODO include the right header
extern void *sys_stackaddr(void);
extern void *sys_stacksize(void);
extern long int sys_getpid();

///////////////////////////////////////////////////////////////////////////////
// File-local API & definitions
///////////////////////////////////////////////////////////////////////////////

static st_handle aarch64_handle = NULL;
static st_handle powerpc64_handle = NULL;
static st_handle x86_64_handle = NULL;
#if _TLS_IMPL == COMPILER_TLS
static __thread stack_bounds bounds = { .high = NULL, .low = NULL };
#else /* PTHREAD_TLS */
//static pthread_key_t stack_bounds_key = 0;
#endif

/*
 * Set inside of musl at __libc_start_main() to point to where environment
 * variables begin on the stack.
 */
extern void* __popcorn_stack_base;

/*
 * Touch stack pages up to the OS-defined stack size limit, so that the OS
 * allocates them and we can divide the stack in half for rewriting.  Also,
 * calculate stack bounds for main thread.
 */
static bool prep_stack(void);

/*
 * Get main thread's stack information from procfs.
 */
static bool get_main_stack(stack_bounds* bounds);

/*
 * Get thread's stack information from pthread library.
 */
static bool get_thread_stack(stack_bounds* bounds);

/*
 * Rewrite from the current stack (metadata provided by src_handle) to a
 * transformed stack (dest_handle).
 */
static int userspace_rewrite_internal(void* sp,
                                      void* src_regs,
                                      void* dest_regs,
                                      st_handle src_handle,
                                      st_handle dest_handle);

///////////////////////////////////////////////////////////////////////////////
// User-space initialization, rewriting & teardown
///////////////////////////////////////////////////////////////////////////////

/*
 * Program name, as invoked by the shell.
 */
// Pierre: FIXME
const char *___progname = "prog_x86_64";

/*
 * Binary names.  User-code can define these symbols to override these
 * definitions in order to provide the names transparently.
 */
char* __attribute__((weak)) aarch64_fn = NULL;
static bool alloc_aarch64_fn = false;
char* __attribute__((weak)) powerpc64_fn = NULL;
static bool alloc_powerpc64_fn = false;
char* __attribute__((weak)) x86_64_fn = NULL;
static bool alloc_x86_64_fn = false;

/*
 * Initialize rewriting meta-data on program startup.  Users *must* set the
 * names of binaries using one of the three methods described below.
 */
void __st_userspace_ctor(void)
{
  /* Initialize the stack for the main thread. */
  if(!prep_stack())
  {
    ST_WARN("could not prepare stack for user-space rewriting\n");
    return;
  }

  /* Prepare libELF. */
  if(elf_version(EV_CURRENT) == EV_NONE)
  {
    ST_WARN("could not prepare libELF for reading binary\n");
    return;
  }

  /*
   * Initialize ST handles - tries the following approaches to finding the
   * binaries:
   *
   * 1. Check environment variables (defined in config.h)
   * 2. Check if application has overridden file name symbols (defined above)
   * 3. Add architecture suffixes to current binary name (defined by libc)
   */
  if(getenv(ENV_AARCH64_BIN)) aarch64_handle = st_init(getenv(ENV_AARCH64_BIN));
  else if(aarch64_fn) aarch64_handle = st_init(aarch64_fn);
  else {
    aarch64_fn = (char*)malloc(sizeof(char) * BUF_SIZE);
    snprintf(aarch64_fn, BUF_SIZE, "%s_aarch64", ___progname);
  }
  aarch64_handle = st_init(aarch64_fn);
  if(aarch64_handle) alloc_aarch64_fn = true;
  else { ST_WARN("could not initialize aarch64 handle\n"); }

  if(getenv(ENV_POWERPC64_BIN))
    powerpc64_handle = st_init(getenv(ENV_POWERPC64_BIN));
  else if(powerpc64_fn) powerpc64_handle = st_init(powerpc64_fn);
  else {
    powerpc64_fn = (char*)malloc(sizeof(char) * BUF_SIZE);
    snprintf(powerpc64_fn, BUF_SIZE, "%s_powerpc64", ___progname);
  }
  powerpc64_handle = st_init(powerpc64_fn);
  if(powerpc64_handle) alloc_powerpc64_fn = true;
  else { ST_WARN("could not initialize powerpc64 handle\n"); }

  if(getenv(ENV_X86_64_BIN)) x86_64_handle = st_init(getenv(ENV_X86_64_BIN));
  else if(x86_64_fn) x86_64_handle = st_init(x86_64_fn);
  else {
    x86_64_fn = (char*)malloc(sizeof(char) * BUF_SIZE);
    snprintf(x86_64_fn, BUF_SIZE, "%s_x86-64", ___progname);
  }
  x86_64_handle = st_init(x86_64_fn);
  if(x86_64_handle) alloc_x86_64_fn = true;
  else { ST_WARN("could not initialize x86-64 handle\n"); }
}

/*
 * Free stack-transformation memory.
 */
void __st_userspace_dtor(void)
{
  if(aarch64_handle)
  {
    st_destroy(aarch64_handle);
    if(alloc_aarch64_fn) free(aarch64_fn);
  }

  if(powerpc64_handle)
  {
    st_destroy(powerpc64_handle);
    if(alloc_powerpc64_fn) free(powerpc64_fn);
  }

  if(x86_64_handle)
  {
    st_destroy(x86_64_handle);
    if(alloc_x86_64_fn) free(x86_64_fn);
  }
}

/*
 * Get stack bounds for a thread.
 */
stack_bounds get_stack_bounds()
{
//  int retval;
  void* cur_stack;
  stack_bounds cur_bounds = {NULL, NULL};

  if(!get_thread_stack(&cur_bounds)) {
	  fprintf(stderr, "Cannot get stack bounds");
	  exit(1);
  }

#if 0 
  /* If not already resolved, get stack limits for thread. */
#if _TLS_IMPL == COMPILER_TLS
  if(bounds.high == NULL)
    if(!get_thread_stack(&bounds)) return cur_bounds;
  cur_bounds = bounds;
#else /* PTHREAD_TLS */
  stack_bounds* bounds_ptr;
  if(!(bounds_ptr = pthread_getspecific(stack_bounds_key)))
  {
    bounds_ptr = (stack_bounds*)malloc(sizeof(stack_bounds));
    ASSERT(bounds_ptr, "could not allocate memory for stack bounds\n");
    retval = pthread_setspecific(stack_bounds_key, bounds_ptr);
    if(retval) {
      ASSERT(!retval, "could not set TLS data for thread\n");
      return cur_bounds;
    }
    if(!get_thread_stack(bounds_ptr)) return cur_bounds;
  }
  cur_bounds = *bounds_ptr;
#endif
#endif

  /* Determine which half of stack we're currently using. */
#ifdef __aarch64__
  asm volatile("mov %0, sp" : "=r"(cur_stack) ::);
#elif defined __powerpc64__
  asm volatile("mr %0, 1" : "=r"(cur_stack) ::);
#elif defined __x86_64__
  asm volatile("movq %%rsp, %0" : "=g"(cur_stack) ::);
#endif
  if(cur_stack >= cur_bounds.low + B_STACK_OFFSET)
    cur_bounds.low += B_STACK_OFFSET;
  else cur_bounds.high = cur_bounds.low + B_STACK_OFFSET;

  return cur_bounds;
}

/*
 * Rewrite from source to destination stack.
 */
int st_userspace_rewrite(void* sp,
                         enum arch src_arch,
                         void* src_regs,
                         enum arch dest_arch,
                         void* dest_regs)
{
  st_handle src_handle, dest_handle;

  switch(src_arch)
  {
  case ARCH_AARCH64: src_handle = aarch64_handle; break;
  case ARCH_POWERPC64: src_handle = powerpc64_handle; break;
  case ARCH_X86_64: src_handle = x86_64_handle; break;
  default: ST_WARN("Unsupported source architecture!\n"); return 1;
  }

  if(!src_handle)
  {
    ST_WARN("Could not load rewriting information for source!");
    return 1;
  }

  switch(dest_arch)
  {
  case ARCH_AARCH64: dest_handle = aarch64_handle; break;
  case ARCH_POWERPC64: dest_handle = powerpc64_handle; break;
  case ARCH_X86_64: dest_handle = x86_64_handle; break;
  default: ST_WARN("Unsupported destination architecture!\n"); return 1;
  }

  if(!dest_handle)
  {
    ST_WARN("Could not rewriting information for destination!");
    return 1;
  }

  return userspace_rewrite_internal(sp, src_regs, dest_regs,
                                    src_handle, dest_handle);
}

///////////////////////////////////////////////////////////////////////////////
// File-local API (implementation)
///////////////////////////////////////////////////////////////////////////////

/*
 * Touch stack pages up to the OS-defined stack size limit, so that the OS
 * allocates them and we can divide the stack in half for rewriting.  Also,
 * calculate stack bounds for main thread.
 */
static bool prep_stack(void)
{
//  long ret;
  size_t offset;
#if 0 //_TLS_IMPL == PTHREAD_TLS
  struct rlimit rlim;
  stack_bounds bounds;
  stack_bounds* bounds_ptr;

  bounds_ptr = (stack_bounds*)malloc(sizeof(stack_bounds));
  ASSERT(bounds_ptr, "could not allocate memory for stack bounds\n");
  ret = pthread_key_create(&stack_bounds_key, free);
  ret |= pthread_setspecific(stack_bounds_key, bounds_ptr);
  ASSERT(!ret, "could not allocate TLS data for main thread\n");
#endif

  if(!get_main_stack(&bounds)) return false;

#if 0
  if((ret = getrlimit(RLIMIT_STACK, &rlim)) < 0) return false;
  if(!ret)
  {
    // Note: the Linux kernel grows the stack automatically, but some versions
    // check to ensure that the stack pointer is near the page being accessed.
    // To grow the stack:
    //   1. Save the current stack pointer
    //   2. Move stack pointer to lowest stack address (according to rlimit)
    //   3. Touch the page using the stack pointer
    //   4. Restore the original stack pointer
    bounds.low = bounds.high - rlim.rlim_cur;
#ifdef __aarch64__
    asm volatile("mov x27, sp;"
                 "mov sp, %0;"
                 "ldr x28, [sp];"
                 "mov sp, x27" : : "r" (bounds.low) : "x27", "x28");
#elif defined(__powerpc64__)
    asm volatile("mr 28, 1;"
                 "mr 1, %0;"
                 "ld 29, 0(1);"
                 "mr 1, 28" : : "r" (bounds.low) : "r28", "r29");
#elif defined(__x86_64__)
    asm volatile("mov %%rsp, %%r14;"
                 "mov %0, %%rsp;"
                 "mov (%%rsp), %%r15;"
                 "mov %%r14, %%rsp" : : "g" (bounds.low) : "r14", "r15");
#endif
  }

  ST_INFO("Prepped stack for main thread, addresses %p -> %p\n",
          bounds.low, bounds.high);
#endif

  /*
   * Get offset of main thread's stack pointer from stack base so we can avoid
   * clobbering argv & environment variables.
   */
  ASSERT(__popcorn_stack_base, "Stack base not correctly set by newlib\n");
  offset = (uint64_t)(bounds.high - __popcorn_stack_base);
  offset += (offset % 0x10 ? 0x10 - (offset % 0x10) : 0);
  bounds.high -= offset;
#if 0 //_TLS_IMPL == PTHREAD_TLS
  *bounds_ptr = bounds;
#endif
  return true;
}

/* Read stack information for the main thread from the procfs. */
static bool get_main_stack(stack_bounds* bounds)
{
	bounds->low = 0x0;
	bounds->low = sys_stackaddr();
	bounds->high = (void *)((uint64_t)bounds->low + (uint64_t)sys_stacksize());

	if(!bounds->low) {
		fprintf(stderr, "Cannot get stack location from hermitcore kernel\n");
		exit(1);
	}

	ST_INFO("Main stack limits: %p -> %p\n", bounds->low, bounds->high);
	return true;
}

/* Read stack information for cloned threads from the pthread library. */
static bool get_thread_stack(stack_bounds* bounds)
{
	bounds->low = 0x0;
	bounds->low = sys_stackaddr();
	bounds->high = (void *)((uint64_t)bounds->low + (uint64_t)sys_stacksize()) - 1;

	if(!bounds->low) {
		fprintf(stderr, "Cannot get stack location from hermitcore kernel\n");
		exit(1);
	}

	ST_INFO("Thread stack limits: %p -> %p\n", bounds->low, bounds->high);
	return true;
}

/*
 * Rewrite from source to destination stack.  Logically, divides 8MB stack in
 * half, detects which half we're currently using and rewrites to the other.
 */
static int userspace_rewrite_internal(void* sp,
                                      void* src_regs,
                                      void* dest_regs,
                                      st_handle src_handle,
                                      st_handle dest_handle)
{
  int retval = 0;
  void* stack_a, *stack_b, *cur_stack, *new_stack;
#if 0 //_TLS_IMPL == PTHREAD_TLS
  stack_bounds bounds;
  stack_bounds* bounds_ptr;
#endif

  if(!sp || !src_regs || !dest_regs || !src_handle || !dest_handle)
  {
    ST_WARN("invalid arguments\n");
    return 1;
  }

  /* If not already resolved, get stack limits for thread. */
#if _TLS_IMPL == COMPILER_TLS
  if(bounds.high == NULL)
    if(!get_thread_stack(&bounds)) return 1;
#else /* PTHREAD_TLS */
  if(!(bounds_ptr = pthread_getspecific(stack_bounds_key)))
  {
    bounds_ptr = (stack_bounds*)malloc(sizeof(stack_bounds));
    ASSERT(bounds_ptr, "could not allocate memory for stack bounds\n");
    retval = pthread_setspecific(stack_bounds_key, bounds_ptr);
    ASSERT(!retval, "could not set TLS data for thread\n");
    if(!get_thread_stack(bounds_ptr)) return 1;
  }
  bounds = *bounds_ptr;
#endif

  if(sp < bounds.low || bounds.high <= sp)
  {
    ST_WARN("invalid stack pointer\n");
    return 1;
  }

  ST_INFO("Thread %ld beginning re-write\n", sys_getpid());

  /* Divide stack into two halves. */
  stack_a = bounds.high;
  stack_b = bounds.low + B_STACK_OFFSET;

  /* Find which half the current stack uses and rewrite to other. */
  cur_stack = (sp >= stack_b) ? stack_a : stack_b;
  new_stack = (sp >= stack_b) ? stack_b : stack_a;
  ST_INFO("On stack %p, rewriting to %p\n", cur_stack, new_stack);
  if(st_rewrite_stack(src_handle, src_regs, cur_stack,
                      dest_handle, dest_regs, new_stack))
  {
    ST_WARN("stack transformation failed (%s -> %s)\n",
            arch_name(src_handle->arch), arch_name(dest_handle->arch));
    retval = 1;
  }

  return retval;
}

