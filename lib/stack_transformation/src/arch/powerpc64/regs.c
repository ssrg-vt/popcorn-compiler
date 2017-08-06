/*
 * Implementation of powerpc64-specific value getters/setters and virtual stack
 * unwinding.
 *
 * Callee-saved register information is derived from the OPENPOWER ABI:
 * PowerPC_64bit_v2ABI_specification_rev1.4.pdf
 *
 * Author: Buse Yilmaz <busey@vt.edu>
 * Date: 05/11/2017
 */

#include "definitions.h"
#include "arch/powerpc64/regs.h"

///////////////////////////////////////////////////////////////////////////////
// File-local APIs & definitions
///////////////////////////////////////////////////////////////////////////////

#define POWERPC64_NUM_REGS 116
#define POWERPC64_FBP_REG 31
#define POWERPC64_LINK_REG lr
#define POWERPC64_CTR_REG ctr

static regset_t regset_default_powerpc64(void);
static regset_t regset_init_powerpc64(const void* regs);
static void regset_free_powerpc64(regset_t regset);
static void regset_clone_powerpc64(const_regset_t src, regset_t dest);
static void regset_copyin_powerpc64(regset_t regset, const void* regs);
static void regset_copyout_powerpc64(const_regset_t regset, void* regs);

static void* pc_powerpc64(const_regset_t regset);
static void* sp_powerpc64(const_regset_t regset);
static void* fbp_powerpc64(const_regset_t regset);
static void* ra_reg_powerpc64(const_regset_t regset);
static void* ctr_reg_powerpc64(const_regset_t regset);

static void set_pc_powerpc64(regset_t regset, void* pc);
static void set_sp_powerpc64(regset_t regset, void* sp);
static void set_fbp_powerpc64(regset_t regset, void* fp);
static void set_ra_reg_powerpc64(regset_t regset, void* ra);
static void set_ctr_reg_powerpc64(regset_t regset, void* ctr);
static void setup_fbp_powerpc64(regset_t regset, void* sp);

static uint16_t reg_size_powerpc64(uint16_t reg);
static void* reg_powerpc64(regset_t regset, uint16_t reg);

/*
 * Internal definition of powerpc64 object, contains powerpc64 registers in
 * addition to common fields & functions.
 */
typedef struct regset_obj_powerpc64
{
  struct regset_t common;
  struct regset_powerpc64 regs;
} regset_obj_powerpc64;

// TODO: Also do this with function pointers
/*
 * powerpc64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regops_t regs_powerpc64 = {
  .num_regs = POWERPC64_NUM_REGS,
  .has_ra_reg = true,
  .has_ctr_reg = true,
  .regset_size = sizeof(regset_obj_powerpc64),

  .regset_default = regset_default_powerpc64,
  .regset_init = regset_init_powerpc64,
  .regset_free = regset_free_powerpc64,
  .regset_clone = regset_clone_powerpc64,
  .regset_copyin = regset_copyin_powerpc64,
  .regset_copyout = regset_copyout_powerpc64,

  .pc = pc_powerpc64,
  .sp = sp_powerpc64,
  .fbp = fbp_powerpc64,
  .ra_reg = ra_reg_powerpc64,
  .ctr_reg = ctr_reg_powerpc64,

  .set_pc = set_pc_powerpc64,
  .set_sp = set_sp_powerpc64,
  .set_fbp = set_fbp_powerpc64,
  .set_ra_reg = set_ra_reg_powerpc64,
  .set_ctr_reg = set_ctr_reg_powerpc64,
  .setup_fbp = setup_fbp_powerpc64,

  .reg_size = reg_size_powerpc64,
  .reg = reg_powerpc64,
};

///////////////////////////////////////////////////////////////////////////////
// powerpc64 APIs
///////////////////////////////////////////////////////////////////////////////

static regset_t regset_default_powerpc64()
{
  regset_obj_powerpc64* new = calloc(1, sizeof(regset_obj_powerpc64));
  ASSERT(new, "could not allocate regset (powerpc64)\n");
  new->common.initialized = true;
  return (regset_t)new;
}

static regset_t regset_init_powerpc64(const void* regs)
{
  regset_obj_powerpc64* new = malloc(sizeof(regset_obj_powerpc64));
  ASSERT(new, "could not allocate regset (powerpc64)\n");
  new->common.initialized = true;
  new->regs = *(struct regset_powerpc64*)regs;
  return (regset_t)new;
}

static void regset_free_powerpc64(regset_t regset)
{
  free(regset);
}

static void regset_clone_powerpc64(const_regset_t src, regset_t dest)
{
  const regset_obj_powerpc64* srcregs = (const regset_obj_powerpc64*)src;
  regset_obj_powerpc64* destregs = (regset_obj_powerpc64*)dest;
  *destregs = *srcregs;
}

static void regset_copyin_powerpc64(regset_t regset, const void* regs)
{
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;
  cur->common.initialized = true;
  cur->regs = *(struct regset_powerpc64*)regs;
}

static void regset_copyout_powerpc64(const_regset_t regset, void* regs)
{
  const regset_obj_powerpc64* cur = (const regset_obj_powerpc64*)regset;
  *(struct regset_powerpc64*)regs = cur->regs;
}

static void* pc_powerpc64(const_regset_t regset)
{
  const regset_obj_powerpc64* cur = (const regset_obj_powerpc64*)regset;
  return cur->regs.pc;
}

static void* sp_powerpc64(const_regset_t regset)
{
  const regset_obj_powerpc64* cur = (const regset_obj_powerpc64*)regset;
  return cur->regs.sp;
}

static void* fbp_powerpc64(const_regset_t regset)
{
  const regset_obj_powerpc64* cur = (const regset_obj_powerpc64*)regset;
  return (void*)cur->regs.r[POWERPC64_FBP_REG];
}

static void* ra_reg_powerpc64(const_regset_t regset)
{
  const regset_obj_powerpc64* cur = (const regset_obj_powerpc64*)regset;
  return (void*)cur->regs.POWERPC64_LINK_REG;
}

static void* ctr_reg_powerpc64(const_regset_t regset)
{
  const regset_obj_powerpc64* cur = (const regset_obj_powerpc64*)regset;
  return (void*)cur->regs.POWERPC64_CTR_REG;
}

static void set_pc_powerpc64(regset_t regset, void* pc)
{
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;
  cur->regs.pc = pc;
}

static void set_sp_powerpc64(regset_t regset, void* sp)
{
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;
  cur->regs.sp = sp;
}

static void set_fbp_powerpc64(regset_t regset, void* fp)
{
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;
  cur->regs.r[POWERPC64_FBP_REG] = (uint64_t)fp;
}

static void set_ra_reg_powerpc64(regset_t regset, void* ra)
{
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;
  cur->regs.POWERPC64_LINK_REG = ra;
}

static void setup_fbp_powerpc64(regset_t regset, void* sp)
{
  ASSERT(sp, "Null stack pointer\n");
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;
 // cur->regs.r[POWERPC64_FBP_REG] = (uint64_t)cur->regs.sp;

 // For some reason stack-dump info sets FBP = CFA-16 ~ old(SP)-16 and offsets from there
 // TODO: This need to be calclulated dynamically since sometimes FBP is not even set
  //cur->regs.r[POWERPC64_FBP_REG] = (uint64_t)cfa-16;
  cur->regs.r[POWERPC64_FBP_REG] = (uint64_t)sp;
}

static void set_ctr_reg_powerpc64(regset_t regset, void* ctr)
{
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;
  cur->regs.POWERPC64_CTR_REG = ctr;
}

static uint16_t reg_size_powerpc64(uint16_t reg)
{
  switch(reg)
  {
  /* General-purpose registers */
  case R0: case R1: case R2: case R3: case R4: case R5: case R6:
  case R7: case R8: case R9: case R10: case R11: case R12: case R13:
  case R14: case R15: case R16: case R17: case R18: case R19: case R20:
  case R21: case R22: case R23: case R24: case R25: case R26: case R27:
  case R28: case R29: case R30: case R31: case LR: case CTR:
    return sizeof(uint64_t);

  /* Floating-point registers */
  case F0: case F1: case F2: case F3: case F4: case F5: case F6:
  case F7: case F8: case F9: case F10: case F11: case F12: case F13:
  case F14: case F15: case F16: case F17: case F18: case F19: case F20:
  case F21: case F22: case F23: case F24: case F25: case F26: case F27:
  case F28: case F29: case F30: case F31:
    return sizeof(unsigned __int128);

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %d (powerpc64)\n", reg);
  return 0;
}

static void* reg_powerpc64(regset_t regset, uint16_t reg)
{
  regset_obj_powerpc64* cur = (regset_obj_powerpc64*)regset;

  switch(reg)
  {
  case R0: return &cur->regs.r[0];
  case R1: return &cur->regs.r[1];
  case R2: return &cur->regs.r[2];
  case R3: return &cur->regs.r[3];
  case R4: return &cur->regs.r[4];
  case R5: return &cur->regs.r[5];
  case R6: return &cur->regs.r[6];
  case R7: return &cur->regs.r[7];
  case R8: return &cur->regs.r[8];
  case R9: return &cur->regs.r[9];
  case R10: return &cur->regs.r[10];
  case R11: return &cur->regs.r[11];
  case R12: return &cur->regs.r[12];
  case R13: return &cur->regs.r[13];
  case R14: return &cur->regs.r[14];
  case R15: return &cur->regs.r[15];
  case R16: return &cur->regs.r[16];
  case R17: return &cur->regs.r[17];
  case R18: return &cur->regs.r[18];
  case R19: return &cur->regs.r[19];
  case R20: return &cur->regs.r[20];
  case R21: return &cur->regs.r[21];
  case R22: return &cur->regs.r[22];
  case R23: return &cur->regs.r[23];
  case R24: return &cur->regs.r[24];
  case R25: return &cur->regs.r[25];
  case R26: return &cur->regs.r[26];
  case R27: return &cur->regs.r[27];
  case R28: return &cur->regs.r[28];
  case R29: return &cur->regs.r[29];
  case R30: return &cur->regs.r[30];
  case R31: return &cur->regs.r[31];
  case CTR: return &cur->regs.POWERPC64_CTR_REG;
  case LR: return &cur->regs.POWERPC64_LINK_REG;
//  case SP: return &cur->regs.sp;    // This is R1
  case F0: return &cur->regs.f[0];
  case F1: return &cur->regs.f[1];
  case F2: return &cur->regs.f[2];
  case F3: return &cur->regs.f[3];
  case F4: return &cur->regs.f[4];
  case F5: return &cur->regs.f[5];
  case F6: return &cur->regs.f[6];
  case F7: return &cur->regs.f[7];
  case F8: return &cur->regs.f[8];
  case F9: return &cur->regs.f[9];
  case F10: return &cur->regs.f[10];
  case F11: return &cur->regs.f[11];
  case F12: return &cur->regs.f[12];
  case F13: return &cur->regs.f[13];
  case F14: return &cur->regs.f[14];
  case F15: return &cur->regs.f[15];
  case F16: return &cur->regs.f[16];
  case F17: return &cur->regs.f[17];
  case F18: return &cur->regs.f[18];
  case F19: return &cur->regs.f[19];
  case F20: return &cur->regs.f[20];
  case F21: return &cur->regs.f[21];
  case F22: return &cur->regs.f[22];
  case F23: return &cur->regs.f[23];
  case F24: return &cur->regs.f[24];
  case F25: return &cur->regs.f[25];
  case F26: return &cur->regs.f[26];
  case F27: return &cur->regs.f[27];
  case F28: return &cur->regs.f[28];
  case F29: return &cur->regs.f[29];
  case F30: return &cur->regs.f[30];
  case F31: return &cur->regs.f[31];

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (powerpc64)\n", reg);
  return NULL;
}

