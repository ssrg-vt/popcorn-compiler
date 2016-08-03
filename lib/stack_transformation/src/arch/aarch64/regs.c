/*
 * Implementation of aarch64-specific value getters/setters and virtual stack
 * unwinding.
 *
 * Callee-saved register information is derived from the ARM ABI:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
 *
 * DWARF register number to name mappings are derived from the ARM DWARF
 * documentation:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0057b/IHI0057B_aadwarf64.pdf
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
#define AARCH64_FBP_REG 29
#define AARCH64_FBP_RULE AARCH64_FBP_REG
#define AARCH64_RA_RULE 30

static regset_t regset_default_aarch64(void);
static regset_t regset_init_aarch64(const void* regs);
static regset_t regset_clone_aarch64(const_regset_t regset);
static void regset_copyout_aarch64(const_regset_t regset, void* regs);
static void free_aarch64(regset_t regset);

static void* pc_aarch64(const_regset_t regset);
static void* sp_aarch64(const_regset_t regset);
static void* fbp_aarch64(const_regset_t regset);
static void* ra_reg_aarch64(const_regset_t regset);

static void set_pc_aarch64(regset_t regset, void* pc);
static void set_sp_aarch64(regset_t regset, void* sp);
static void set_fbp_aarch64(regset_t regset, void* fp);
static void set_ra_reg_aarch64(regset_t regset, void* ra);

static void* reg_aarch64(regset_t regset, dwarf_reg reg);

/*
 * aarch64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regset_t regs_aarch64 = {
  .num_regs = AARCH64_NUM_REGS,
  .ra_rule = AARCH64_RA_RULE,
  .fbp_rule = AARCH64_FBP_RULE,
  .has_ra_reg = true,

  .regset_default = regset_default_aarch64,
  .regset_init = regset_init_aarch64,
  .regset_clone = regset_clone_aarch64,
  .regset_copyout = regset_copyout_aarch64,
  .free = free_aarch64,

  .pc = pc_aarch64,
  .sp = sp_aarch64,
  .fbp = fbp_aarch64,
  .ra_reg = ra_reg_aarch64,

  .set_pc = set_pc_aarch64,
  .set_sp = set_sp_aarch64,
  .set_fbp = set_fbp_aarch64,
  .set_ra_reg = set_ra_reg_aarch64,

  .reg = reg_aarch64,
};

/*
 * Internal definition of aarch64 object, contains aarch64 registers in
 * addition to common fields & functions.
 */
typedef struct regset_obj_aarch64
{
  struct regset_t common;
  struct regset_aarch64 regs;
} regset_obj_aarch64;

///////////////////////////////////////////////////////////////////////////////
// aarch64 APIs
///////////////////////////////////////////////////////////////////////////////

static regset_t regset_default_aarch64()
{
  regset_obj_aarch64* new = calloc(1, sizeof(regset_obj_aarch64));
  ASSERT(new, "could not allocate regset (aarch64)\n");
  memcpy(&new->common, &regs_aarch64, sizeof(struct regset_t));
  return (regset_t)new;
}

static regset_t regset_init_aarch64(const void* regs)
{
  regset_obj_aarch64* new = malloc(sizeof(regset_obj_aarch64));
  ASSERT(new, "could not allocate regset (aarch64)\n");
  memcpy(&new->common, &regs_aarch64, sizeof(struct regset_t));
  new->regs = *(struct regset_aarch64*)regs;
  return (regset_t)new;
}

static regset_t regset_clone_aarch64(const_regset_t regset)
{
  const regset_obj_aarch64* cur = (const regset_obj_aarch64*)regset;
  regset_obj_aarch64* new = malloc(sizeof(regset_obj_aarch64));
  ASSERT(new, "could not allocate regset (aarch64)\n");
  memcpy(&new->common, &regs_aarch64, sizeof(struct regset_t));
  new->regs = cur->regs;
  return (regset_t)new;
}

static void regset_copyout_aarch64(const_regset_t regset, void* regs)
{
  const regset_obj_aarch64* cur = (const regset_obj_aarch64*)regset;
  memcpy(regs, &cur->regs, sizeof(struct regset_aarch64));
}

static void free_aarch64(regset_t regset)
{
  free(regset);
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
  return (void*)cur->regs.x[AARCH64_RA_RULE];
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
  cur->regs.x[AARCH64_RA_RULE] = (uint64_t)ra;
}

static void* reg_aarch64(regset_t regset, dwarf_reg reg)
{
  regset_obj_aarch64* cur = (regset_obj_aarch64*)regset;
  const char* op_name;

  switch(reg.reg)
  {
  case DW_OP_reg0: return &cur->regs.x[0];
  case DW_OP_reg1: return &cur->regs.x[1];
  case DW_OP_reg2: return &cur->regs.x[2];
  case DW_OP_reg3: return &cur->regs.x[3];
  case DW_OP_reg4: return &cur->regs.x[4];
  case DW_OP_reg5: return &cur->regs.x[5];
  case DW_OP_reg6: return &cur->regs.x[6];
  case DW_OP_reg7: return &cur->regs.x[7];
  case DW_OP_reg8: return &cur->regs.x[8];
  case DW_OP_reg9: return &cur->regs.x[9];
  case DW_OP_reg10: return &cur->regs.x[10];
  case DW_OP_reg11: return &cur->regs.x[11];
  case DW_OP_reg12: return &cur->regs.x[12];
  case DW_OP_reg13: return &cur->regs.x[13];
  case DW_OP_reg14: return &cur->regs.x[14];
  case DW_OP_reg15: return &cur->regs.x[15];
  case DW_OP_reg16: return &cur->regs.x[16];
  case DW_OP_reg17: return &cur->regs.x[17];
  case DW_OP_reg18: return &cur->regs.x[18];
  case DW_OP_reg19: return &cur->regs.x[19];
  case DW_OP_reg20: return &cur->regs.x[20];
  case DW_OP_reg21: return &cur->regs.x[21];
  case DW_OP_reg22: return &cur->regs.x[22];
  case DW_OP_reg23: return &cur->regs.x[23];
  case DW_OP_reg24: return &cur->regs.x[24];
  case DW_OP_reg25: return &cur->regs.x[25];
  case DW_OP_reg26: return &cur->regs.x[26];
  case DW_OP_reg27: return &cur->regs.x[27];
  case DW_OP_reg28: return &cur->regs.x[28];
  case DW_OP_reg29: return &cur->regs.x[29];
  case DW_OP_reg30: return &cur->regs.x[30];
  case DW_OP_reg31: return &cur->regs.sp;
  case DW_OP_regx:
    switch(reg.x) {
    case 64: return &cur->regs.v[0];
    case 65: return &cur->regs.v[1];
    case 66: return &cur->regs.v[2];
    case 67: return &cur->regs.v[3];
    case 68: return &cur->regs.v[4];
    case 69: return &cur->regs.v[5];
    case 70: return &cur->regs.v[6];
    case 71: return &cur->regs.v[7];
    case 72: return &cur->regs.v[8];
    case 73: return &cur->regs.v[9];
    case 74: return &cur->regs.v[10];
    case 75: return &cur->regs.v[11];
    case 76: return &cur->regs.v[12];
    case 77: return &cur->regs.v[13];
    case 78: return &cur->regs.v[14];
    case 79: return &cur->regs.v[15];
    case 80: return &cur->regs.v[16];
    case 81: return &cur->regs.v[17];
    case 82: return &cur->regs.v[18];
    case 83: return &cur->regs.v[19];
    case 84: return &cur->regs.v[20];
    case 85: return &cur->regs.v[21];
    case 86: return &cur->regs.v[22];
    case 87: return &cur->regs.v[23];
    case 88: return &cur->regs.v[24];
    case 89: return &cur->regs.v[25];
    case 90: return &cur->regs.v[26];
    case 91: return &cur->regs.v[27];
    case 92: return &cur->regs.v[28];
    case 93: return &cur->regs.v[29];
    case 94: return &cur->regs.v[30];
    case 95: return &cur->regs.v[31];
    /*
     * TODO:
     *   33: ELR_mode
     */
    default: break;
    }
    break;
  default: break;
  }

  dwarf_get_OP_name(reg.reg, &op_name);
  ASSERT(false, "unknown/invalid register '%s' (aarch64)\n", op_name);
  return NULL;
}

