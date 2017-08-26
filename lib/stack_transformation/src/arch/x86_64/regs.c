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

#define X86_64_NUM_REGS 67

static regset_t regset_default_x86_64(void);
static regset_t regset_init_x86_64(const void* regs);
static void regset_free_x86_64(regset_t regset);
static void regset_clone_x86_64(const_regset_t src, regset_t dest);
static void regset_copyin_x86_64(regset_t regset, const void* regs);
static void regset_copyout_x86_64(const_regset_t regset, void* regs);

static void* pc_x86_64(const_regset_t regset);
static void* sp_x86_64(const_regset_t regset);
static void* fbp_x86_64(const_regset_t regset);
static void* ra_reg_x86_64(const_regset_t regset);

static void set_pc_x86_64(regset_t regset, void* pc);
static void set_sp_x86_64(regset_t regset, void* sp);
static void set_fbp_x86_64(regset_t regset, void* fbp);
static void set_ra_reg_x86_64(regset_t regset, void* ra);
static void setup_fbp_x86_64(regset_t regset, void* cfa);

static uint16_t reg_size_x86_64(uint16_t reg);
static void* reg_x86_64(regset_t regset, uint16_t reg);

/*
 * Internal definition of x86-64 object, contains x86-64 registers in addition
 * to common fields & functions.
 */
typedef struct regset_obj_x86_64
{
  struct regset_t common;
  struct regset_x86_64 regs;
} regset_obj_x86_64;

/*
 * x86-64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regops_t regs_x86_64 = {
  .num_regs = X86_64_NUM_REGS,
  .has_ra_reg = false,
  .regset_size = sizeof(regset_obj_x86_64),
  .fbp_regnum = RBP,

  .regset_default = regset_default_x86_64,
  .regset_init = regset_init_x86_64,
  .regset_free = regset_free_x86_64,
  .regset_clone = regset_clone_x86_64,
  .regset_copyin = regset_copyin_x86_64,
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

static regset_t regset_default_x86_64()
{
  regset_obj_x86_64* new = calloc(1, sizeof(regset_obj_x86_64));
  ASSERT(new, "could not allocate regset (x86-64)\n");
  new->common.initialized = true;
  return (regset_t)new;
}

static regset_t regset_init_x86_64(const void* regs)
{
  regset_obj_x86_64* new = malloc(sizeof(regset_obj_x86_64));
  ASSERT(new, "could not allocate regset (x86-64)\n");
  new->common.initialized = true;
  new->regs = *(struct regset_x86_64*)regs;
  return (regset_t)new;
}

static void regset_free_x86_64(regset_t regset)
{
  free(regset);
}

static void regset_clone_x86_64(const_regset_t src, regset_t dest)
{
  const regset_obj_x86_64* srcregs = (const regset_obj_x86_64*)src;
  regset_obj_x86_64* destregs = (regset_obj_x86_64*)dest;
  *destregs = *srcregs;
}

static void regset_copyin_x86_64(regset_t regset, const void* regs)
{
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;
  cur->common.initialized = true;
  cur->regs = *(struct regset_x86_64*)regs;
}

static void regset_copyout_x86_64(const_regset_t regset, void* regs)
{
  const regset_obj_x86_64* cur = (const regset_obj_x86_64*)regset;
  *(struct regset_x86_64*)regs = cur->regs;
}

static void* pc_x86_64(const_regset_t regset)
{
  const regset_obj_x86_64* cur = (const regset_obj_x86_64*)regset;
  return cur->regs.rip;
}

static void* sp_x86_64(const_regset_t regset)
{
  const regset_obj_x86_64* cur = (const regset_obj_x86_64*)regset;
  return (void*)cur->regs.rsp;
}

static void* fbp_x86_64(const_regset_t regset)
{
  const regset_obj_x86_64* cur = (const regset_obj_x86_64*)regset;
  return (void*)cur->regs.rbp;
}

static void* ra_reg_x86_64(const_regset_t regset)
{
  // N/a for x86-64, return address is always stored on the stack
  ST_ERR(1, "no return-address register for x86-64\n");
  return NULL;
}

static void set_pc_x86_64(regset_t regset, void* pc)
{
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;
  cur->regs.rip = pc;
}

static void set_sp_x86_64(regset_t regset, void* sp)
{
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;
  cur->regs.rsp = (uint64_t)sp;
}

static void set_fbp_x86_64(regset_t regset, void* fbp)
{
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;
  cur->regs.rbp = (uint64_t)fbp;
}

static void set_ra_reg_x86_64(regset_t regset, void* val)
{
  // N/a for x86-64, return address is always stored on the stack
  ST_ERR(1, "no return-address register for x86-64\n");
}

static void setup_fbp_x86_64(regset_t regset, void* cfa)
{
  ASSERT(cfa, "Null canonical frame address\n");
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;
  cur->regs.rbp = (uint64_t)cfa - 0x10;
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

static void* reg_x86_64(regset_t regset, uint16_t reg)
{
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;

  switch(reg)
  {
  case RAX: return &cur->regs.rax;
  case RDX: return &cur->regs.rdx;
  case RCX: return &cur->regs.rcx;
  case RBX: return &cur->regs.rbx;
  case RSI: return &cur->regs.rsi;
  case RDI: return &cur->regs.rdi;
  case RBP: return &cur->regs.rbp;
  case RSP: return &cur->regs.rsp;
  case R8: return &cur->regs.r8;
  case R9: return &cur->regs.r9;
  case R10: return &cur->regs.r10;
  case R11: return &cur->regs.r11;
  case R12: return &cur->regs.r12;
  case R13: return &cur->regs.r13;
  case R14: return &cur->regs.r14;
  case R15: return &cur->regs.r15;
  case RIP: return &cur->regs.rip;
  case XMM0: return &cur->regs.xmm[0];
  case XMM1: return &cur->regs.xmm[1];
  case XMM2: return &cur->regs.xmm[2];
  case XMM3: return &cur->regs.xmm[3];
  case XMM4: return &cur->regs.xmm[4];
  case XMM5: return &cur->regs.xmm[5];
  case XMM6: return &cur->regs.xmm[6];
  case XMM7: return &cur->regs.xmm[7];
  case XMM8: return &cur->regs.xmm[8];
  case XMM9: return &cur->regs.xmm[9];
  case XMM10: return &cur->regs.xmm[10];
  case XMM11: return &cur->regs.xmm[11];
  case XMM12: return &cur->regs.xmm[12];
  case XMM13: return &cur->regs.xmm[13];
  case XMM14: return &cur->regs.xmm[14];
  case XMM15: return &cur->regs.xmm[15];
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

