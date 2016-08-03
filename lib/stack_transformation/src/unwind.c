/*
 * Implements the logic necessary to unwind/un-unwind stack frame activations.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 11/18/2015
 */

#include "unwind.h"
#include "func.h"
#include "query.h"

///////////////////////////////////////////////////////////////////////////////
// Stack unwinding
///////////////////////////////////////////////////////////////////////////////

/*
 * General frame-unwinding initialization.
 */
void init_unwinding(st_handle handle)
{
  dwarf_set_frame_rule_table_size(handle->dbg, handle->regops->num_regs);
  dwarf_set_frame_rule_initial_value(handle->dbg, DW_FRAME_UNDEFINED_VAL);
  dwarf_set_frame_cfa_value(handle->dbg, DW_FRAME_CFA_COL3);
}

/*
 * Read frame unwinding rules for a stack frame.
 */
void read_unwind_rules(rewrite_context ctx)
{
  size_t num_regs;
  void* pc;
  value cfa_loc;
  Dwarf_Addr row_pc;
  Dwarf_Cie cie;
  Dwarf_Fde fde;
  Dwarf_Error err;

  TIMER_FG_START(read_unwind_rules);

  /* Allocate frame unwinding rules & callee-saved registers from pools. */
  num_regs = ACT(ctx).regs->num_regs;
  ACT(ctx).rules.rt3_reg_table_size = num_regs;
  ACT(ctx).rules.rt3_rules = &ctx->regtable_pool[ctx->act * num_regs];
  ACT(ctx).callee_saved.size = num_regs;
  ACT(ctx).callee_saved.bits = &ctx->callee_saved_pool[ctx->act * bitmap_size(num_regs)];

  /* Read the rules. */
  pc = ACT(ctx).regs->pc(ACT(ctx).regs);
  get_fde_cie(ctx->handle, pc, &fde, &cie);
  DWARF_OK(dwarf_get_fde_info_for_all_regs3(fde,
                                            (Dwarf_Addr)pc,
                                            &ACT(ctx).rules,
                                            &row_pc,
                                            &err),
           "dwarf_get_fde_info_for_all_regs3");
  cfa_loc = get_stored_loc(ctx, &ACT(ctx), &ACT(ctx).rules.rt3_cfa_rule, true);
  ASSERT(cfa_loc.is_valid, "could not calculate CFA\n");
  ASSERT(cfa_loc.type == CONSTANT, "unhandled CFA location type\n");
  ACT(ctx).cfa = cfa_loc.addr;

  ST_INFO("Read frame unwinding info (CFA = %p)\n", ACT(ctx).cfa);

  TIMER_FG_STOP(read_unwind_rules);
}

/*
 * Return function info if the current frame corresponds to the first function
 * called by the thread, or NULL otherwise.
 */
func_info first_frame(st_handle handle, void* pc)
{
  if(is_func(handle->start_main, pc)) return handle->start_main;
  else if(is_func(handle->start_thread, pc)) return handle->start_thread;
  return NULL;
}

/*
 * Pop a frame from CTX's stack.
 */
void pop_frame(rewrite_context ctx)
{
  size_t i, num_callee_saved;
  uint64_t cur_idx;
  const uint16_t* saved_idx;
  const uint16_t* saved_size;
  void* src_addr;
  value saved_val;
  dwarf_reg reg;

  TIMER_FG_START(pop_frame);

  ST_INFO("Popping frame (CFA = %p)\n", ACT(ctx).cfa);

  /* Initialize next activation's regset */
  NEXT_ACT(ctx).regs = ACT(ctx).regs->regset_clone(ACT(ctx).regs);

  /* Apply rules to unwind to previous frame */
  num_callee_saved = ctx->handle->props->num_callee_saved;
  saved_idx = ctx->handle->props->callee_saved;
  saved_size = ctx->handle->props->callee_save_size;
  for(i = 0; i < num_callee_saved; i++)
  {
    cur_idx = saved_idx[i];
    saved_val = get_stored_loc(ctx,
                               &ACT(ctx),
                               &ACT(ctx).rules.rt3_rules[cur_idx],
                               false);
    if(saved_val.is_valid)
    {
      ST_INFO("Callee-saved: %lu\n", cur_idx);

      switch(saved_val.type)
      {
      case ADDRESS: src_addr = saved_val.addr; break;
      case REGISTER: src_addr = ACT(ctx).regs->reg(ACT(ctx).regs, saved_val.reg); break;
      case CONSTANT: src_addr = &saved_val.cnst; break;
      default: ASSERT(false, "invalid value\n"); src_addr = NULL; break;
      }

      reg = OP_REG(cur_idx);
      memcpy(NEXT_ACT(ctx).regs->reg(NEXT_ACT(ctx).regs, reg),
             src_addr,
             saved_size[i]);
      bitmap_set(ACT(ctx).callee_saved, cur_idx);
    }
  }

  /*
   * Some ABIs map the return address to the PC register (e.g. x86-64) and some
   * map it to another register (e.g. RA is mapped to x30 for AArch64).  Handle
   * the latter case by explicitly setting the new PC.
   */
  if(NEXT_ACT(ctx).regs->has_ra_reg)
    NEXT_ACT(ctx).regs->set_pc(NEXT_ACT(ctx).regs,
                               NEXT_ACT(ctx).regs->ra_reg(NEXT_ACT(ctx).regs));
  ST_INFO("Return address: %p\n", NEXT_ACT(ctx).regs->pc(NEXT_ACT(ctx).regs));

  /*
   * Set the stack pointer in the previous frame which is by definition the CFA
   * of the current frame.
   */
  NEXT_ACT(ctx).regs->set_sp(NEXT_ACT(ctx).regs, ACT(ctx).cfa);

  /* Advance to next frame. */
  ctx->act++;
  ASSERT(ctx->act < MAX_FRAMES, "too many frames on stack\n");

  TIMER_FG_STOP(pop_frame);
}

/*
 * Process unwinding rule to get the saved location for the register (or the
 * constant value).
 */
value get_stored_loc(rewrite_context ctx,
                     activation* act,
                     Dwarf_Regtable_Entry3* rule,
                     bool is_cfa)
{
  value loc = {
    .is_valid = true,
    .act = ctx->act,
    .addr = NULL
  };
  dwarf_reg reg;
  Dwarf_Locdesc* loc_desc;
  Dwarf_Signed loc_len;
  Dwarf_Error err;

  ASSERT(act && rule, "invalid arguments to get_stored_loc()\n");

  if(rule->dw_regnum != DW_FRAME_UNDEFINED_VAL &&
     rule->dw_regnum != DW_FRAME_SAME_VAL)
  {
    switch(rule->dw_value_type)
    {
    case DW_EXPR_OFFSET:
      if(rule->dw_offset_relevant)
      {
        if(is_cfa)
        {
          // Note: we assume that this is a 64-bit register
          ASSERT(rule->dw_regnum != DW_FRAME_CFA_COL3,
                 "invalid register for CFA calculation\n");
          reg = OP_REG(rule->dw_regnum);
          ASSERT(ctx->handle->props->reg_size(reg) == sizeof(uint64_t),
                 "invalid register size for CFA calculation\n");
          loc.type = CONSTANT;
          loc.cnst = *(uint64_t*)act->regs->reg(act->regs, reg) +
                     (Dwarf_Signed)rule->dw_offset_or_block_len;
          loc.num_bytes = sizeof(uint64_t);
        }
        else
        {
          ASSERT(rule->dw_regnum == DW_FRAME_CFA_COL3,
                 "invalid register for callee-saved storage offset\n");
          loc.type = ADDRESS;
          loc.addr = act->cfa + (Dwarf_Signed)rule->dw_offset_or_block_len;
          loc.num_bytes = ctx->handle->ptr_size;
        }
      }
      else
      {
        ASSERT(rule->dw_regnum != DW_FRAME_CFA_COL3,
               "invalid register for storing callee-saved register\n");
        loc.type = REGISTER;
        loc.reg = OP_REG(rule->dw_regnum);
        loc.num_bytes = ctx->handle->props->reg_size(loc.reg);
      }
      break;
    case DW_EXPR_VAL_OFFSET:
      loc.type = CONSTANT;
      loc.addr = act->cfa + (Dwarf_Signed)rule->dw_offset_or_block_len;
      loc.num_bytes = sizeof(uint64_t);
      break;
    case DW_EXPR_EXPRESSION:
      DWARF_OK(dwarf_loclist_from_expr_b(ctx->handle->dbg,
                                         rule->dw_block_ptr,
                                         rule->dw_offset_or_block_len,
                                         ctx->handle->ptr_size,
                                         sizeof(Dwarf_Off), /* Assumed to be 8 bytes */
                                         4, /* CU version = 4 per DWARF4 standard */
                                         &loc_desc,
                                         &loc_len, /* Should always be set to 1 */
                                         &err), "dwarf_loclist_from_expr_b");
      ASSERT(loc_len == 1, "invalid location description from expression\n");

      // Note: this should always be an address because the register rule is
      // handled by previous cases and the constant rule is handled below
      loc = get_val_from_desc(ctx, loc_desc);
      ASSERT(loc.is_valid, "could not evaluate expression for unwind rule\n");
      ASSERT(loc.type == ADDRESS, "invalid location for callee-saved register\n");
      dwarf_dealloc(ctx->handle->dbg, loc_desc->ld_s, DW_DLA_LOC_BLOCK);
      dwarf_dealloc(ctx->handle->dbg, loc_desc, DW_DLA_LOCDESC);
      break;
    case DW_EXPR_VAL_EXPRESSION:
      DWARF_OK(dwarf_loclist_from_expr_b(ctx->handle->dbg,
                                         rule->dw_block_ptr,
                                         rule->dw_offset_or_block_len,
                                         ctx->handle->ptr_size,
                                         sizeof(Dwarf_Off), /* Assumed to be 8 bytes */
                                         4, /* CU version = 4 per DWARF4 standard */
                                         &loc_desc,
                                         &loc_len, /* Should always be set to 1 */
                                         &err), "dwarf_loclist_from_expr_b");
      ASSERT(loc_len == 1, "invalid location description from expression\n");

      // Note: this should always be a constant because the register rule is
      // handled by previous cases and the address rule is handle above
      loc = get_val_from_desc(ctx, loc_desc);
      ASSERT(loc.is_valid, "could not evaluate expression for unwind rule\n");
      ASSERT(loc.type == CONSTANT, "invalid value for callee-saved register\n");
      dwarf_dealloc(ctx->handle->dbg, loc_desc->ld_s, DW_DLA_LOC_BLOCK);
      dwarf_dealloc(ctx->handle->dbg, loc_desc, DW_DLA_LOCDESC);
      break;
    default:
      ASSERT(false, "cannot process unwind rule\n");
      break;
    }
  }
  else loc.is_valid = false;
  return loc;
}

/*
 * Free a stack activation's information.
 */
void free_activation(st_handle handle, activation* act)
{
  ASSERT(act, "invalid arguments to free_activation()\n");

  act->regs->free(act->regs);
  if(act->function != handle->start_main &&
     act->function != handle->start_thread)
#ifdef _CHECKS
  {
    free_func_info(handle, act->function);
    memset(act->callee_saved.bits, 0, bitmap_size(act->callee_saved.size));
    memset(act->rules.rt3_rules, 0,
           sizeof(Dwarf_Regtable_Entry3) * handle->regops->num_regs);
  }
  act->function = NULL;
  act->cfa = NULL;
  act->regs = NULL;
  memset(&act->site, 0, sizeof(call_site));
  memset(&act->callee_saved, 0, sizeof(bitmap));
  memset(&act->rules, 0, sizeof(Dwarf_Regtable3));
#else
    free_func_info(handle, act->function);
#endif
}

