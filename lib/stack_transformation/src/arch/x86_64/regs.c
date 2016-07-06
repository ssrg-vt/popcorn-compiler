/*
 * Implementation of x86-64-specific value getters/setters and virtual stack
 * unwinding.
 *
 * DWARF register number to name mappings are derived from the x86-64 ABI
 * http://www.x86-64.org/documentation/abi.pdf
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
#define X86_64_FBP_RULE 6
#define X86_64_RA_RULE 16

static regset_t regset_default_x86_64(void);
static regset_t regset_init_x86_64(const void* regs);
static regset_t regset_clone_x86_64(const_regset_t regset);
static void regset_copyout_x86_64(const_regset_t regset, void* regs);
static void free_x86_64(regset_t regset);

static void* pc_x86_64(const_regset_t regset);
static void* sp_x86_64(const_regset_t regset);
static void* fbp_x86_64(const_regset_t regset);
static uint64_t reg_x86_64(const_regset_t regset, dwarf_reg reg);
static uint64_t ra_reg_x86_64(const_regset_t regset);
static void* ext_reg_x86_64(regset_t regset, dwarf_reg reg);

static void set_pc_x86_64(regset_t regset, void* pc);
static void set_sp_x86_64(regset_t regset, void* sp);
static void set_fbp_x86_64(regset_t regset, void* fbp);
static void set_reg_x86_64(regset_t regset, dwarf_reg reg, uint64_t val);
static void set_ra_reg_x86_64(regset_t regset, uint64_t val);

/*
 * x86-64 register operations (externally visible), used to construct new
 * objects.
 */
const struct regset_t regs_x86_64 = {
  .num_regs = X86_64_NUM_REGS,
  .fbp_rule = X86_64_FBP_RULE,
  .ra_rule = X86_64_RA_RULE,
  .has_ra_reg = false,

  .regset_default = regset_default_x86_64,
  .regset_init = regset_init_x86_64,
  .regset_clone = regset_clone_x86_64,
  .regset_copyout = regset_copyout_x86_64,
  .free = free_x86_64,

  .pc = pc_x86_64,
  .sp = sp_x86_64,
  .fbp = fbp_x86_64,
  .reg = reg_x86_64,
  .ra_reg = ra_reg_x86_64,
  .ext_reg = ext_reg_x86_64,

  .set_pc = set_pc_x86_64,
  .set_sp = set_sp_x86_64,
  .set_fbp = set_fbp_x86_64,
  .set_reg = set_reg_x86_64,
  .set_ra_reg = set_ra_reg_x86_64,
};

/*
 * Internal definition of x86-64 object, contains x86-64 registers in addition
 * to common fields & functions.
 */
typedef struct regset_obj_x86_64
{
  struct regset_t common;
  struct regset_x86_64 regs;
} regset_obj_x86_64;

///////////////////////////////////////////////////////////////////////////////
// x86-64 APIs
///////////////////////////////////////////////////////////////////////////////

static regset_t regset_default_x86_64()
{
  regset_obj_x86_64* new = calloc(1, sizeof(regset_obj_x86_64));
  ASSERT(new, "could not allocate regset (x86-64)\n");
  memcpy(&new->common, &regs_x86_64, sizeof(struct regset_t));
  return (regset_t)new;
}

static regset_t regset_init_x86_64(const void* regs)
{
  regset_obj_x86_64* new = malloc(sizeof(regset_obj_x86_64));
  ASSERT(new, "could not allocate regset (x86-64)\n");
  memcpy(&new->common, &regs_x86_64, sizeof(struct regset_t));
  new->regs = *(struct regset_x86_64*)regs;
  return (regset_t)new;
}

static regset_t regset_clone_x86_64(const_regset_t regset)
{
  const regset_obj_x86_64* cur = (const regset_obj_x86_64*)regset;
  regset_obj_x86_64* new = malloc(sizeof(regset_obj_x86_64));
  ASSERT(new, "could not allocate regset (x86-64)\n");
  memcpy(&new->common, &regs_x86_64, sizeof(struct regset_t));
  new->regs = cur->regs;
  return (regset_t)new;
}

static void regset_copyout_x86_64(const_regset_t regset, void* regs)
{
  const regset_obj_x86_64* cur = (const regset_obj_x86_64*)regset;
  memcpy(regs, &cur->regs, sizeof(struct regset_x86_64));
}

static void free_x86_64(regset_t regset)
{
  free(regset);
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

static uint64_t reg_x86_64(const_regset_t regset, dwarf_reg reg)
{
  const regset_obj_x86_64* cur = (const regset_obj_x86_64*)regset;
  const char* op_name;
  switch(reg.reg)
  {
  case DW_OP_reg0: return cur->regs.rax;
  case DW_OP_reg1: return cur->regs.rdx;
  case DW_OP_reg2: return cur->regs.rcx;
  case DW_OP_reg3: return cur->regs.rbx;
  case DW_OP_reg4: return cur->regs.rsi;
  case DW_OP_reg5: return cur->regs.rdi;
  case DW_OP_reg6: return cur->regs.rbp;
  case DW_OP_reg7: return (uint64_t)cur->regs.rsp;
  case DW_OP_reg8: return cur->regs.r8;
  case DW_OP_reg9: return cur->regs.r9;
  case DW_OP_reg10: return cur->regs.r10;
  case DW_OP_reg11: return cur->regs.r11;
  case DW_OP_reg12: return cur->regs.r12;
  case DW_OP_reg13: return cur->regs.r13;
  case DW_OP_reg14: return cur->regs.r14;
  case DW_OP_reg15: return cur->regs.r15;
  case DW_OP_reg16: return (uint64_t)cur->regs.rip;

  case DW_OP_reg17: case DW_OP_reg18: case DW_OP_reg19: case DW_OP_reg20:
  case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23: case DW_OP_reg24:
  case DW_OP_reg25: case DW_OP_reg26: case DW_OP_reg27: case DW_OP_reg28:
  case DW_OP_reg29: case DW_OP_reg30: case DW_OP_reg31:
    dwarf_get_OP_name(reg.reg, &op_name);
    ASSERT(false, "attempting to get value from extended register %s (x86-64)\n", op_name);
    return 0;

  case DW_OP_regx:
    switch(reg.x) {
    case 32:
      ASSERT(false, "attempting to get value from extended register 32 (x86-64)");
      return 0;
    /*
     * TODO:
     *   33-40: st[0] - st[7]
     *   41-48: st[0] - st[7] (MMX registers mm[0] - mm[7])
     *   49:  rflags
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
    default:
      ASSERT(false, "regx (%llu) not yet supported (x86-64)\n", reg.x);
      return 0;
    }
  default:
    dwarf_get_OP_name(reg.reg, &op_name);
    ASSERT(false, "unknown/invalid register operation '%s' (x86-64)\n", op_name);
    return 0;
  }
}

static void* ext_reg_x86_64(regset_t regset, dwarf_reg reg)
{
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;
  const char* op_name;
  switch(reg.reg)
  {
  case DW_OP_reg17: return &cur->regs.xmm[0];
  case DW_OP_reg18: return &cur->regs.xmm[1];
  case DW_OP_reg19: return &cur->regs.xmm[2];
  case DW_OP_reg20: return &cur->regs.xmm[3];
  case DW_OP_reg21: return &cur->regs.xmm[4];
  case DW_OP_reg22: return &cur->regs.xmm[5];
  case DW_OP_reg23: return &cur->regs.xmm[6];
  case DW_OP_reg24: return &cur->regs.xmm[7];
  case DW_OP_reg25: return &cur->regs.xmm[8];
  case DW_OP_reg26: return &cur->regs.xmm[9];
  case DW_OP_reg27: return &cur->regs.xmm[10];
  case DW_OP_reg28: return &cur->regs.xmm[11];
  case DW_OP_reg29: return &cur->regs.xmm[12];
  case DW_OP_reg30: return &cur->regs.xmm[13];
  case DW_OP_reg31: return &cur->regs.xmm[14];
  case DW_OP_regx:
    switch(reg.x) {
    case 32: return &cur->regs.xmm[15];
    default: break; // Fall through to error-checking below
    };
  default:
    dwarf_get_OP_name(reg.reg, &op_name);
    ASSERT(false, "unknown/invalid register operation '%s' (x86-64)\n", op_name);
    return 0;
  }
}

static uint64_t ra_reg_x86_64(const_regset_t regset)
{
  // N/a for x86-64, return address is always stored on the stack
  return DW_FRAME_UNDEFINED_VAL;
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

static void set_reg_x86_64(regset_t regset, dwarf_reg reg, uint64_t val)
{
  regset_obj_x86_64* cur = (regset_obj_x86_64*)regset;
  const char* op_name;
  switch(reg.reg)
  {
  case DW_OP_reg0: cur->regs.rax = val; break;
  case DW_OP_reg1: cur->regs.rdx = val; break;
  case DW_OP_reg2: cur->regs.rcx = val; break;
  case DW_OP_reg3: cur->regs.rbx = val; break;
  case DW_OP_reg4: cur->regs.rsi = val; break;
  case DW_OP_reg5: cur->regs.rdi = val; break;
  case DW_OP_reg6: cur->regs.rbp = val; break;
  case DW_OP_reg7: cur->regs.rsp = val; break;
  case DW_OP_reg8: cur->regs.r8 = val; break;
  case DW_OP_reg9: cur->regs.r9 = val; break;
  case DW_OP_reg10: cur->regs.r10 = val; break;
  case DW_OP_reg11: cur->regs.r11 = val; break;
  case DW_OP_reg12: cur->regs.r12 = val; break;
  case DW_OP_reg13: cur->regs.r13 = val; break;
  case DW_OP_reg14: cur->regs.r14 = val; break;
  case DW_OP_reg15: cur->regs.r15 = val; break;
  case DW_OP_reg16: cur->regs.rip = (void*)val; break;
    /*
     * TODO:
     *   DW_OP_reg17 - DW_OP_reg31: xmm[0] - xmm[15]
     */
  case DW_OP_regx:
    /*
     * TODO:
     *   33-40: st[0] - st[7]
     *   41-48: st[0] - st[7] (MMX registers mm[0] - mm[7])
     *   49:  rflags
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
    ASSERT(false, "regx not yet supported (x86-64)\n");
    break;
  default:
    dwarf_get_OP_name(reg.reg, &op_name);
    ASSERT(false, "unknown/invalid register operation '%s' (x86-64)\n", op_name);
    break;
  }
}

static void set_ra_reg_x86_64(regset_t regset, uint64_t val)
{
  // Nothing to do for x86-64, return address is always stored on the stack
  ASSERT(false, "no return-address register for x86-64\n");
}

