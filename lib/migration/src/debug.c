/*
 * Migration debugging helper functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 3/3/2018
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "config.h"
#include "platform.h"
#include "migrate.h"
#include "debug.h"

/*
 * Helpers for dumping register contents.
 */

#define UINT64( val ) ((uint64_t)val)
#define UPPER_HALF_128( val ) \
  ({ \
    uint64_t chunk; \
    memcpy(&chunk, ((void *)(&val) + 8), 8); \
    chunk; \
  })
#define LOWER_HALF_128( val ) \
  ({ \
    uint64_t chunk; \
    memcpy(&chunk, &(val), 8); \
    chunk; \
  })

void dump_regs_aarch64(const struct regset_aarch64 *regset, const char *log)
{
  size_t i;
  FILE *stream;

  assert(regset && "Invalid regset");
  if(log)
  {
    stream = fopen(log, "a");
    if(!stream) return;
  }
  else stream = stderr;

  fprintf(stream, "Register set located @ %p\n", regset);
  fprintf(stream, "Program counter: %p\n", regset->pc);
  fprintf(stream, "Stack pointer: %p\n", regset->sp);

  for(i = 0; i < 31; i++)
  {
    if(i == 29) fprintf(stream, "Frame pointer / ");
    else if(i == 30) fprintf(stream, "Link register / ");
    fprintf(stream, "X%lu: %ld / %lu / %lx\n", i,
            regset->x[i], regset->x[i], regset->x[i]);
  }

  for(i = 0; i < 32; i++)
  {
    uint64_t upper = UPPER_HALF_128(regset->v[i]),
             lower = LOWER_HALF_128(regset->v[i]);
    fprintf(stream, "V%lu: ", i);
    if(upper) fprintf(stream, "%lx", upper);
    fprintf(stream, "%lx\n", lower);
  }

  fclose(stream);
}

void dump_regs_powerpc64(const struct regset_powerpc64 *regset,
                         const char *log)
{
  size_t i;
  FILE *stream;

  assert(regset && "Invalid regset");
  if(log)
  {
    stream = fopen(log, "a");
    if(!stream) return;
  }
  else stream = stderr;

  fprintf(stream, "Register set located @ %p\n", regset);
  fprintf(stream, "Program counter: %p\n", regset->pc);
  fprintf(stream, "Link register: %p\n", regset->lr);
  fprintf(stream, "Counter: %ld / %lu / %lx / %p\n",
          UINT64(regset->ctr), UINT64(regset->ctr), UINT64(regset->ctr),
          regset->ctr);

  for(i = 0; i < 32; i++)
  {
    if(i == 1) fprintf(stream, "Stack pointer / ");
    else if(i == 2) fprintf(stream, "Table-of-contents pointer / ");
    else if(i == 13) fprintf(stream, "Frame-base pointer / ");
    fprintf(stream, "R%lu: %ld / %lu / %lx\n", i,
            regset->r[i], regset->r[i], regset->r[i]);
  }

  for(i = 0; i < 32; i++)
    fprintf(stream, "F%lu: %lx\n", i, regset->f[i]);

  fclose(stream);
}

void dump_regs_x86_64(const struct regset_x86_64 *regset, const char *log)
{
  size_t i;
  FILE *stream;

  assert(regset && "Invalid regset");
  if(log)
  {
    stream = fopen(log, "a");
    if(!stream) return;
  }
  else stream = stderr;

  fprintf(stream, "Register set located @ %p\n", regset);
  fprintf(stream, "Instruction pointer: %p\n", regset->rip);
  fprintf(stream, "RAX: %ld / %lu / %lx\n",
          regset->rax, regset->rax, regset->rax);
  fprintf(stream, "RDX: %ld / %lu / %lx\n",
          regset->rdx, regset->rdx, regset->rdx);
  fprintf(stream, "RCX: %ld / %lu / %lx\n",
          regset->rcx, regset->rcx, regset->rcx);
  fprintf(stream, "RBX: %ld / %lu / %lx\n",
          regset->rbx, regset->rbx, regset->rbx);
  fprintf(stream, "RSI: %ld / %lu / %lx\n",
          regset->rsi, regset->rsi, regset->rsi);
  fprintf(stream, "RDI: %ld / %lu / %lx\n",
          regset->rdi, regset->rdi, regset->rdi);
  fprintf(stream, "Frame pointer / RBP: %ld / %lu / %lx\n",
          regset->rbp, regset->rbp, regset->rbp);
  fprintf(stream, "Stack pointer / RSP: %ld / %lu / %lx\n",
          regset->rsp, regset->rsp, regset->rsp);
  fprintf(stream, "R8: %ld / %lu / %lx\n",
          regset->r8, regset->r8, regset->r8);
  fprintf(stream, "R9: %ld / %lu / %lx\n",
          regset->r9, regset->r9, regset->r9);
  fprintf(stream, "R10: %ld / %lu / %lx\n",
          regset->r10, regset->r10, regset->r10);
  fprintf(stream, "R11: %ld / %lu / %lx\n",
          regset->r11, regset->r11, regset->r11);
  fprintf(stream, "R12: %ld / %lu / %lx\n",
          regset->r12, regset->r12, regset->r12);
  fprintf(stream, "R13: %ld / %lu / %lx\n",
          regset->r13, regset->r13, regset->r13);
  fprintf(stream, "R14: %ld / %lu / %lx\n",
          regset->r14, regset->rax, regset->rax);
  fprintf(stream, "R15: %ld / %lu / %lx\n",
          regset->r15, regset->r15, regset->r15);

  for(i = 0; i < 16; i++)
  {
    uint64_t upper = UPPER_HALF_128(regset->xmm[i]),
             lower = LOWER_HALF_128(regset->xmm[i]);
    fprintf(stream, "XMM%lu: ", i);
    if(upper) fprintf(stream, "%lx", upper);
    fprintf(stream, "%lx\n", lower);
  }

  fclose(stream);
}

void dump_regs(const void *regset, const char *log)
{
#if defined __aarch64__
  dump_regs_aarch64(regset, log);
#elif defined __powerpc64__
  dump_regs_powerpc64(regset, log);
#else /* x86_64 */
  dump_regs_x86_64(regset, log);
#endif
}

/* Per-node debugging structures */
typedef struct {
  size_t threads;
  int fd;
  pthread_mutex_t lock;
  char padding[PAGESZ - (2 * sizeof(size_t)) - sizeof(pthread_mutex_t)];
} remote_debug_t;

static __attribute__((aligned(PAGESZ))) remote_debug_t
debug_info[MAX_POPCORN_NODES];

static void segfault_handler(int sig, siginfo_t *info, void *ctx)
{
#if _LOG == 1
#define BUFSIZE 512
#define LOG_WRITE( format, ... ) \
  do { \
    char buf[BUFSIZE]; \
    int sz = snprintf(buf, BUFSIZE, format, __VA_ARGS__) + 1; \
    write(debug_info[nid].fd, buf, sz); \
  } while(0);

  // Note: *must* use trylock to ensure we don't block in signal handler
  int nid = popcorn_getnid();
  if(!pthread_mutex_trylock(debug_info[nid].lock) && debug_info[nid].fd)
  {
    LOG_WRITE("%d: segfault @ %p\n", info.si_pid, info.si_addr);
    pthread_mutex_unlock(debug_info[nid].lock);
  }
#endif
  // TODO do we need to migrate back to the origin before exiting?
  kill(getpid(), SIGSEGV);
  _Exit(SIGSEGV);
}

/*
 * If first thread to arrive on a node, open files and register signal handlers
 * for resilient remote crashes.
 */
void remote_debug_init(int nid)
{
  struct sigaction act;

  if(nid < 0 || nid >= MAX_POPCORN_NODES) return;

  pthread_mutex_lock(&debug_info[nid].lock);
  if(!debug_info[nid].threads) // First thread to arrive on node
  {
#if _LOG == 1
    char fn[32];
    snprintf(fn, 128, "/tmp/node-%d.log", nid);
    debug_info[nid].fd = open(fn, O_CREAT | O_APPEND);
#endif
    act.sa_sigaction = segfault_handler;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_restorer = NULL;
    sigaction(SIGSEGV, &act, NULL);
  }
  debug_info[nid].threads += 1;
  pthread_mutex_unlock(&debug_info[nid].lock);
}

/*
 * If the last thread to leave a node, close files.
 */
void remote_debug_cleanup(int nid)
{
  if(nid < 0 || nid >= MAX_POPCORN_NODES) return;

  pthread_mutex_lock(&debug_info[nid].lock);
  debug_info[nid].threads -= 1;
  if(!debug_info[nid].threads)
  {
#if _LOG == 1
    close(debug_info[nid].fd);
    debug_info[nid].fd = 0;
#endif
    // TODO do we want to clean up signal handler?
  }
  pthread_mutex_unlock(&debug_info[nid].lock);
}

#if _CLEAN_CRASH == 1
static void __attribute__((constructor)) __init_debug_info()
{
  size_t i;
  for(i = 0; i < MAX_POPCORN_NODES; i++)
    pthread_mutex_init(&debug_info[i].lock, NULL);
}
#endif

