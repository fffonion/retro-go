/* gameplaySP RISC-V backend entry point.
 *
 * ESP32-S31 is RV32 with separate data/instruction cache maintenance through
 * ESP-IDF. This header is the place where cpu_threaded.c will bind the ARM and
 * Thumb translation macros to RV32 emission helpers.
 */

#ifndef RISCV_EMIT_H
#define RISCV_EMIT_H

#include "riscv_codegen.h"
#include <stdlib.h>

#define RISCV_BACKEND_IN_PROGRESS 1

static void riscv_dynarec_unimplemented(void) {
  abort();
}

void riscv_return_to_main(void);

static u32 riscv_thumb_adc_flags(u32 lhs, u32 rhs) {
  u32 carry = reg[REG_C_FLAG] & 1U;
  u32 rhs_carry = rhs + carry;
  u32 rhs_overflow = rhs_carry < rhs;
  u32 result = lhs + rhs_carry;
  reg[REG_C_FLAG] = rhs_overflow | (result < lhs);
  reg[REG_N_FLAG] = result >> 31;
  reg[REG_Z_FLAG] = result == 0;
  reg[REG_V_FLAG] = (~(lhs ^ rhs_carry) & (lhs ^ result)) >> 31;
  return result;
}

static u32 riscv_thumb_sbc_flags(u32 lhs, u32 rhs) {
  u32 borrow = (reg[REG_C_FLAG] & 1U) ^ 1U;
  u32 rhs_borrow = rhs + borrow;
  u32 rhs_overflow = rhs_borrow < rhs;
  u32 result = lhs - rhs_borrow;
  reg[REG_C_FLAG] = !(rhs_overflow | (lhs < rhs_borrow));
  reg[REG_N_FLAG] = result >> 31;
  reg[REG_Z_FLAG] = result == 0;
  reg[REG_V_FLAG] = ((lhs ^ rhs_borrow) & (lhs ^ result)) >> 31;
  return result;
}

static u32 riscv_thumb_lsl_reg_flags(u32 value, u32 amount) {
  amount &= 0xffU;
  if (amount == 0)
    return value;
  if (amount < 32) {
    reg[REG_C_FLAG] = (value >> (32 - amount)) & 1U;
    value <<= amount;
  } else if (amount == 32) {
    reg[REG_C_FLAG] = value & 1U;
    value = 0;
  } else {
    reg[REG_C_FLAG] = 0;
    value = 0;
  }
  reg[REG_N_FLAG] = value >> 31;
  reg[REG_Z_FLAG] = value == 0;
  return value;
}

static u32 riscv_thumb_lsr_reg_flags(u32 value, u32 amount) {
  amount &= 0xffU;
  if (amount == 0)
    return value;
  if (amount < 32) {
    reg[REG_C_FLAG] = (value >> (amount - 1)) & 1U;
    value >>= amount;
  } else if (amount == 32) {
    reg[REG_C_FLAG] = value >> 31;
    value = 0;
  } else {
    reg[REG_C_FLAG] = 0;
    value = 0;
  }
  reg[REG_N_FLAG] = value >> 31;
  reg[REG_Z_FLAG] = value == 0;
  return value;
}

static u32 riscv_thumb_asr_reg_flags(u32 value, u32 amount) {
  amount &= 0xffU;
  if (amount == 0)
    return value;
  if (amount < 32) {
    reg[REG_C_FLAG] = (value >> (amount - 1)) & 1U;
    value = (u32)((s32)value >> amount);
  } else {
    reg[REG_C_FLAG] = value >> 31;
    value = (u32)((s32)value >> 31);
  }
  reg[REG_N_FLAG] = value >> 31;
  reg[REG_Z_FLAG] = value == 0;
  return value;
}

static u32 riscv_thumb_ror_reg_flags(u32 value, u32 amount) {
  amount &= 0xffU;
  if (amount == 0)
    return value;
  amount &= 31U;
  if (amount)
    value = (value >> amount) | (value << (32 - amount));
  reg[REG_C_FLAG] = value >> 31;
  reg[REG_N_FLAG] = value >> 31;
  reg[REG_Z_FLAG] = value == 0;
  return value;
}

static u8 *riscv_arm_interpret_step(u32 pc) {
  reg[REG_PC] = pc & ~3U;
  reg[REG_CPSR] &= ~0x20U;
  execute_arm(1);
  if (reg[REG_CPSR] & 0x20U)
    return block_lookup_address_thumb(reg[REG_PC] & ~1U);
  if ((reg[REG_PC] & ~3U) != ((pc + 4) & ~3U))
    return block_lookup_address_arm(reg[REG_PC] & ~3U);
  return NULL;
}

static u8 *riscv_thumb_interpret_step(u32 pc) {
  reg[REG_PC] = pc & ~1U;
  reg[REG_CPSR] |= 0x20U;
  execute_arm(1);
  if (!(reg[REG_CPSR] & 0x20U))
    return block_lookup_address_arm(reg[REG_PC] & ~3U);
  if ((reg[REG_PC] & ~1U) != ((pc + 2) & ~1U))
    return block_lookup_address_thumb(reg[REG_PC] & ~1U);
  return NULL;
}

#define riscv_emit_unimplemented()                                           \
  do {                                                                        \
    rv_call_ptr((const void *)riscv_dynarec_unimplemented);                   \
  } while (0)

#define block_prologue_size 0

#define reg_a0 RV_A0
#define reg_a1 RV_A1
#define reg_a2 RV_A2
#define reg_rv RV_A0

#define reg_base RV_S0
#define reg_cycles RV_S1
#define reg_flag_tmp RV_S2

#define reg_x0 RV_S3
#define reg_x1 RV_S4
#define reg_x2 RV_S5
#define reg_x3 RV_S6
#define reg_x4 RV_S7
#define reg_x5 RV_S8

#define reg_rm RV_A0
#define reg_rn RV_A1
#define reg_rs RV_A2
#define reg_rd RV_A0

#define mem_reg (~0U)

#define rv_condition_eq 0
#define rv_condition_ne 1
#define rv_condition_cs 2
#define rv_condition_cc 3
#define rv_condition_mi 4
#define rv_condition_pl 5
#define rv_condition_vs 6
#define rv_condition_vc 7
#define rv_condition_hi 8
#define rv_condition_ls 9
#define rv_condition_ge 10
#define rv_condition_lt 11
#define rv_condition_gt 12
#define rv_condition_le 13
#define rv_condition_al 14

static inline void rv_emit_jump_placeholder_at(u8 **tptr) {
  rv32_emit32_at(tptr, rv32_j(0, RV_X0, 0x6f));
}

static inline void rv_emit_branch_placeholder_at(u8 **tptr) {
  rv32_emit32_at(tptr, rv32_b(0, RV_X0, RV_X0, 0, 0x63));
}

#define rv_emit_jump_placeholder() rv_emit_jump_placeholder_at(&translation_ptr)
#define rv_emit_branch_placeholder() rv_emit_branch_placeholder_at(&translation_ptr)

static const u32 arm_register_allocation[] = {
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
};

static const u32 thumb_register_allocation[] = {
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
  mem_reg, mem_reg, mem_reg, mem_reg,
};

static inline u32 rv_arm_alloc(u32 reg_index) {
  return arm_register_allocation[reg_index & 31U];
}

static inline u32 rv_thumb_alloc(u32 reg_index) {
  return thumb_register_allocation[reg_index & 31U];
}

static inline void rv_load_gba_reg_at(u8 **tptr, const u32 *alloc, u32 host,
    u32 reg_index, u32 pc_value) {
  if (reg_index == REG_PC) {
    rv_li_at(tptr, host, pc_value);
    return;
  }

  u32 mapped = alloc[reg_index & 31U];
  if (mapped == mem_reg) {
    rv_load_u32_abs_at(tptr, host, &reg[reg_index]);
  } else if (mapped != host) {
    rv32_emit32_at(tptr, rv32_i(0, mapped, 0, host, 0x13));
  }
}

static inline void rv_store_gba_reg_at(u8 **tptr, const u32 *alloc, u32 host,
    u32 reg_index) {
  u32 mapped = alloc[reg_index & 31U];
  if (mapped == mem_reg || reg_index == REG_PC) {
    rv_store_u32_abs_at(tptr, host, &reg[reg_index]);
  } else if (mapped != host) {
    rv32_emit32_at(tptr, rv32_i(0, host, 0, mapped, 0x13));
  }
}

#define generate_block_extra_vars_arm()
#define generate_block_extra_vars_thumb()

#define generate_block_prologue()                                             \
  do {                                                                        \
  } while (0)

#define generate_load_imm(ireg, imm, imm_ror)                                 \
  rv_li((ireg), (imm))

#define generate_mov(ireg_dest, ireg_src)                                     \
  rv_mv((ireg_dest), (ireg_src))

#define generate_add(ireg_dest, ireg_src)                                     \
  rv_add((ireg_dest), (ireg_dest), (ireg_src))

#define generate_sub(ireg_dest, ireg_src)                                     \
  rv_sub((ireg_dest), (ireg_dest), (ireg_src))

#define generate_and(ireg_dest, ireg_src)                                     \
  rv_and((ireg_dest), (ireg_dest), (ireg_src))

#define generate_or(ireg_dest, ireg_src)                                      \
  rv_or((ireg_dest), (ireg_dest), (ireg_src))

#define generate_xor(ireg_dest, ireg_src)                                     \
  rv_xor((ireg_dest), (ireg_dest), (ireg_src))

#define generate_not(ireg)                                                    \
  rv_xori((ireg), (ireg), -1)

#define generate_and_imm(ireg, imm, imm_ror)                                  \
  do {                                                                        \
    rv_li(RV_T0, (imm));                                                      \
    rv_and((ireg), (ireg), RV_T0);                                            \
  } while (0)

#define generate_add_imm(ireg, imm, imm_ror)                                  \
  rv_addi_checked((ireg), (ireg), (s32)(imm))

#define generate_sub_imm(ireg, imm, imm_ror)                                  \
  do {                                                                        \
    rv_li(RV_T0, (imm));                                                      \
    rv_sub((ireg), (ireg), RV_T0);                                            \
  } while (0)

#define generate_xor_imm(ireg, imm, imm_ror)                                  \
  do {                                                                        \
    rv_li(RV_T0, (imm));                                                      \
    rv_xor((ireg), (ireg), RV_T0);                                            \
  } while (0)

#define generate_add_reg_reg_imm(ireg_dest, ireg_src, imm, imm_ror)           \
  rv_addi_checked((ireg_dest), (ireg_src), (s32)(imm))

#define generate_load_memreg(ireg, reg_index)                                 \
  rv_load_u32_abs((ireg), &reg[(reg_index)])

#define arm_generate_load_reg(ireg, reg_index)                                \
  rv_load_gba_reg_at(&translation_ptr, arm_register_allocation, (ireg),       \
      (reg_index), pc + 8)

#define thumb_generate_load_reg(ireg, reg_index)                              \
  rv_load_gba_reg_at(&translation_ptr, thumb_register_allocation, (ireg),     \
      (reg_index), pc + 4)

#define arm_generate_load_reg_pc(ireg, reg_index, pc_offset)                  \
  rv_load_gba_reg_at(&translation_ptr, arm_register_allocation, (ireg),       \
      (reg_index), pc + (pc_offset))

#define thumb_generate_load_reg_pc(ireg, reg_index, pc_offset)                \
  rv_load_gba_reg_at(&translation_ptr, thumb_register_allocation, (ireg),     \
      (reg_index), pc + (pc_offset))

#define arm_generate_store_reg(ireg, reg_index)                               \
  rv_store_gba_reg_at(&translation_ptr, arm_register_allocation, (ireg),      \
      (reg_index))

#define thumb_generate_store_reg(ireg, reg_index)                             \
  rv_store_gba_reg_at(&translation_ptr, thumb_register_allocation, (ireg),    \
      (reg_index))

#define generate_update_pc(new_pc)                                            \
  do {                                                                        \
    rv_li(RV_T1, (new_pc));                                                   \
    rv_store_u32_abs(RV_T1, &reg[REG_PC]);                                    \
  } while (0)

#define generate_function_call(function_location)                             \
  rv_call_ptr((const void *)(function_location))

#define generate_function_far_call(function_number)                           \
  do {                                                                        \
    rv_load_u32_abs(RV_T0, &reg[REG_USERDEF + (function_number)]);            \
    rv_jalr(RV_RA, RV_T0, 0);                                                 \
  } while (0)

#define generate_cycle_update()                                               \
  do {                                                                        \
    if (cycle_count) {                                                        \
      rv_addi_checked(reg_cycles, reg_cycles, -(s32)cycle_count);             \
      u8 *skip_return = translation_ptr;                                      \
      rv_blt(RV_X0, reg_cycles, 0);                                           \
      rv_jump_ptr(riscv_return_to_main);                                      \
      rv_patch_branch((u32 *)skip_return, translation_ptr);                   \
      cycle_count = 0;                                                        \
    }                                                                         \
  } while (0)

#define generate_branch_patch_unconditional(dest, offset)                     \
  rv_patch_jal((u32 *)(dest), (offset))

#define generate_branch_patch_conditional(dest, offset)                       \
  rv_patch_branch((u32 *)(dest), (offset))

#define generate_branch_no_cycle_update(writeback_location, new_pc, mode)     \
  do {                                                                        \
    writeback_location = translation_ptr;                                     \
    rv_emit_jump_placeholder();                                               \
  } while (0)

#define generate_branch_cycle_update(writeback_location, new_pc, mode)        \
  do {                                                                        \
    generate_cycle_update();                                                  \
    generate_branch_no_cycle_update(writeback_location, new_pc, mode);        \
  } while (0)

#define generate_branch(mode)                                                 \
  do {                                                                        \
    generate_branch_cycle_update(                                             \
        block_exits[block_exit_position].branch_source,                       \
        block_exits[block_exit_position].branch_target, mode);                \
    block_exit_position++;                                                    \
  } while (0)

#define emit_trace_arm_instruction(pc)
#define emit_trace_thumb_instruction(pc)

#define generate_translation_gate(type)                                       \
  do {                                                                        \
    generate_update_pc(pc);                                                   \
    rv_tailcall_ptr(block_lookup_address_##type);                             \
  } while (0)

#define generate_indirect_branch_no_cycle_update(type)                        \
  do {                                                                        \
    generate_function_call(block_lookup_address_##type);                      \
    rv_jalr(RV_X0, reg_rv, 0);                                                \
  } while (0)

#define generate_indirect_branch_cycle_update(type)                           \
  do {                                                                        \
    generate_cycle_update();                                                  \
    generate_indirect_branch_no_cycle_update(type);                           \
  } while (0)

#define riscv_emit_interpreter_step(helper, current_pc)                       \
  do {                                                                        \
    rv_li(reg_a0, (current_pc));                                              \
    generate_function_call(helper);                                           \
    u8 *skip_branch = translation_ptr;                                        \
    rv_beq(reg_rv, RV_X0, 0);                                                 \
    rv_jalr(RV_X0, reg_rv, 0);                                                \
    rv_patch_branch((u32 *)skip_branch, translation_ptr);                     \
  } while (0)

#define riscv_update_nz_flags(ireg)                                           \
  do {                                                                        \
    rv_srli(RV_T1, (ireg), 31);                                               \
    rv_store_u32_abs(RV_T1, &reg[REG_N_FLAG]);                                \
    rv_sltiu(RV_T1, (ireg), 1);                                               \
    rv_store_u32_abs(RV_T1, &reg[REG_Z_FLAG]);                                \
  } while (0)

#define riscv_update_add_flags(result, lhs, rhs)                              \
  do {                                                                        \
    riscv_update_nz_flags((result));                                          \
    rv_sltu(RV_T1, (result), (lhs));                                          \
    rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                                \
    rv_xor(RV_T2, (lhs), (rhs));                                              \
    rv_xori(RV_T2, RV_T2, -1);                                                \
    rv_xor(RV_T3, (lhs), (result));                                           \
    rv_and(RV_T2, RV_T2, RV_T3);                                              \
    rv_srli(RV_T1, RV_T2, 31);                                                \
    rv_store_u32_abs(RV_T1, &reg[REG_V_FLAG]);                                \
  } while (0)

#define riscv_update_sub_flags(result, lhs, rhs)                              \
  do {                                                                        \
    riscv_update_nz_flags((result));                                          \
    rv_sltu(RV_T1, (lhs), (rhs));                                             \
    rv_xori(RV_T1, RV_T1, 1);                                                 \
    rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                                \
    rv_xor(RV_T2, (lhs), (rhs));                                              \
    rv_xor(RV_T3, (lhs), (result));                                           \
    rv_and(RV_T2, RV_T2, RV_T3);                                              \
    rv_srli(RV_T1, RV_T2, 31);                                                \
    rv_store_u32_abs(RV_T1, &reg[REG_V_FLAG]);                                \
  } while (0)

#define riscv_thumb_load_operand_reg(ireg, value)                             \
  thumb_generate_load_reg((ireg), (value))

#define riscv_thumb_load_operand_imm(ireg, value)                             \
  rv_li((ireg), (value))

#define riscv_thumb_load_operand(op_type, ireg, value)                        \
  riscv_thumb_load_operand_##op_type((ireg), (value))

#define riscv_arm_operand_supported_imm(opcode_value) 1
#define riscv_arm_operand_supported_imm_flags(opcode_value) 1
#define riscv_arm_operand_supported_reg(opcode_value)                         \
  (((opcode_value) & 0x00000ff0U) == 0)
#define riscv_arm_operand_supported_reg_flags(opcode_value)                   \
  (((opcode_value) & 0x00000ff0U) == 0)

static inline u32 riscv_arm_imm_operand(u32 imm, u32 imm_ror) {
  imm &= 0xffU;
  imm_ror &= 31U;
  if (imm_ror == 0)
    return imm;
  return (imm >> imm_ror) | (imm << (32 - imm_ror));
}

#define riscv_arm_load_operand_imm(ireg)                                      \
  rv_li((ireg), riscv_arm_imm_operand(imm, imm_ror))

#define riscv_arm_load_operand_imm_flags(ireg)                                \
  do {                                                                        \
    u32 arm_operand_value = riscv_arm_imm_operand(imm, imm_ror);              \
    rv_li((ireg), arm_operand_value);                                         \
    if (imm_ror) {                                                            \
      rv_li(RV_T1, arm_operand_value >> 31);                                  \
      rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                              \
    }                                                                         \
  } while (0)

#define riscv_arm_load_operand_reg(ireg)                                      \
  arm_generate_load_reg_pc((ireg), rm, 8)

#define riscv_arm_load_operand_reg_flags(ireg)                                \
  arm_generate_load_reg_pc((ireg), rm, 8)

#define riscv_arm_load_operand(type, ireg)                                    \
  riscv_arm_load_operand_##type((ireg))

#define riscv_arm_store_data_result(ireg, rd_value)                           \
  do {                                                                        \
    if ((rd_value) == REG_PC) {                                               \
      riscv_emit_interpreter_step(riscv_arm_interpret_step, pc);              \
    } else {                                                                  \
      arm_generate_store_reg((ireg), (rd_value));                             \
    }                                                                         \
  } while (0)

#define riscv_arm_data_proc_common(type, body)                                \
  do {                                                                        \
    arm_decode_data_proc_reg(opcode);                                         \
    u32 imm = opcode & 0xffU;                                                 \
    u32 imm_ror = ((opcode >> 8) & 0x0fU) * 2U;                               \
    if (!riscv_arm_operand_supported_##type(opcode) || rd == REG_PC) {        \
      riscv_emit_interpreter_step(riscv_arm_interpret_step, pc);              \
    } else {                                                                  \
      arm_generate_load_reg_pc(reg_a0, rn, 8);                                \
      riscv_arm_load_operand(type, reg_a1);                                   \
      body;                                                                   \
    }                                                                         \
  } while (0)

#define riscv_arm_data_proc_logic_emit(type, rvop, do_flags)                  \
  riscv_arm_data_proc_common(type, {                                          \
    rvop(reg_a2, reg_a0, reg_a1);                                             \
    if (do_flags)                                                             \
      riscv_update_nz_flags(reg_a2);                                          \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_add_emit(type, do_flags)                          \
  riscv_arm_data_proc_common(type, {                                          \
    rv_add(reg_a2, reg_a0, reg_a1);                                           \
    if (do_flags)                                                             \
      riscv_update_add_flags(reg_a2, reg_a0, reg_a1);                         \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_sub_emit(type, do_flags)                          \
  riscv_arm_data_proc_common(type, {                                          \
    rv_sub(reg_a2, reg_a0, reg_a1);                                           \
    if (do_flags)                                                             \
      riscv_update_sub_flags(reg_a2, reg_a0, reg_a1);                         \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_rsb_emit(type, do_flags)                          \
  riscv_arm_data_proc_common(type, {                                          \
    rv_sub(reg_a2, reg_a1, reg_a0);                                           \
    if (do_flags)                                                             \
      riscv_update_sub_flags(reg_a2, reg_a1, reg_a0);                         \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_adc_emit(type, do_flags)                          \
  riscv_arm_data_proc_common(type, {                                          \
    rv_load_u32_abs(RV_T1, &reg[REG_C_FLAG]);                                 \
    rv_add(reg_a2, reg_a0, reg_a1);                                           \
    rv_add(reg_a2, reg_a2, RV_T1);                                            \
    if (do_flags) {                                                           \
      generate_function_call(riscv_thumb_adc_flags);                          \
    } else {                                                                  \
      riscv_arm_store_data_result(reg_a2, rd);                                \
    }                                                                         \
    if (do_flags)                                                             \
      riscv_arm_store_data_result(reg_rv, rd);                                \
  })

#define riscv_arm_data_proc_sbc_emit(type, do_flags)                          \
  riscv_arm_data_proc_common(type, {                                          \
    rv_load_u32_abs(RV_T1, &reg[REG_C_FLAG]);                                 \
    rv_xori(RV_T1, RV_T1, 1);                                                 \
    rv_add(RV_T2, reg_a1, RV_T1);                                             \
    rv_sub(reg_a2, reg_a0, RV_T2);                                            \
    if (do_flags) {                                                           \
      generate_function_call(riscv_thumb_sbc_flags);                          \
    } else {                                                                  \
      riscv_arm_store_data_result(reg_a2, rd);                                \
    }                                                                         \
    if (do_flags)                                                             \
      riscv_arm_store_data_result(reg_rv, rd);                                \
  })

#define riscv_arm_data_proc_rsc_emit(type, do_flags)                          \
  riscv_arm_data_proc_common(type, {                                          \
    rv_load_u32_abs(RV_T1, &reg[REG_C_FLAG]);                                 \
    rv_xori(RV_T1, RV_T1, 1);                                                 \
    rv_add(RV_T2, reg_a0, RV_T1);                                             \
    rv_sub(reg_a2, reg_a1, RV_T2);                                            \
    if (do_flags) {                                                           \
      rv_sub(RV_T3, reg_a1, RV_T2);                                           \
      riscv_update_sub_flags(RV_T3, reg_a1, RV_T2);                           \
    }                                                                         \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_bic_emit(type, do_flags)                          \
  riscv_arm_data_proc_common(type, {                                          \
    generate_not(reg_a1);                                                     \
    rv_and(reg_a2, reg_a0, reg_a1);                                           \
    if (do_flags)                                                             \
      riscv_update_nz_flags(reg_a2);                                          \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_unary_common(type, body)                          \
  do {                                                                        \
    arm_decode_data_proc_imm(opcode);                                         \
    u32 rm = opcode & 0x0fU;                                                  \
    if (!riscv_arm_operand_supported_##type(opcode) || rd == REG_PC) {        \
      riscv_emit_interpreter_step(riscv_arm_interpret_step, pc);              \
    } else {                                                                  \
      riscv_arm_load_operand(type, reg_a1);                                   \
      body;                                                                   \
    }                                                                         \
  } while (0)

#define riscv_arm_data_proc_unary_mov_emit(type, do_flags)                    \
  riscv_arm_data_proc_unary_common(type, {                                    \
    generate_mov(reg_a2, reg_a1);                                             \
    if (do_flags)                                                             \
      riscv_update_nz_flags(reg_a2);                                          \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_unary_mvn_emit(type, do_flags)                    \
  riscv_arm_data_proc_unary_common(type, {                                    \
    generate_mov(reg_a2, reg_a1);                                             \
    generate_not(reg_a2);                                                     \
    if (do_flags)                                                             \
      riscv_update_nz_flags(reg_a2);                                          \
    riscv_arm_store_data_result(reg_a2, rd);                                  \
  })

#define riscv_arm_data_proc_test_common(type, body)                           \
  do {                                                                        \
    arm_decode_data_proc_reg(opcode);                                         \
    u32 imm = opcode & 0xffU;                                                 \
    u32 imm_ror = ((opcode >> 8) & 0x0fU) * 2U;                               \
    if (!riscv_arm_operand_supported_##type(opcode)) {                        \
      riscv_emit_interpreter_step(riscv_arm_interpret_step, pc);              \
    } else {                                                                  \
      arm_generate_load_reg_pc(reg_a0, rn, 8);                                \
      riscv_arm_load_operand(type, reg_a1);                                   \
      body;                                                                   \
    }                                                                         \
  } while (0)

#define riscv_arm_access_supported_imm(opcode_value) 1
#define riscv_arm_access_supported_reg(opcode_value)                          \
  (((opcode_value) & 0x00000ff0U) == 0)
#define riscv_arm_access_supported_half_imm(opcode_value) 0
#define riscv_arm_access_supported_half_reg(opcode_value) 0

#define arm_decode_data_trans_half_imm() arm_decode_half_trans_of()
#define arm_decode_data_trans_half_reg() arm_decode_half_trans_r()

#define riscv_arm_load_offset_imm() rv_li(reg_a1, offset)
#define riscv_arm_load_offset_reg() arm_generate_load_reg_pc(reg_a1, rm, 8)
#define riscv_arm_load_offset_half_imm() rv_li(reg_a1, 0)
#define riscv_arm_load_offset_half_reg() rv_li(reg_a1, 0)

#define riscv_arm_apply_direction_up(base_reg, off_reg, dest_reg)             \
  rv_add((dest_reg), (base_reg), (off_reg))
#define riscv_arm_apply_direction_down(base_reg, off_reg, dest_reg)           \
  rv_sub((dest_reg), (base_reg), (off_reg))

#define riscv_arm_address_post(direction)                                     \
  do {                                                                        \
    generate_mov(reg_a0, RV_T2);                                              \
    riscv_arm_apply_direction_##direction(RV_T2, reg_a1, RV_T3);              \
    arm_generate_store_reg(RV_T3, rn);                                        \
  } while (0)
#define riscv_arm_address_pre(direction)                                      \
  riscv_arm_apply_direction_##direction(RV_T2, reg_a1, reg_a0)
#define riscv_arm_address_pre_wb(direction)                                   \
  do {                                                                        \
    riscv_arm_apply_direction_##direction(RV_T2, reg_a1, reg_a0);             \
    arm_generate_store_reg(reg_a0, rn);                                       \
  } while (0)

#define riscv_arm_memory_load_u32(rd_value)                                   \
  do {                                                                        \
    cycle_count += 2;                                                         \
    generate_update_pc(pc);                                                   \
    generate_function_call(execute_load_u32);                                 \
    arm_generate_store_reg(reg_rv, (rd_value));                               \
  } while (0)
#define riscv_arm_memory_load_u8(rd_value)                                    \
  do {                                                                        \
    cycle_count += 2;                                                         \
    generate_update_pc(pc);                                                   \
    generate_function_call(execute_load_u8);                                  \
    arm_generate_store_reg(reg_rv, (rd_value));                               \
  } while (0)
#define riscv_arm_memory_load_u16(rd_value)                                   \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define riscv_arm_memory_load_s8(rd_value)                                    \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define riscv_arm_memory_load_s16(rd_value)                                   \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)

#define riscv_arm_memory_store_u32(rd_value)                                  \
  do {                                                                        \
    cycle_count++;                                                            \
    arm_generate_load_reg_pc(reg_a1, (rd_value), 12);                         \
    generate_update_pc(pc + 4);                                               \
    generate_function_call(execute_store_u32);                                \
  } while (0)
#define riscv_arm_memory_store_u8(rd_value)                                   \
  do {                                                                        \
    cycle_count++;                                                            \
    arm_generate_load_reg_pc(reg_a1, (rd_value), 12);                         \
    generate_update_pc(pc + 4);                                               \
    generate_function_call(execute_store_u8);                                 \
  } while (0)
#define riscv_arm_memory_store_u16(rd_value)                                  \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define riscv_arm_memory_store_s8(rd_value)                                   \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define riscv_arm_memory_store_s16(rd_value)                                  \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)

#define riscv_emit_branch_filler(funct3, rs1, rs2)                            \
  do {                                                                        \
    backpatch_address = translation_ptr;                                      \
    rv32_emit32(rv32_b(0, (rs2), (rs1), (funct3), 0x63));                     \
  } while (0)

#define riscv_load_flag(ireg, flag_reg)                                       \
  rv_load_u32_abs((ireg), &reg[(flag_reg)])

#define generate_condition_eq()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_Z_FLAG);                                       \
    riscv_emit_branch_filler(0, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_ne()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_Z_FLAG);                                       \
    riscv_emit_branch_filler(1, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_cs()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_C_FLAG);                                       \
    riscv_emit_branch_filler(0, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_cc()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_C_FLAG);                                       \
    riscv_emit_branch_filler(1, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_mi()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_N_FLAG);                                       \
    riscv_emit_branch_filler(0, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_pl()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_N_FLAG);                                       \
    riscv_emit_branch_filler(1, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_vs()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_V_FLAG);                                       \
    riscv_emit_branch_filler(0, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_vc()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_V_FLAG);                                       \
    riscv_emit_branch_filler(1, RV_T1, RV_X0);                                \
  } while (0)

#define riscv_load_unsigned_compare_flags()                                   \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_C_FLAG);                                       \
    riscv_load_flag(RV_T2, REG_Z_FLAG);                                       \
    rv_xori(RV_T1, RV_T1, 1);                                                 \
    rv_or(RV_T1, RV_T1, RV_T2);                                               \
  } while (0)

#define generate_condition_hi()                                               \
  do {                                                                        \
    riscv_load_unsigned_compare_flags();                                      \
    riscv_emit_branch_filler(1, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_ls()                                               \
  do {                                                                        \
    riscv_load_unsigned_compare_flags();                                      \
    riscv_emit_branch_filler(0, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_ge()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_N_FLAG);                                       \
    riscv_load_flag(RV_T2, REG_V_FLAG);                                       \
    riscv_emit_branch_filler(1, RV_T1, RV_T2);                                \
  } while (0)

#define generate_condition_lt()                                               \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_N_FLAG);                                       \
    riscv_load_flag(RV_T2, REG_V_FLAG);                                       \
    riscv_emit_branch_filler(0, RV_T1, RV_T2);                                \
  } while (0)

#define riscv_load_signed_compare_flags()                                     \
  do {                                                                        \
    riscv_load_flag(RV_T1, REG_N_FLAG);                                       \
    riscv_load_flag(RV_T2, REG_V_FLAG);                                       \
    rv_xor(RV_T1, RV_T1, RV_T2);                                              \
    riscv_load_flag(RV_T2, REG_Z_FLAG);                                       \
    rv_or(RV_T1, RV_T1, RV_T2);                                               \
  } while (0)

#define generate_condition_gt()                                               \
  do {                                                                        \
    riscv_load_signed_compare_flags();                                        \
    riscv_emit_branch_filler(1, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition_le()                                               \
  do {                                                                        \
    riscv_load_signed_compare_flags();                                        \
    riscv_emit_branch_filler(0, RV_T1, RV_X0);                                \
  } while (0)

#define generate_condition()                                                  \
  do {                                                                        \
    switch (condition) {                                                      \
      case 0x0: generate_condition_eq(); break;                               \
      case 0x1: generate_condition_ne(); break;                               \
      case 0x2: generate_condition_cs(); break;                               \
      case 0x3: generate_condition_cc(); break;                               \
      case 0x4: generate_condition_mi(); break;                               \
      case 0x5: generate_condition_pl(); break;                               \
      case 0x6: generate_condition_vs(); break;                               \
      case 0x7: generate_condition_vc(); break;                               \
      case 0x8: generate_condition_hi(); break;                               \
      case 0x9: generate_condition_ls(); break;                               \
      case 0xA: generate_condition_ge(); break;                               \
      case 0xB: generate_condition_lt(); break;                               \
      case 0xC: generate_condition_gt(); break;                               \
      case 0xD: generate_condition_le(); break;                               \
      default: backpatch_address = NULL; break;                               \
    }                                                                         \
  } while (0)

#define thumb_process_cheats() generate_function_call(process_cheats)
#define arm_process_cheats() generate_function_call(process_cheats)

#define arm_conditional_block_header()                                        \
  do {                                                                        \
    generate_cycle_update();                                                  \
    generate_condition();                                                     \
  } while (0)
#define arm_access_memory(access_type, direction, adjust_op, mem_type, off_type) \
  do {                                                                        \
    arm_decode_data_trans_##off_type();                                       \
    if (!riscv_arm_access_supported_##off_type(opcode) || rd == REG_PC) {     \
      riscv_emit_interpreter_step(riscv_arm_interpret_step, pc);              \
    } else {                                                                  \
      arm_generate_load_reg_pc(RV_T2, rn, 8);                                 \
      riscv_arm_load_offset_##off_type();                                     \
      riscv_arm_address_##adjust_op(direction);                               \
      riscv_arm_memory_##access_type##_##mem_type(rd);                        \
    }                                                                         \
  } while (0)
#define arm_multiply(add_op, flags)                                           \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define arm_multiply_long(name, add_op, flags)                                \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define arm_data_proc(name, type, flags_op)                                   \
  riscv_arm_data_proc_##name(type)
#define riscv_arm_data_proc_and(type) riscv_arm_data_proc_logic_emit(type, rv_and, 0)
#define riscv_arm_data_proc_ands(type) riscv_arm_data_proc_logic_emit(type, rv_and, 1)
#define riscv_arm_data_proc_eor(type) riscv_arm_data_proc_logic_emit(type, rv_xor, 0)
#define riscv_arm_data_proc_eors(type) riscv_arm_data_proc_logic_emit(type, rv_xor, 1)
#define riscv_arm_data_proc_sub(type) riscv_arm_data_proc_sub_emit(type, 0)
#define riscv_arm_data_proc_subs(type) riscv_arm_data_proc_sub_emit(type, 1)
#define riscv_arm_data_proc_rsb(type) riscv_arm_data_proc_rsb_emit(type, 0)
#define riscv_arm_data_proc_rsbs(type) riscv_arm_data_proc_rsb_emit(type, 1)
#define riscv_arm_data_proc_add(type) riscv_arm_data_proc_add_emit(type, 0)
#define riscv_arm_data_proc_adds(type) riscv_arm_data_proc_add_emit(type, 1)
#define riscv_arm_data_proc_adc(type) riscv_arm_data_proc_adc_emit(type, 0)
#define riscv_arm_data_proc_adcs(type) riscv_arm_data_proc_adc_emit(type, 1)
#define riscv_arm_data_proc_sbc(type) riscv_arm_data_proc_sbc_emit(type, 0)
#define riscv_arm_data_proc_sbcs(type) riscv_arm_data_proc_sbc_emit(type, 1)
#define riscv_arm_data_proc_rsc(type) riscv_arm_data_proc_rsc_emit(type, 0)
#define riscv_arm_data_proc_rscs(type) riscv_arm_data_proc_rsc_emit(type, 1)
#define riscv_arm_data_proc_orr(type) riscv_arm_data_proc_logic_emit(type, rv_or, 0)
#define riscv_arm_data_proc_orrs(type) riscv_arm_data_proc_logic_emit(type, rv_or, 1)
#define riscv_arm_data_proc_bic(type) riscv_arm_data_proc_bic_emit(type, 0)
#define riscv_arm_data_proc_bics(type) riscv_arm_data_proc_bic_emit(type, 1)
#define arm_data_proc_test(name, type)                                        \
  riscv_arm_data_proc_test_##name(type)
#define riscv_arm_data_proc_test_tst(type)                                    \
  riscv_arm_data_proc_test_common(type, {                                     \
    rv_and(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_nz_flags(reg_a2);                                            \
  })
#define riscv_arm_data_proc_test_teq(type)                                    \
  riscv_arm_data_proc_test_common(type, {                                     \
    rv_xor(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_nz_flags(reg_a2);                                            \
  })
#define riscv_arm_data_proc_test_cmp(type)                                    \
  riscv_arm_data_proc_test_common(type, {                                     \
    rv_sub(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_sub_flags(reg_a2, reg_a0, reg_a1);                           \
  })
#define riscv_arm_data_proc_test_cmn(type)                                    \
  riscv_arm_data_proc_test_common(type, {                                     \
    rv_add(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_add_flags(reg_a2, reg_a0, reg_a1);                           \
  })
#define arm_data_proc_unary(name, type, flags_op)                             \
  riscv_arm_data_proc_unary_##name(type)
#define riscv_arm_data_proc_unary_mov(type)                                   \
  riscv_arm_data_proc_unary_mov_emit(type, 0)
#define riscv_arm_data_proc_unary_movs(type)                                  \
  riscv_arm_data_proc_unary_mov_emit(type, 1)
#define riscv_arm_data_proc_unary_mvn(type)                                   \
  riscv_arm_data_proc_unary_mvn_emit(type, 0)
#define riscv_arm_data_proc_unary_mvns(type)                                  \
  riscv_arm_data_proc_unary_mvn_emit(type, 1)
#define arm_psr(op_type, transfer_type, psr_reg)                              \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define arm_swap(type)                                                        \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define arm_block_memory(access_type, offset_type, writeback_type, s_bit)     \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define arm_b() generate_branch(arm)
#define arm_bl()                                                              \
  do {                                                                        \
    rv_li(reg_a0, pc + 4);                                                    \
    arm_generate_store_reg(reg_a0, REG_LR);                                   \
    generate_branch(arm);                                                     \
  } while (0)
#define arm_bx()                                                              \
  do {                                                                        \
    arm_decode_branchx(opcode);                                               \
    arm_generate_load_reg(reg_a0, rn);                                        \
    generate_indirect_branch_cycle_update(dual);                              \
  } while (0)
#define arm_swi()                                                             \
  riscv_emit_interpreter_step(riscv_arm_interpret_step, pc)
#define arm_hle_div(cpu_mode)                                                 \
  riscv_emit_interpreter_step(riscv_##cpu_mode##_interpret_step, pc)
#define arm_hle_div_arm(cpu_mode)                                             \
  riscv_emit_interpreter_step(riscv_##cpu_mode##_interpret_step, pc)

#define thumb_data_proc(type, name, op_type, _rd, _rs, _rn)                  \
  riscv_thumb_data_proc_##name(type, op_type, _rd, _rs, _rn)
#define riscv_thumb_data_proc_ands(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    generate_and(reg_a0, reg_a1);                                             \
    riscv_update_nz_flags(reg_a0);                                            \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_eors(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    generate_xor(reg_a0, reg_a1);                                             \
    riscv_update_nz_flags(reg_a0);                                            \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_orrs(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    generate_or(reg_a0, reg_a1);                                              \
    riscv_update_nz_flags(reg_a0);                                            \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_adds(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    rv_add(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_add_flags(reg_a2, reg_a0, reg_a1);                           \
    thumb_generate_store_reg(reg_a2, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_subs(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    rv_sub(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_sub_flags(reg_a2, reg_a0, reg_a1);                           \
    thumb_generate_store_reg(reg_a2, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_adcs(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    generate_function_call(riscv_thumb_adc_flags);                            \
    thumb_generate_store_reg(reg_rv, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_sbcs(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    generate_function_call(riscv_thumb_sbc_flags);                            \
    thumb_generate_store_reg(reg_rv, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_muls(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    rv_mul(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_nz_flags(reg_a2);                                            \
    thumb_generate_store_reg(reg_a2, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_bics(type, op_type, _rd, _rs, _rn)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rs));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rn));                         \
    generate_not(reg_a1);                                                     \
    generate_and(reg_a0, reg_a1);                                             \
    riscv_update_nz_flags(reg_a0);                                            \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define thumb_data_proc_test(type, name, op_type, _rd, _rs)                  \
  riscv_thumb_data_proc_test_##name(type, op_type, _rd, _rs)
#define riscv_thumb_data_proc_test_tst(type, op_type, _rd, _rs)              \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rd));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rs));                         \
    generate_and(reg_a0, reg_a1);                                             \
    riscv_update_nz_flags(reg_a0);                                            \
  } while (0)
#define riscv_thumb_data_proc_test_cmp(type, op_type, _rd, _rs)              \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rd));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rs));                         \
    rv_sub(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_sub_flags(reg_a2, reg_a0, reg_a1);                           \
  } while (0)
#define riscv_thumb_data_proc_test_cmn(type, op_type, _rd, _rs)              \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    thumb_generate_load_reg(reg_a0, (_rd));                                   \
    riscv_thumb_load_operand(op_type, reg_a1, (_rs));                         \
    rv_add(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_add_flags(reg_a2, reg_a0, reg_a1);                           \
  } while (0)
#define thumb_data_proc_unary(type, name, op_type, _rd, _rs)                 \
  riscv_thumb_data_proc_unary_##name(type, op_type, _rd, _rs)
#define riscv_thumb_data_proc_unary_movs(type, op_type, _rd, _rs)            \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    riscv_thumb_load_operand(op_type, reg_a0, (_rs));                         \
    riscv_update_nz_flags(reg_a0);                                            \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_unary_neg(type, op_type, _rd, _rs)             \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    rv_li(reg_a0, 0);                                                         \
    riscv_thumb_load_operand(op_type, reg_a1, (_rs));                         \
    rv_sub(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_sub_flags(reg_a2, reg_a0, reg_a1);                           \
    thumb_generate_store_reg(reg_a2, (_rd));                                  \
  } while (0)
#define riscv_thumb_data_proc_unary_mvns(type, op_type, _rd, _rs)            \
  do {                                                                        \
    thumb_decode_##type();                                                    \
    riscv_thumb_load_operand(op_type, reg_a0, (_rs));                         \
    generate_not(reg_a0);                                                     \
    riscv_update_nz_flags(reg_a0);                                            \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define thumb_data_proc_hi(name)                                              \
  do {                                                                        \
    thumb_decode_hireg_op();                                                  \
    thumb_generate_load_reg_pc(reg_a0, rd, 4);                                \
    thumb_generate_load_reg_pc(reg_a1, rs, 4);                                \
    generate_##name(reg_a0, reg_a1);                                          \
    if (rd == REG_PC) {                                                       \
      rv_andi(reg_a0, reg_a0, -2);                                            \
      thumb_generate_store_reg(reg_a0, REG_PC);                               \
      generate_indirect_branch_cycle_update(thumb);                           \
    } else {                                                                  \
      thumb_generate_store_reg(reg_a0, rd);                                   \
    }                                                                         \
  } while (0)
#define thumb_data_proc_test_hi(name)                                         \
  do {                                                                        \
    thumb_decode_hireg_op();                                                  \
    thumb_generate_load_reg_pc(reg_a0, rd, 4);                                \
    thumb_generate_load_reg_pc(reg_a1, rs, 4);                                \
    rv_sub(reg_a2, reg_a0, reg_a1);                                           \
    riscv_update_sub_flags(reg_a2, reg_a0, reg_a1);                           \
  } while (0)
#define thumb_data_proc_mov_hi()                                              \
  do {                                                                        \
    thumb_decode_hireg_op();                                                  \
    thumb_generate_load_reg_pc(reg_a0, rs, 4);                                \
    if (rd == REG_PC) {                                                       \
      rv_andi(reg_a0, reg_a0, -2);                                            \
      thumb_generate_store_reg(reg_a0, REG_PC);                               \
      generate_indirect_branch_cycle_update(thumb);                           \
    } else {                                                                  \
      thumb_generate_store_reg(reg_a0, rd);                                   \
    }                                                                         \
  } while (0)
#define thumb_load_pc(_rd)                                                    \
  do {                                                                        \
    thumb_decode_imm();                                                       \
    rv_li(reg_a0, ((pc & ~2U) + 4) + (imm * 4));                              \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define thumb_load_sp(_rd)                                                    \
  do {                                                                        \
    thumb_decode_imm();                                                       \
    thumb_generate_load_reg(reg_a0, REG_SP);                                  \
    rv_addi_checked(reg_a0, reg_a0, (s32)(imm * 4));                          \
    thumb_generate_store_reg(reg_a0, (_rd));                                  \
  } while (0)
#define thumb_adjust_sp_up()                                                  \
  rv_addi_checked(reg_a0, reg_a0, (s32)(imm * 4))
#define thumb_adjust_sp_down()                                                \
  rv_addi_checked(reg_a0, reg_a0, -(s32)(imm * 4))
#define thumb_adjust_sp(direction)                                            \
  do {                                                                        \
    thumb_decode_add_sp();                                                    \
    thumb_generate_load_reg(reg_a0, REG_SP);                                  \
    thumb_adjust_sp_##direction();                                            \
    thumb_generate_store_reg(reg_a0, REG_SP);                                 \
  } while (0)
#define thumb_shift(decode_type, op_type, value_type)                         \
  riscv_thumb_shift_##op_type##_##value_type(decode_type)
#define riscv_thumb_shift_lsl_imm(decode_type)                                \
  do {                                                                        \
    thumb_decode_##decode_type();                                             \
    thumb_generate_load_reg(reg_a0, rs);                                      \
    if (imm == 0) {                                                           \
      generate_mov(reg_a2, reg_a0);                                           \
    } else {                                                                  \
      rv_srli(RV_T1, reg_a0, 32 - imm);                                       \
      rv_andi(RV_T1, RV_T1, 1);                                               \
      rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                              \
      rv_slli(reg_a2, reg_a0, imm);                                           \
    }                                                                         \
    riscv_update_nz_flags(reg_a2);                                            \
    thumb_generate_store_reg(reg_a2, rd);                                     \
  } while (0)
#define riscv_thumb_shift_lsr_imm(decode_type)                                \
  do {                                                                        \
    thumb_decode_##decode_type();                                             \
    thumb_generate_load_reg(reg_a0, rs);                                      \
    if (imm == 0) {                                                           \
      rv_srli(RV_T1, reg_a0, 31);                                             \
      rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                              \
      rv_li(reg_a2, 0);                                                       \
    } else {                                                                  \
      rv_srli(RV_T1, reg_a0, imm - 1);                                        \
      rv_andi(RV_T1, RV_T1, 1);                                               \
      rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                              \
      rv_srli(reg_a2, reg_a0, imm);                                           \
    }                                                                         \
    riscv_update_nz_flags(reg_a2);                                            \
    thumb_generate_store_reg(reg_a2, rd);                                     \
  } while (0)
#define riscv_thumb_shift_asr_imm(decode_type)                                \
  do {                                                                        \
    thumb_decode_##decode_type();                                             \
    thumb_generate_load_reg(reg_a0, rs);                                      \
    if (imm == 0) {                                                           \
      rv_srli(RV_T1, reg_a0, 31);                                             \
      rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                              \
      rv_srai(reg_a2, reg_a0, 31);                                            \
    } else {                                                                  \
      rv_srli(RV_T1, reg_a0, imm - 1);                                        \
      rv_andi(RV_T1, RV_T1, 1);                                               \
      rv_store_u32_abs(RV_T1, &reg[REG_C_FLAG]);                              \
      rv_srai(reg_a2, reg_a0, imm);                                           \
    }                                                                         \
    riscv_update_nz_flags(reg_a2);                                            \
    thumb_generate_store_reg(reg_a2, rd);                                     \
  } while (0)
#define riscv_thumb_shift_lsl_reg(decode_type)                                \
  do {                                                                        \
    thumb_decode_##decode_type();                                             \
    thumb_generate_load_reg(reg_a0, rd);                                      \
    thumb_generate_load_reg(reg_a1, rs);                                      \
    generate_function_call(riscv_thumb_lsl_reg_flags);                        \
    thumb_generate_store_reg(reg_rv, rd);                                     \
  } while (0)
#define riscv_thumb_shift_lsr_reg(decode_type)                                \
  do {                                                                        \
    thumb_decode_##decode_type();                                             \
    thumb_generate_load_reg(reg_a0, rd);                                      \
    thumb_generate_load_reg(reg_a1, rs);                                      \
    generate_function_call(riscv_thumb_lsr_reg_flags);                        \
    thumb_generate_store_reg(reg_rv, rd);                                     \
  } while (0)
#define riscv_thumb_shift_asr_reg(decode_type)                                \
  do {                                                                        \
    thumb_decode_##decode_type();                                             \
    thumb_generate_load_reg(reg_a0, rd);                                      \
    thumb_generate_load_reg(reg_a1, rs);                                      \
    generate_function_call(riscv_thumb_asr_reg_flags);                        \
    thumb_generate_store_reg(reg_rv, rd);                                     \
  } while (0)
#define riscv_thumb_shift_ror_reg(decode_type)                                \
  do {                                                                        \
    thumb_decode_##decode_type();                                             \
    thumb_generate_load_reg(reg_a0, rd);                                      \
    thumb_generate_load_reg(reg_a1, rs);                                      \
    generate_function_call(riscv_thumb_ror_reg_flags);                        \
    thumb_generate_store_reg(reg_rv, rd);                                     \
  } while (0)
#define thumb_load_pc_pool_const(reg_rd, value)                               \
  do {                                                                        \
    rv_li(reg_a0, (value));                                                   \
    thumb_generate_store_reg(reg_a0, (reg_rd));                               \
  } while (0)
#define thumb_access_memory_load(mem_type, reg_rd)                            \
  do {                                                                        \
    cycle_count += 2;                                                         \
    generate_update_pc(pc);                                                   \
    generate_function_call(execute_load_##mem_type);                          \
    thumb_generate_store_reg(reg_rv, (reg_rd));                               \
  } while (0)
#define thumb_access_memory_store(mem_type, reg_rd)                           \
  do {                                                                        \
    cycle_count++;                                                            \
    thumb_generate_load_reg(reg_a1, (reg_rd));                                \
    generate_update_pc(pc + 2);                                               \
    generate_function_call(execute_store_##mem_type);                         \
  } while (0)
#define thumb_access_memory_generate_address_pc_relative(offset, _rb, _ro)    \
  rv_li(reg_a0, (offset))
#define thumb_access_memory_generate_address_reg_imm(offset, _rb, _ro)        \
  do {                                                                        \
    thumb_generate_load_reg(reg_a0, (_rb));                                   \
    rv_addi_checked(reg_a0, reg_a0, (s32)(offset));                           \
  } while (0)
#define thumb_access_memory_generate_address_reg_imm_sp(offset, _rb, _ro)     \
  do {                                                                        \
    thumb_generate_load_reg(reg_a0, (_rb));                                   \
    rv_addi_checked(reg_a0, reg_a0, (s32)((offset) * 4));                     \
  } while (0)
#define thumb_access_memory_generate_address_reg_reg(offset, _rb, _ro)        \
  do {                                                                        \
    thumb_generate_load_reg(reg_a0, (_rb));                                   \
    thumb_generate_load_reg(reg_a1, (_ro));                                   \
    generate_add(reg_a0, reg_a1);                                             \
  } while (0)
#define thumb_access_memory(access_type, op_type, _rd, _rb, _ro,             \
    address_type, offset_type, mem_type)                                      \
  do {                                                                        \
    thumb_decode_##op_type();                                                 \
    thumb_access_memory_generate_address_##address_type(                      \
        offset_type, _rb, _ro);                                               \
    thumb_access_memory_##access_type(mem_type, _rd);                         \
  } while (0)
#define thumb_block_address_preadjust_no()                                    \
  do {                                                                        \
  } while (0)
#define thumb_block_address_preadjust_down()                                  \
  rv_addi_checked(reg_a0, reg_a0, -(s32)(bit_count[reg_list] * 4))
#define thumb_block_address_preadjust_push_lr()                               \
  rv_addi_checked(reg_a0, reg_a0, -(s32)((bit_count[reg_list] + 1) * 4))
#define thumb_block_address_postadjust_no(base_reg)                           \
  thumb_generate_store_reg(reg_a0, (base_reg))
#define thumb_block_address_postadjust_up(base_reg)                           \
  do {                                                                        \
    rv_addi_checked(reg_a1, reg_a0, (s32)(bit_count[reg_list] * 4));          \
    thumb_generate_store_reg(reg_a1, (base_reg));                             \
  } while (0)
#define thumb_block_address_postadjust_down(base_reg)                         \
  do {                                                                        \
    rv_addi_checked(reg_a1, reg_a0, -(s32)(bit_count[reg_list] * 4));         \
    thumb_generate_store_reg(reg_a1, (base_reg));                             \
  } while (0)
#define thumb_block_address_postadjust_pop_pc(base_reg)                       \
  do {                                                                        \
    rv_addi_checked(reg_a1, reg_a0,                                           \
        (s32)((bit_count[reg_list] + 1) * 4));                                \
    thumb_generate_store_reg(reg_a1, (base_reg));                             \
  } while (0)
#define thumb_block_address_postadjust_push_lr(base_reg)                      \
  thumb_generate_store_reg(reg_a0, (base_reg))
#define thumb_block_memory_load(i)                                            \
  do {                                                                        \
    rv_li(reg_a1, pc);                                                        \
    generate_function_call(execute_load_u32);                                 \
    thumb_generate_store_reg(reg_rv, (i));                                    \
  } while (0)
#define thumb_block_memory_store(i)                                           \
  do {                                                                        \
    thumb_generate_load_reg(reg_a1, (i));                                     \
    generate_function_call(execute_store_aligned_u32);                        \
  } while (0)
#define thumb_block_memory_extra_no()                                         \
  do {                                                                        \
  } while (0)
#define thumb_block_memory_extra_up()                                         \
  do {                                                                        \
  } while (0)
#define thumb_block_memory_extra_down()                                       \
  do {                                                                        \
  } while (0)
#define thumb_block_memory_extra_push_lr()                                    \
  do {                                                                        \
    rv_load_u32_abs(reg_a0, &reg[REG_SAVE3]);                                 \
    rv_addi_checked(reg_a0, reg_a0, (s32)(bit_count[reg_list] * 4));          \
    thumb_generate_load_reg(reg_a1, REG_LR);                                  \
    generate_function_call(execute_store_aligned_u32);                        \
  } while (0)
#define thumb_block_memory_extra_pop_pc()                                     \
  do {                                                                        \
    rv_load_u32_abs(reg_a0, &reg[REG_SAVE3]);                                 \
    rv_addi_checked(reg_a0, reg_a0, (s32)(bit_count[reg_list] * 4));          \
    rv_li(reg_a1, pc + 4);                                                    \
    generate_function_call(execute_load_u32);                                 \
    rv_andi(reg_rv, reg_rv, -2);                                              \
    thumb_generate_store_reg(reg_rv, REG_PC);                                 \
    generate_indirect_branch_cycle_update(thumb);                             \
  } while (0)
#define thumb_block_memory(access_type, pre_op, post_op, base_reg)           \
  do {                                                                        \
    thumb_decode_rlist();                                                     \
    u32 offset = 0;                                                           \
    thumb_generate_load_reg(reg_a0, (base_reg));                              \
    rv_andi(reg_a0, reg_a0, -4);                                              \
    thumb_block_address_preadjust_##pre_op();                                 \
    thumb_block_address_postadjust_##post_op(base_reg);                       \
    rv_store_u32_abs(reg_a0, &reg[REG_SAVE3]);                                \
    for (u32 i = 0; i < 8; i++) {                                             \
      if ((reg_list >> i) & 0x01) {                                           \
        cycle_count++;                                                        \
        rv_load_u32_abs(reg_a0, &reg[REG_SAVE3]);                             \
        rv_addi_checked(reg_a0, reg_a0, (s32)offset);                         \
        thumb_block_memory_##access_type(i);                                  \
        offset += 4;                                                          \
      }                                                                       \
    }                                                                         \
    thumb_block_memory_extra_##post_op();                                     \
  } while (0)
#define thumb_conditional_branch(condition)                                   \
  do {                                                                        \
    generate_cycle_update();                                                  \
    generate_condition_##condition();                                         \
    block_exits[block_exit_position].branch_source = translation_ptr;         \
    rv_emit_jump_placeholder();                                               \
    generate_branch_patch_conditional(backpatch_address, translation_ptr);    \
    block_exit_position++;                                                    \
  } while (0)
#define thumb_b() generate_branch(thumb)
#define thumb_bl()                                                            \
  do {                                                                        \
    rv_li(reg_a0, ((pc + 2) | 0x01));                                         \
    thumb_generate_store_reg(reg_a0, REG_LR);                                 \
    generate_branch(thumb);                                                   \
  } while (0)
#define thumb_blh()                                                           \
  do {                                                                        \
    thumb_decode_branch();                                                    \
    rv_li(reg_a0, ((pc + 2) | 0x01));                                         \
    thumb_generate_load_reg(reg_a1, REG_LR);                                  \
    thumb_generate_store_reg(reg_a0, REG_LR);                                 \
    rv_addi_checked(reg_a0, reg_a1, (s32)(offset * 2));                       \
    rv_andi(reg_a0, reg_a0, -2);                                              \
    thumb_generate_store_reg(reg_a0, REG_PC);                                 \
    generate_indirect_branch_cycle_update(thumb);                             \
  } while (0)
#define thumb_bx()                                                            \
  do {                                                                        \
    thumb_decode_hireg_op();                                                  \
    thumb_generate_load_reg_pc(reg_a0, rs, 4);                                \
    thumb_generate_store_reg(reg_a0, REG_PC);                                 \
    generate_indirect_branch_cycle_update(dual);                              \
  } while (0)
#define thumb_swi()                                                           \
  riscv_emit_interpreter_step(riscv_thumb_interpret_step, pc)

void init_emitter(bool must_swap) {
  (void)must_swap;
  rom_cache_watermark = INITIAL_ROM_WATERMARK;
  init_bios_hooks();
}

u32 execute_arm_translate_internal(u32 cycles, void *regptr);
u32 execute_arm_translate(u32 cycles) {
  return execute_arm_translate_internal(cycles, &reg[0]);
}

#endif
