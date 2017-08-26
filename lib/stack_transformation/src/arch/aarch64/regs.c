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

#define AARCH64_NUM_REGS 128
#define AARCH64_FBP_REG X29
#define AARCH64_LINK_REG X30

static regset_t regset_default_aarch64(void);
static regset_t regset_init_aarch64(const void* regs);
static void regset_free_aarch64(regset_t regset);
static void regset_clone_aarch64(const_regset_t src, regset_t dest);
static void regset_copyin_aarch64(regset_t regset, const void* regs);
static void regset_copyout_aarch64(const_regset_t regset, void* regs);

static void* pc_aarch64(const_regset_t regset);
static void* sp_aarch64(const_regset_t regset);
static void* fbp_aarch64(const_regset_t regset);
static void* ra_reg_aarch64(const_regset_t regset);

static void set_pc_aarch64(regset_t regset, void* pc);
static void set_sp_aarch64(regset_t regset, void* sp);
static void set_fbp_aarch64(regset_t regset, void* fp);
static void set_ra_reg_aarch64(regset_t regset, void* ra);
static void setup_fbp_aarch64(regset_t regset, void* cfa);

static uint16_t reg_size_aarch64(uint16_t reg);
static void* reg_aarch64(regset_t regset, uint16_t reg);

/*
 * Internal definition of aarch64 object, contains aarch64 registers in
 * addition to common fields & functions.
 */
typedef struct regset_obj_aarch64
{
  struct regset_t common;
  struct regset_aarch64 regs;
} regset_obj_aarch64;

/*
 * aarch64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regops_t regs_aarch64 = {
  .num_regs = AARCH64_NUM_REGS,
  .has_ra_reg = true,
  .regset_size = sizeof(regset_obj_aarch64),
  .fbp_regnum = AARCH64_FBP_REG,

  .regset_default = regset_default_aarch64,
  .regset_init = regset_init_aarch64,
  .regset_free = regset_free_aarch64,
  .regset_clone = regset_clone_aarch64,
  .regset_copyin = regset_copyin_aarch64,
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

static regset_t regset_default_aarch64()
{
  regset_obj_aarch64* new = calloc(1, sizeof(regset_obj_aarch64));
  ASSERT(new, "could not allocate regset (aarch64)\n");
  new->common.initialized = true;
  return (regset_t)new;
}

static regset_t regset_init_aarch64(const void* regs)
{
  regset_obj_aarch64* new = malloc(sizeof(regset_obj_aarch64));
  ASSERT(new, "could not allocate regset (aarch64)\n");
  new->common.initialized = true;
  new->regs = *(struct regset_aarch64*)regs;
  return (regset_t)new;
}

static void regset_free_aarch64(regset_t regset)
{
  free(regset);
}

static void regset_clone_aarch64(const_regset_t src, regset_t dest)
{
  const regset_obj_aarch64* srcregs = (const regset_obj_aarch64*)src;
  regset_obj_aarch64* destregs = (regset_obj_aarch64*)dest;
  *destregs = *srcregs;
}

static void regset_copyin_aarch64(regset_t regset, const void* regs)
{
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;
  cur->common.initialized = true;
  cur->regs = *(struct regset_aarch64*)regs;
}

static void regset_copyout_aarch64(const_regset_t regset, void* regs)
{
  const regset_obj_aarch64* cur = (const regset_obj_aarch64*)regset;
  *(struct regset_aarch64*)regs = cur->regs;
}

static void* pc_aarch64(const_regset_t regset)
{
  const regset_obj_aarch64* cur = (const regset_obj_aarch64*)regset;
  return cur->regs.pc;
}

static void* sp_aarch64(const_regset_t regset)
{
  const regset_obj_aarch64* cur = (const regset_obj_aarch64*)regset;
  return cur->regs.sp;
}

static void* fbp_aarch64(const_regset_t regset)
{
  const regset_obj_aarch64* cur = (const regset_obj_aarch64*)regset;
  return (void*)cur->regs.x[AARCH64_FBP_REG];
}

static void* ra_reg_aarch64(const_regset_t regset)
{
  const regset_obj_aarch64* cur = (const regset_obj_aarch64*)regset;
  return (void*)cur->regs.x[AARCH64_LINK_REG];
}

static void set_pc_aarch64(regset_t regset, void* pc)
{
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;
  cur->regs.pc = pc;
}

static void set_sp_aarch64(regset_t regset, void* sp)
{
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;
  cur->regs.sp = sp;
}

static void set_fbp_aarch64(regset_t regset, void* fp)
{
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;
  cur->regs.x[AARCH64_FBP_REG] = (uint64_t)fp;
}

static void set_ra_reg_aarch64(regset_t regset, void* ra)
{
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;
  cur->regs.x[AARCH64_LINK_REG] = (uint64_t)ra;
}

static void setup_fbp_aarch64(regset_t regset, void* cfa)
{
  ASSERT(cfa, "Null canonical frame address\n");
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;
  cur->regs.x[AARCH64_FBP_REG] = (uint64_t)cfa - 0x10;
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

static void* reg_aarch64(regset_t regset, uint16_t reg)
{
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;

  switch(reg)
  {
  case X0: return &cur->regs.x[0];
  case X1: return &cur->regs.x[1];
  case X2: return &cur->regs.x[2];
  case X3: return &cur->regs.x[3];
  case X4: return &cur->regs.x[4];
  case X5: return &cur->regs.x[5];
  case X6: return &cur->regs.x[6];
  case X7: return &cur->regs.x[7];
  case X8: return &cur->regs.x[8];
  case X9: return &cur->regs.x[9];
  case X10: return &cur->regs.x[10];
  case X11: return &cur->regs.x[11];
  case X12: return &cur->regs.x[12];
  case X13: return &cur->regs.x[13];
  case X14: return &cur->regs.x[14];
  case X15: return &cur->regs.x[15];
  case X16: return &cur->regs.x[16];
  case X17: return &cur->regs.x[17];
  case X18: return &cur->regs.x[18];
  case X19: return &cur->regs.x[19];
  case X20: return &cur->regs.x[20];
  case X21: return &cur->regs.x[21];
  case X22: return &cur->regs.x[22];
  case X23: return &cur->regs.x[23];
  case X24: return &cur->regs.x[24];
  case X25: return &cur->regs.x[25];
  case X26: return &cur->regs.x[26];
  case X27: return &cur->regs.x[27];
  case X28: return &cur->regs.x[28];
  case X29: return &cur->regs.x[29];
  case X30: return &cur->regs.x[30];
  case SP: return &cur->regs.sp;
  case V0: return &cur->regs.v[0];
  case V1: return &cur->regs.v[1];
  case V2: return &cur->regs.v[2];
  case V3: return &cur->regs.v[3];
  case V4: return &cur->regs.v[4];
  case V5: return &cur->regs.v[5];
  case V6: return &cur->regs.v[6];
  case V7: return &cur->regs.v[7];
  case V8: return &cur->regs.v[8];
  case V9: return &cur->regs.v[9];
  case V10: return &cur->regs.v[10];
  case V11: return &cur->regs.v[11];
  case V12: return &cur->regs.v[12];
  case V13: return &cur->regs.v[13];
  case V14: return &cur->regs.v[14];
  case V15: return &cur->regs.v[15];
  case V16: return &cur->regs.v[16];
  case V17: return &cur->regs.v[17];
  case V18: return &cur->regs.v[18];
  case V19: return &cur->regs.v[19];
  case V20: return &cur->regs.v[20];
  case V21: return &cur->regs.v[21];
  case V22: return &cur->regs.v[22];
  case V23: return &cur->regs.v[23];
  case V24: return &cur->regs.v[24];
  case V25: return &cur->regs.v[25];
  case V26: return &cur->regs.v[26];
  case V27: return &cur->regs.v[27];
  case V28: return &cur->regs.v[28];
  case V29: return &cur->regs.v[29];
  case V30: return &cur->regs.v[30];
  case V31: return &cur->regs.v[31];
  /*
   * TODO:
   *   33: ELR_mode
   */
  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (aarch64)\n", reg);
  return NULL;
}

