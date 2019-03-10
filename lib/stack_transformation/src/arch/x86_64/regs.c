/*
 * Implementation of x86-64-specific value getters/setters and virtual stack
 * unwinding.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/12/2015
 */

#include "definitions.h"
#include "arch/x86_64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

static void* regset_default_x86_64(void);
static void* regset_init_x86_64(const void* regs);
static void regset_free_x86_64(void* regset);
static void regset_clone_x86_64(const void* src, void* dest);
static void regset_clear_x86_64(void* regset);
static void regset_copyin_x86_64(void* regset, const void* regs);
static void regset_copy_arg_regs_x86_64(void* regset, const void* regs);
static void regset_copy_ret_regs_x86_64(void* regset, const void* regs);
static void regset_copyout_x86_64(const void* regset, void* regs);

static void* pc_x86_64(const void* regset);
static void* sp_x86_64(const void* regset);
static void* fbp_x86_64(const void* regset);
static void* ra_reg_x86_64(const void* regset);

static void set_pc_x86_64(void* regset, void* pc);
static void set_sp_x86_64(void* regset, void* sp);
static void set_fbp_x86_64(void* regset, void* fbp);
static void set_ra_reg_x86_64(void* regset, void* ra);
static void setup_fbp_x86_64(void* regset, void* cfa);

static uint16_t reg_size_x86_64(uint16_t reg);
static void* reg_x86_64(void* regset, uint16_t reg);

/*
 * x86-64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regops_t regs_x86_64 = {
  .num_regs = X86_64_NUM_REGS,
  .has_ra_reg = false,
  .regset_size = sizeof(struct regset_x86_64),
  .fbp_regnum = RBP,
  .sp_regnum = RSP,

  .regset_default = regset_default_x86_64,
  .regset_init = regset_init_x86_64,
  .regset_free = regset_free_x86_64,
  .regset_clone = regset_clone_x86_64,
  .regset_clear = regset_clear_x86_64,
  .regset_copyin = regset_copyin_x86_64,
  .regset_copy_arg_regs = regset_copy_arg_regs_x86_64,
  .regset_copy_ret_regs = regset_copy_ret_regs_x86_64,
  .regset_copyout = regset_copyout_x86_64,

  .pc = pc_x86_64,
  .sp = sp_x86_64,
  .fbp = fbp_x86_64,
  .ra_reg = ra_reg_x86_64,

  .set_pc = set_pc_x86_64,
  .set_sp = set_sp_x86_64,
  .set_fbp = set_fbp_x86_64,
  .set_ra_reg = set_ra_reg_x86_64,
  .setup_fbp = setup_fbp_x86_64,

  .reg_size = reg_size_x86_64,
  .reg = reg_x86_64,
};

///////////////////////////////////////////////////////////////////////////////
// x86-64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* regset_default_x86_64()
{
  struct regset_x86_64* new = calloc(1, sizeof(struct regset_x86_64));
  ASSERT(new, "could not allocate regset (x86-64)\n");
  return new;
}

static void* regset_init_x86_64(const void* regs)
{
  struct regset_x86_64* new = MALLOC(sizeof(struct regset_x86_64));
  ASSERT(new, "could not allocate regset (x86-64)\n");
  *new = *(struct regset_x86_64*)regs;
  return new;
}

static void regset_free_x86_64(void* regset)
{
  free(regset);
}

static void regset_clone_x86_64(const void* src, void* dest)
{
  const struct regset_x86_64* srcregs = (const struct regset_x86_64*)src;
  struct regset_x86_64* destregs = (struct regset_x86_64*)dest;
  *destregs = *srcregs;
}

static void regset_clear_x86_64(void *regs)
{
  memset(regs, 0, sizeof(struct regset_x86_64));
}

static void regset_copyin_x86_64(void* in, const void* out)
{
  struct regset_x86_64* cur = (struct regset_x86_64*)in;
  *cur = *(struct regset_x86_64*)out;
}

static void regset_copy_arg_regs_x86_64(void* in, const void* out)
{
  struct regset_x86_64* cur = (struct regset_x86_64*)in,
                      * input = (struct regset_x86_64*)out;

  cur->rdi = input->rdi;
  cur->rsi = input->rsi;
  cur->rdx = input->rdx;
  cur->rcx = input->rcx;
  cur->r8 = input->r8;
  cur->r9 = input->r9;

  cur->xmm[0] = input->xmm[0];
  cur->xmm[1] = input->xmm[1];
  cur->xmm[2] = input->xmm[2];
  cur->xmm[3] = input->xmm[3];
  cur->xmm[4] = input->xmm[4];
  cur->xmm[5] = input->xmm[5];
  cur->xmm[6] = input->xmm[6];
  cur->xmm[7] = input->xmm[7];
}

static void regset_copy_ret_regs_x86_64(void* in, const void* out)
{
  struct regset_x86_64* cur = (struct regset_x86_64*)in,
                      * input = (struct regset_x86_64*)out;

  cur->rax = input->rax;
  cur->rdx = input->rdx;

  cur->xmm[0] = input->xmm[0];
  cur->xmm[1] = input->xmm[1];
}

static void regset_copyout_x86_64(const void* in, void* out)
{
  const struct regset_x86_64* cur = (const struct regset_x86_64*)in;
  *(struct regset_x86_64*)out = *cur;
}

static void* pc_x86_64(const void* regset)
{
  const struct regset_x86_64* cur = (const struct regset_x86_64*)regset;
  return cur->rip;
}

static void* sp_x86_64(const void* regset)
{
  const struct regset_x86_64* cur = (const struct regset_x86_64*)regset;
  return (void*)cur->rsp;
}

static void* fbp_x86_64(const void* regset)
{
  const struct regset_x86_64* cur = (const struct regset_x86_64*)regset;
  return (void*)cur->rbp;
}

static void* ra_reg_x86_64(const void* __attribute__((unused)) regset)
{
  // N/a for x86-64, return address is always stored on the stack
  ST_ERR(1, "no return-address register for x86-64\n");
  return NULL;
}

static void set_pc_x86_64(void* regset, void* pc)
{
  struct regset_x86_64* cur = (struct regset_x86_64*)regset;
  cur->rip = pc;
}

static void set_sp_x86_64(void* regset, void* sp)
{
  struct regset_x86_64* cur = (struct regset_x86_64*)regset;
  cur->rsp = (uint64_t)sp;
}

static void set_fbp_x86_64(void* regset, void* fbp)
{
  struct regset_x86_64* cur = (struct regset_x86_64*)regset;
  cur->rbp = (uint64_t)fbp;
}

static void set_ra_reg_x86_64(void*  __attribute__((unused)) regset,
                              void* __attribute__((unused)) val)
{
  // N/a for x86-64, return address is always stored on the stack
  ST_ERR(1, "no return-address register for x86-64\n");
}

static void setup_fbp_x86_64(void* regset, void* cfa)
{
  ASSERT(cfa, "Null canonical frame address\n");
  struct regset_x86_64* cur = (struct regset_x86_64*)regset;
  cur->rbp = (uint64_t)cfa - 0x10;
}

static uint16_t reg_size_x86_64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers */
  case RAX: case RDX: case RCX: case RBX: case RSI: case RDI: case RBP:
  case RSP: case R8:  case R9 : case R10: case R11: case R12: case R13:
  case R14: case R15: case RIP:
    return sizeof(uint64_t);

  /* XMM floating-point registers */
  case XMM0 : case XMM1 : case XMM2 : case XMM3 : case XMM4 : case XMM5 :
  case XMM6 : case XMM7 : case XMM8 : case XMM9 : case XMM10: case XMM11:
  case XMM12: case XMM13: case XMM14: case XMM15:
    return sizeof(unsigned __int128);

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (x86-64)\n", reg);
  return 0;
}

static void* reg_x86_64(void* regset, uint16_t reg)
{
  struct regset_x86_64* cur = (struct regset_x86_64*)regset;

  switch(reg)
  {
  case RAX: return &cur->rax;
  case RDX: return &cur->rdx;
  case RCX: return &cur->rcx;
  case RBX: return &cur->rbx;
  case RSI: return &cur->rsi;
  case RDI: return &cur->rdi;
  case RBP: return &cur->rbp;
  case RSP: return &cur->rsp;
  case R8: return &cur->r8;
  case R9: return &cur->r9;
  case R10: return &cur->r10;
  case R11: return &cur->r11;
  case R12: return &cur->r12;
  case R13: return &cur->r13;
  case R14: return &cur->r14;
  case R15: return &cur->r15;
  case RIP: return &cur->rip;
  case XMM0: return &cur->xmm[0];
  case XMM1: return &cur->xmm[1];
  case XMM2: return &cur->xmm[2];
  case XMM3: return &cur->xmm[3];
  case XMM4: return &cur->xmm[4];
  case XMM5: return &cur->xmm[5];
  case XMM6: return &cur->xmm[6];
  case XMM7: return &cur->xmm[7];
  case XMM8: return &cur->xmm[8];
  case XMM9: return &cur->xmm[9];
  case XMM10: return &cur->xmm[10];
  case XMM11: return &cur->xmm[11];
  case XMM12: return &cur->xmm[12];
  case XMM13: return &cur->xmm[13];
  case XMM14: return &cur->xmm[14];
  case XMM15: return &cur->xmm[15];
  /*
   * TODO:
   *   33-40: st[0] - st[7]
   *   41-48: st[0] - st[7] (MMX registers mm[0] - mm[7])
   *   49: rflags
   *   50: es
   *   51: cs
   *   52: ss
   *   53: ds
   *   54: fs
   *   55: gs
   *   58: fs.base
   *   59: gs.base
   *   62: tr
   *   62: ldtr
   *   64: mxcsr
   *   65: fcw
   *   66: fsw
   */
  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (x86-64)\n", reg);
  return NULL;
}

