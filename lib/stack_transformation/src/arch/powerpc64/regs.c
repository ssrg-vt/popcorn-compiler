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

#define POWERPC64_SP_REG R1
#define POWERPC64_FBP_REG R31

static void* regset_default_powerpc64(void);
static void* regset_init_powerpc64(const void* regs);
static void regset_free_powerpc64(void* regset);
static void regset_clone_powerpc64(const void* src, void* dest);
static void regset_copyin_powerpc64(void* regset, const void* regs);
static void regset_copyout_powerpc64(const void* regset, void* regs);

static void* pc_powerpc64(const void* regset);
static void* sp_powerpc64(const void* regset);
static void* fbp_powerpc64(const void* regset);
static void* ra_reg_powerpc64(const void* regset);

static void set_pc_powerpc64(void* regset, void* pc);
static void set_sp_powerpc64(void* regset, void* sp);
static void set_fbp_powerpc64(void* regset, void* fp);
static void set_ra_reg_powerpc64(void* regset, void* ra);
static void setup_fbp_powerpc64(void* regset, void* cfa);

static uint16_t reg_size_powerpc64(uint16_t reg);
static void* reg_powerpc64(void* regset, uint16_t reg);

/*
 * powerpc64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regops_t regs_powerpc64 = {
  .num_regs = POWERPC64_NUM_REGS,
  .has_ra_reg = true,
  .regset_size = sizeof(struct regset_powerpc64),
  .fbp_regnum = POWERPC64_FBP_REG,

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

  .set_pc = set_pc_powerpc64,
  .set_sp = set_sp_powerpc64,
  .set_fbp = set_fbp_powerpc64,
  .set_ra_reg = set_ra_reg_powerpc64,
  .setup_fbp = setup_fbp_powerpc64,

  .reg_size = reg_size_powerpc64,
  .reg = reg_powerpc64,
};

///////////////////////////////////////////////////////////////////////////////
// powerpc64 APIs
///////////////////////////////////////////////////////////////////////////////

static void* regset_default_powerpc64()
{
  struct regset_powerpc64* new = calloc(1, sizeof(struct regset_powerpc64));
  ASSERT(new, "could not allocate regset (powerpc64)\n");
  return new;
}

static void* regset_init_powerpc64(const void* regs)
{
  struct regset_powerpc64* new = MALLOC(sizeof(struct regset_powerpc64));
  ASSERT(new, "could not allocate regset (powerpc64)\n");
  *new = *(struct regset_powerpc64*)regs;
  return new;
}

static void regset_free_powerpc64(void* regset)
{
  free(regset);
}

static void regset_clone_powerpc64(const void* src, void* dest)
{
  const struct regset_powerpc64* srcregs = (const struct regset_powerpc64*)src;
  struct regset_powerpc64* destregs = (struct regset_powerpc64*)dest;
  *destregs = *srcregs;
}

static void regset_copyin_powerpc64(void* in, const void* out)
{
  struct regset_powerpc64* cur = (struct regset_powerpc64*)in;
  *cur = *(struct regset_powerpc64*)out;
}

static void regset_copyout_powerpc64(const void* in, void* out)
{
  const struct regset_powerpc64* cur = (const struct regset_powerpc64*)in;
  *(struct regset_powerpc64*)out = *cur;
}

static void* pc_powerpc64(const void* regset)
{
  const struct regset_powerpc64* cur = (const struct regset_powerpc64*)regset;
  return cur->pc;
}

static void* sp_powerpc64(const void* regset)
{
  const struct regset_powerpc64* cur = (const struct regset_powerpc64*)regset;
  return (void*)cur->r[POWERPC64_SP_REG];
}

static void* fbp_powerpc64(const void* regset)
{
  const struct regset_powerpc64* cur = (const struct regset_powerpc64*)regset;
  return (void*)cur->r[POWERPC64_FBP_REG];
}

static void* ra_reg_powerpc64(const void* regset)
{
  const struct regset_powerpc64* cur = (const struct regset_powerpc64*)regset;
  return (void*)cur->lr;
}

static void set_pc_powerpc64(void* regset, void* pc)
{
  struct regset_powerpc64* cur = (struct regset_powerpc64*)regset;
  cur->pc = pc;
}

static void set_sp_powerpc64(void* regset, void* sp)
{
  struct regset_powerpc64* cur = (struct regset_powerpc64*)regset;
  cur->r[POWERPC64_SP_REG] = (uint64_t)sp;
}

static void set_fbp_powerpc64(void* regset, void* fp)
{
  struct regset_powerpc64* cur = (struct regset_powerpc64*)regset;
  cur->r[POWERPC64_FBP_REG] = (uint64_t)fp;
}

static void set_ra_reg_powerpc64(void* regset, void* ra)
{
  struct regset_powerpc64* cur = (struct regset_powerpc64*)regset;
  cur->lr = ra;
}

static void setup_fbp_powerpc64(void* regset,
                                void* __attribute__((unused)) cfa)
{
  struct regset_powerpc64* cur = (struct regset_powerpc64*)regset;
  ASSERT(cur->r[POWERPC64_SP_REG], "Null stack pointer\n");
  cur->r[POWERPC64_FBP_REG] = (uint64_t)cur->r[POWERPC64_SP_REG];
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

static void* reg_powerpc64(void* regset, uint16_t reg)
{
  struct regset_powerpc64* cur = (struct regset_powerpc64*)regset;

  switch(reg)
  {
  case R0: return &cur->r[0];
  case R1: return &cur->r[1];
  case R2: return &cur->r[2];
  case R3: return &cur->r[3];
  case R4: return &cur->r[4];
  case R5: return &cur->r[5];
  case R6: return &cur->r[6];
  case R7: return &cur->r[7];
  case R8: return &cur->r[8];
  case R9: return &cur->r[9];
  case R10: return &cur->r[10];
  case R11: return &cur->r[11];
  case R12: return &cur->r[12];
  case R13: return &cur->r[13];
  case R14: return &cur->r[14];
  case R15: return &cur->r[15];
  case R16: return &cur->r[16];
  case R17: return &cur->r[17];
  case R18: return &cur->r[18];
  case R19: return &cur->r[19];
  case R20: return &cur->r[20];
  case R21: return &cur->r[21];
  case R22: return &cur->r[22];
  case R23: return &cur->r[23];
  case R24: return &cur->r[24];
  case R25: return &cur->r[25];
  case R26: return &cur->r[26];
  case R27: return &cur->r[27];
  case R28: return &cur->r[28];
  case R29: return &cur->r[29];
  case R30: return &cur->r[30];
  case R31: return &cur->r[31];
  case CTR: return &cur->ctr;
  case LR: return &cur->lr;
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

  default: break;
  }

  ST_ERR(1, "unknown/invalid register %u (powerpc64)\n", reg);
  return NULL;
}

