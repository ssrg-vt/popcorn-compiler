/*
 * Migration debugging helper functions.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 3/3/2018
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
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

