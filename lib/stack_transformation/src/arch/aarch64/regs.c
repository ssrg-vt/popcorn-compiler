/*
 * Implementation of aarch64-specific value getters/setters and virtual stack
 * unwinding.
 *
 * Callee-saved register information is derived from the ARM ABI:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/12/2015
 */

#include "definitions.h"
#include "arch/aarch64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define AARCH64_FBP_REG X29
#define AARCH64_LINK_REG X30

static void* regset_default_aarch64(void);
static void* regset_init_aarch64(const void* regs);
static void regset_free_aarch64(void* regset);
static void regset_clone_aarch64(const void* src, void* dest);
static void regset_clear_aarch64(void* regset);
static void regset_copyin_aarch64(void* regset, const void* regs);
static void regset_copy_arg_regs_aarch64(void* regset, const void* regs);
static void regset_copyout_aarch64(const void* regset, void* regs);

static void* pc_aarch64(const void* regset);
static void* sp_aarch64(const void* regset);
static void* fbp_aarch64(const void* regset);
static void* ra_reg_aarch64(const void* regset);

static void set_pc_aarch64(void* regset, void* pc);
static void set_sp_aarch64(void* regset, void* sp);
static void set_fbp_aarch64(void* regset, void* fp);
static void set_ra_reg_aarch64(void* regset, void* ra);
static void setup_fbp_aarch64(void* regset, void* cfa);

static uint16_t reg_size_aarch64(uint16_t reg);
static void* reg_aarch64(void* regset, uint16_t reg);

/*
 * aarch64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regops_t regs_aarch64 = {
  .num_regs = AARCH64_NUM_REGS,
  .has_ra_reg = true,
  .regset_size = sizeof(struct regset_aarch64),
  .fbp_regnum = AARCH64_FBP_REG,
  .sp_regnum = SP,

  .regset_default = regset_default_aarch64,
  .regset_init = regset_init_aarch64,
  .regset_free = regset_free_aarch64,
  .regset_clone = regset_clone_aarch64,
  .regset_clear = regset_clear_aarch64,
  .regset_copyin = regset_copyin_aarch64,
  .regset_copy_arg_regs = regset_copy_arg_regs_aarch64,
  .regset_copy_ret_regs = regset_copy_arg_regs_aarch64,
  .regset_copyout = regset_copyout_aarch64,

  .pc = pc_aarch64,
  .sp = sp_aarch64,
  .fbp = fbp_aarch64,
  .ra_reg = ra_reg_aarch64,

  .set_pc = set_pc_aarch64,
  .set_sp = set_sp_aarch64,
  .set_fbp = set_fbp_aarch64,
  .set_ra_reg = set_ra_reg_aarch64,
  .setup_fbp = setup_fbp_aarch64,

  .reg_size = reg_size_aarch64,
  .reg = reg_aarch64,
};

///////////////////////////////////////////////////////////////////////////////
// aarch64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* regset_default_aarch64()
{
  struct regset_aarch64* new = calloc(1, sizeof(struct regset_aarch64));
  ASSERT(new, "could not allocate regset (aarch64)\n");
  return new;
}

static void* regset_init_aarch64(const void* regs)
{
  struct regset_aarch64* new = MALLOC(sizeof(struct regset_aarch64));
  ASSERT(new, "could not allocate regset (aarch64)\n");
  *new = *(struct regset_aarch64*)regs;
  return new;
}

static void regset_free_aarch64(void* regset)
{
  free(regset);
}

static void regset_clone_aarch64(const void* src, void* dest)
{
  const struct regset_aarch64* srcregs = (const struct regset_aarch64*)src;
  struct regset_aarch64* destregs = (struct regset_aarch64*)dest;
  *destregs = *srcregs;
}

static void regset_clear_aarch64(void *regs)
{
  memset(regs, 0, sizeof(struct regset_aarch64));
}

static void regset_copyin_aarch64(void* in, const void* out)
{
  struct regset_aarch64* cur = (struct regset_aarch64*)in;
  *cur = *(struct regset_aarch64*)out;
}

static void regset_copy_arg_regs_aarch64(void* in, const void* out)
{
  struct regset_aarch64* cur = (struct regset_aarch64*)in,
                       * input = (struct regset_aarch64*)out;

  cur->x[0] = input->x[0];
  cur->x[1] = input->x[1];
  cur->x[2] = input->x[2];
  cur->x[3] = input->x[3];
  cur->x[4] = input->x[4];
  cur->x[5] = input->x[5];
  cur->x[6] = input->x[6];
  cur->x[7] = input->x[7];

  cur->v[0] = input->v[0];
  cur->v[1] = input->v[1];
  cur->v[2] = input->v[2];
  cur->v[3] = input->v[3];
  cur->v[4] = input->v[4];
  cur->v[5] = input->v[5];
  cur->v[6] = input->v[6];
  cur->v[7] = input->v[7];
}

static void regset_copyout_aarch64(const void* in, void* out)
{
  const struct regset_aarch64* cur = (const struct regset_aarch64*)in;
  *(struct regset_aarch64*)out = *cur;
}

static void* pc_aarch64(const void* regset)
{
  const struct regset_aarch64* cur = (const struct regset_aarch64*)regset;
  return cur->pc;
}

static void* sp_aarch64(const void* regset)
{
  const struct regset_aarch64* cur = (const struct regset_aarch64*)regset;
  return cur->sp;
}

static void* fbp_aarch64(const void* regset)
{
  const struct regset_aarch64* cur = (const struct regset_aarch64*)regset;
  return (void*)cur->x[AARCH64_FBP_REG];
}

static void* ra_reg_aarch64(const void* regset)
{
  const struct regset_aarch64* cur = (const struct regset_aarch64*)regset;
  return (void*)cur->x[AARCH64_LINK_REG];
}

static void set_pc_aarch64(void* regset, void* pc)
{
  struct regset_aarch64* cur = (struct regset_aarch64*)regset;
  cur->pc = pc;
}

static void set_sp_aarch64(void* regset, void* sp)
{
  struct regset_aarch64* cur = (struct regset_aarch64*)regset;
  cur->sp = sp;
}

static void set_fbp_aarch64(void* regset, void* fp)
{
  struct regset_aarch64* cur = (struct regset_aarch64*)regset;
  cur->x[AARCH64_FBP_REG] = (uint64_t)fp;
}

static void set_ra_reg_aarch64(void* regset, void* ra)
{
  struct regset_aarch64* cur = (struct regset_aarch64*)regset;
  cur->x[AARCH64_LINK_REG] = (uint64_t)ra;
}

static void setup_fbp_aarch64(void* regset, void* cfa)
{
  ASSERT(cfa, "Null canonical frame address\n");
  struct regset_aarch64* cur = (struct regset_aarch64*)regset;
  cur->x[AARCH64_FBP_REG] = (uint64_t)cfa - 0x10;
}

static uint16_t reg_size_aarch64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers */
  case X0: case X1: case X2: case X3: case X4: case X5: case X6:
  case X7: case X8: case X9: case X10: case X11: case X12: case X13:
  case X14: case X15: case X16: case X17: case X18: case X19: case X20:
  case X21: case X22: case X23: case X24: case X25: case X26: case X27:
  case X28: case X29: case X30: case SP:
    return sizeof(uint64_t);

  /* Floating-point registers */
  case V0: case V1: case V2: case V3: case V4: case V5: case V6:
  case V7: case V8: case V9: case V10: case V11: case V12: case V13:
  case V14: case V15: case V16: case V17: case V18: case V19: case V20:
  case V21: case V22: case V23: case V24: case V25: case V26: case V27:
  case V28: case V29: case V30: case V31:
    return sizeof(unsigned __int128);

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %d (aarch64)\n", reg);
  return 0;
}

static void* reg_aarch64(void* regset, uint16_t reg)
{
  struct regset_aarch64* cur = (struct regset_aarch64*)regset;

  switch(reg)
  {
  case X0: return &cur->x[0];
  case X1: return &cur->x[1];
  case X2: return &cur->x[2];
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
  case SP: return &cur->sp;
  case V0: return &cur->v[0];
  case V1: return &cur->v[1];
  case V2: return &cur->v[2];
  case V3: return &cur->v[3];
  case V4: return &cur->v[4];
  case V5: return &cur->v[5];
  case V6: return &cur->v[6];
  case V7: return &cur->v[7];
  case V8: return &cur->v[8];
  case V9: return &cur->v[9];
  case V10: return &cur->v[10];
  case V11: return &cur->v[11];
  case V12: return &cur->v[12];
  case V13: return &cur->v[13];
  case V14: return &cur->v[14];
  case V15: return &cur->v[15];
  case V16: return &cur->v[16];
  case V17: return &cur->v[17];
  case V18: return &cur->v[18];
  case V19: return &cur->v[19];
  case V20: return &cur->v[20];
  case V21: return &cur->v[21];
  case V22: return &cur->v[22];
  case V23: return &cur->v[23];
  case V24: return &cur->v[24];
  case V25: return &cur->v[25];
  case V26: return &cur->v[26];
  case V27: return &cur->v[27];
  case V28: return &cur->v[28];
  case V29: return &cur->v[29];
  case V30: return &cur->v[30];
  case V31: return &cur->v[31];
  /*
   * TODO:
   *   33: ELR_mode
   */
  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (aarch64)\n", reg);
  return NULL;
}

