/*
 * Implementation of riscv64-specific value getters/setters and virtual stack
 * unwinding.
 *
 * Callee-saved register information is derived from the ARM ABI:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
 *
 * Author: Cesar Philippidis
 * Date: 2/7/2020
 */

#include "definitions.h"
#include "arch/riscv64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define RISCV64_FBP_REG X8
#define RISCV64_LINK_REG X1

static void* regset_default_riscv64(void);
static void* regset_init_riscv64(const void* regs);
static void regset_free_riscv64(void* regset);
static void regset_clone_riscv64(const void* src, void* dest);
static void regset_copyin_riscv64(void* regset, const void* regs);
static void regset_copyout_riscv64(const void* regset, void* regs);

static void* pc_riscv64(const void* regset);
static void* sp_riscv64(const void* regset);
static void* fbp_riscv64(const void* regset);
static void* ra_reg_riscv64(const void* regset);

static void set_pc_riscv64(void* regset, void* pc);
static void set_sp_riscv64(void* regset, void* sp);
static void set_fbp_riscv64(void* regset, void* fp);
static void set_ra_reg_riscv64(void* regset, void* ra);
static void setup_fbp_riscv64(void* regset, void* cfa);

static uint16_t reg_size_riscv64(uint16_t reg);
static void* reg_riscv64(void* regset, uint16_t reg);

/*
 * riscv64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regops_t regs_riscv64 = {
  .num_regs = RISCV64_NUM_REGS,
  .has_ra_reg = true,
  .regset_size = sizeof(struct regset_riscv64),
  .fbp_regnum = RISCV64_FBP_REG,

  .regset_default = regset_default_riscv64,
  .regset_init = regset_init_riscv64,
  .regset_free = regset_free_riscv64,
  .regset_clone = regset_clone_riscv64,
  .regset_copyin = regset_copyin_riscv64,
  .regset_copyout = regset_copyout_riscv64,

  .pc = pc_riscv64,
  .sp = sp_riscv64,
  .fbp = fbp_riscv64,
  .ra_reg = ra_reg_riscv64,

  .set_pc = set_pc_riscv64,
  .set_sp = set_sp_riscv64,
  .set_fbp = set_fbp_riscv64,
  .set_ra_reg = set_ra_reg_riscv64,
  .setup_fbp = setup_fbp_riscv64,

  .reg_size = reg_size_riscv64,
  .reg = reg_riscv64,
};

///////////////////////////////////////////////////////////////////////////////
// riscv64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* regset_default_riscv64()
{
  struct regset_riscv64* new = calloc(1, sizeof(struct regset_riscv64));
  ASSERT(new, "could not allocate regset (riscv64)\n");
  return new;
}

static void* regset_init_riscv64(const void* regs)
{
  struct regset_riscv64* new = MALLOC(sizeof(struct regset_riscv64));
  ASSERT(new, "could not allocate regset (riscv64)\n");
  *new = *(struct regset_riscv64*)regs;
  return new;
}

static void regset_free_riscv64(void* regset)
{
  free(regset);
}

static void regset_clone_riscv64(const void* src, void* dest)
{
  const struct regset_riscv64* srcregs = (const struct regset_riscv64*)src;
  struct regset_riscv64* destregs = (struct regset_riscv64*)dest;
  *destregs = *srcregs;
}

static void regset_copyin_riscv64(void* in, const void* out)
{
  struct regset_riscv64* cur = (struct regset_riscv64*)in;
  *cur = *(struct regset_riscv64*)out;
}

static void regset_copyout_riscv64(const void* in, void* out)
{
  const struct regset_riscv64* cur = (const struct regset_riscv64*)in;
  *(struct regset_riscv64*)out = *cur;
}

static void* pc_riscv64(const void* regset)
{
  const struct regset_riscv64* cur = (const struct regset_riscv64*)regset;
  return cur->pc;
}

static void* sp_riscv64(const void* regset)
{
  const struct regset_riscv64* cur = (const struct regset_riscv64*)regset;
  return cur->sp;
}

static void* fbp_riscv64(const void* regset)
{
  const struct regset_riscv64* cur = (const struct regset_riscv64*)regset;
  return (void*)cur->x[RISCV64_FBP_REG];
}

static void* ra_reg_riscv64(const void* regset)
{
  const struct regset_riscv64* cur = (const struct regset_riscv64*)regset;
  return (void*)cur->x[RISCV64_LINK_REG];
}

static void set_pc_riscv64(void* regset, void* pc)
{
  struct regset_riscv64* cur = (struct regset_riscv64*)regset;
  cur->pc = pc;
}

static void set_sp_riscv64(void* regset, void* sp)
{
  struct regset_riscv64* cur = (struct regset_riscv64*)regset;
  cur->sp = sp;
}

static void set_fbp_riscv64(void* regset, void* fp)
{
  struct regset_riscv64* cur = (struct regset_riscv64*)regset;
  cur->x[RISCV64_FBP_REG] = (uint64_t)fp;
}

static void set_ra_reg_riscv64(void* regset, void* ra)
{
  struct regset_riscv64* cur = (struct regset_riscv64*)regset;
  cur->x[RISCV64_LINK_REG] = (uint64_t)ra;
}

static void setup_fbp_riscv64(void* regset, void* cfa)
{
  ASSERT(cfa, "Null canonical frame address\n");
  struct regset_riscv64* cur = (struct regset_riscv64*)regset;
  cur->x[RISCV64_FBP_REG] = (uint64_t)cfa - 0x10;
}

static uint16_t reg_size_riscv64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers */
  case X0: case X1: case X2: case X3: case X4: case X5: case X6:
  case X7: case X8: case X9: case X10: case X11: case X12: case X13:
  case X14: case X15: case X16: case X17: case X18: case X19: case X20:
  case X21: case X22: case X23: case X24: case X25: case X26: case X27:
  case X28: case X29: case X30:
    return sizeof(uint64_t);

  /* Floating-point registers */
  case F0: case F1: case F2: case F3: case F4: case F5: case F6:
  case F7: case F8: case F9: case F10: case F11: case F12: case F13:
  case F14: case F15: case F16: case F17: case F18: case F19: case F20:
  case F21: case F22: case F23: case F24: case F25: case F26: case F27:
  case F28: case F29: case F30: case F31:
    return sizeof(uint64_t);

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %d (riscv64)\n", reg);
  return 0;
}

static void* reg_riscv64(void* regset, uint16_t reg)
{
  struct regset_riscv64* cur = (struct regset_riscv64*)regset;

  switch(reg)
  {
  case X0: return &cur->x[0];
  case X1: return &cur->x[1];
  case X2: return &cur->sp;
  case X3: return &cur->x[3];
  case X4: return &cur->x[4];
  case X5: return &cur->x[5];
  case X6: return &cur->x[6];
  case X7: return &cur->x[7];
  case X8: return &cur->x[8];
  case X9: return &cur->x[9];
  case X10: return &cur->x[10];
  case X11: return &cur->x[11];
  case X12: return &cur->x[12];
  case X13: return &cur->x[13];
  case X14: return &cur->x[14];
  case X15: return &cur->x[15];
  case X16: return &cur->x[16];
  case X17: return &cur->x[17];
  case X18: return &cur->x[18];
  case X19: return &cur->x[19];
  case X20: return &cur->x[20];
  case X21: return &cur->x[21];
  case X22: return &cur->x[22];
  case X23: return &cur->x[23];
  case X24: return &cur->x[24];
  case X25: return &cur->x[25];
  case X26: return &cur->x[26];
  case X27: return &cur->x[27];
  case X28: return &cur->x[28];
  case X29: return &cur->x[29];
  case X30: return &cur->x[30];
  case X31: return &cur->x[31];
  case F0: return &cur->f[0];
  case F1: return &cur->f[1];
  case F2: return &cur->f[2];
  case F3: return &cur->f[3];
  case F4: return &cur->f[4];
  case F5: return &cur->f[5];
  case F6: return &cur->f[6];
  case F7: return &cur->f[7];
  case F8: return &cur->f[8];
  case F9: return &cur->f[9];
  case F10: return &cur->f[10];
  case F11: return &cur->f[11];
  case F12: return &cur->f[12];
  case F13: return &cur->f[13];
  case F14: return &cur->f[14];
  case F15: return &cur->f[15];
  case F16: return &cur->f[16];
  case F17: return &cur->f[17];
  case F18: return &cur->f[18];
  case F19: return &cur->f[19];
  case F20: return &cur->f[20];
  case F21: return &cur->f[21];
  case F22: return &cur->f[22];
  case F23: return &cur->f[23];
  case F24: return &cur->f[24];
  case F25: return &cur->f[25];
  case F26: return &cur->f[26];
  case F27: return &cur->f[27];
  case F28: return &cur->f[28];
  case F29: return &cur->f[29];
  case F30: return &cur->f[30];
  case F31: return &cur->f[31];
  /*
   * TODO:
   *   33: ELR_mode
   */
  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (riscv64)\n", reg);
  return NULL;
}

