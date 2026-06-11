/* gameplaySP RISC-V code emitter helpers */

#ifndef RISCV_CODEGEN_H
#define RISCV_CODEGEN_H

#include <stdbool.h>
#include <stdint.h>

#include "../common.h"

enum {
  RV_X0 = 0,
  RV_RA = 1,
  RV_SP = 2,
  RV_GP = 3,
  RV_TP = 4,
  RV_T0 = 5,
  RV_T1 = 6,
  RV_T2 = 7,
  RV_S0 = 8,
  RV_S1 = 9,
  RV_A0 = 10,
  RV_A1 = 11,
  RV_A2 = 12,
  RV_A3 = 13,
  RV_A4 = 14,
  RV_A5 = 15,
  RV_A6 = 16,
  RV_A7 = 17,
  RV_S2 = 18,
  RV_S3 = 19,
  RV_S4 = 20,
  RV_S5 = 21,
  RV_S6 = 22,
  RV_S7 = 23,
  RV_S8 = 24,
  RV_S9 = 25,
  RV_S10 = 26,
  RV_S11 = 27,
  RV_T3 = 28,
  RV_T4 = 29,
  RV_T5 = 30,
  RV_T6 = 31
};

static inline void rv32_emit32_at(u8 **tptr, u32 value) {
  *((u32 *)*tptr) = value;
  *tptr += 4;
}

#define rv32_emit32(value) rv32_emit32_at(&translation_ptr, (u32)(value))

static inline u32 rv32_r(u32 funct7, u32 rs2, u32 rs1, u32 funct3, u32 rd,
    u32 opcode) {
  return ((funct7 & 0x7f) << 25) | ((rs2 & 0x1f) << 20) |
         ((rs1 & 0x1f) << 15) | ((funct3 & 0x07) << 12) |
         ((rd & 0x1f) << 7) | (opcode & 0x7f);
}

static inline u32 rv32_i(s32 imm, u32 rs1, u32 funct3, u32 rd, u32 opcode) {
  return (((u32)imm & 0xfff) << 20) | ((rs1 & 0x1f) << 15) |
         ((funct3 & 0x07) << 12) | ((rd & 0x1f) << 7) | (opcode & 0x7f);
}

static inline u32 rv32_s(s32 imm, u32 rs2, u32 rs1, u32 funct3, u32 opcode) {
  u32 uimm = (u32)imm;
  return ((uimm & 0xfe0) << 20) | ((rs2 & 0x1f) << 20) |
         ((rs1 & 0x1f) << 15) | ((funct3 & 0x07) << 12) |
         ((uimm & 0x1f) << 7) | (opcode & 0x7f);
}

static inline u32 rv32_b(s32 imm, u32 rs2, u32 rs1, u32 funct3, u32 opcode) {
  u32 uimm = (u32)imm;
  return ((uimm & 0x1000) << 19) | ((uimm & 0x07e0) << 20) |
         ((rs2 & 0x1f) << 20) | ((rs1 & 0x1f) << 15) |
         ((funct3 & 0x07) << 12) | ((uimm & 0x001e) << 7) |
         ((uimm & 0x0800) >> 4) | (opcode & 0x7f);
}

static inline u32 rv32_u(s32 imm, u32 rd, u32 opcode) {
  return ((u32)imm & 0xfffff000U) | ((rd & 0x1f) << 7) | (opcode & 0x7f);
}

static inline u32 rv32_j(s32 imm, u32 rd, u32 opcode) {
  u32 uimm = (u32)imm;
  return ((uimm & 0x100000) << 11) | (uimm & 0x000ff000) |
         ((uimm & 0x00000800) << 9) | ((uimm & 0x000007fe) << 20) |
         ((rd & 0x1f) << 7) | (opcode & 0x7f);
}

#define rv_addi(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 0, (rd), 0x13))
#define rv_slti(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 2, (rd), 0x13))
#define rv_sltiu(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 3, (rd), 0x13))
#define rv_xori(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 4, (rd), 0x13))
#define rv_ori(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 6, (rd), 0x13))
#define rv_andi(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 7, (rd), 0x13))
#define rv_slli(rd, rs1, shamt) rv32_emit32(rv32_i((shamt) & 0x1f, (rs1), 1, (rd), 0x13))
#define rv_srli(rd, rs1, shamt) rv32_emit32(rv32_i((shamt) & 0x1f, (rs1), 5, (rd), 0x13))
#define rv_srai(rd, rs1, shamt) rv32_emit32(rv32_i(0x400 | ((shamt) & 0x1f), (rs1), 5, (rd), 0x13))

#define rv_add(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 0, (rd), 0x33))
#define rv_sub(rd, rs1, rs2) rv32_emit32(rv32_r(0x20, (rs2), (rs1), 0, (rd), 0x33))
#define rv_sll(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 1, (rd), 0x33))
#define rv_slt(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 2, (rd), 0x33))
#define rv_sltu(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 3, (rd), 0x33))
#define rv_xor(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 4, (rd), 0x33))
#define rv_srl(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 5, (rd), 0x33))
#define rv_sra(rd, rs1, rs2) rv32_emit32(rv32_r(0x20, (rs2), (rs1), 5, (rd), 0x33))
#define rv_or(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 6, (rd), 0x33))
#define rv_and(rd, rs1, rs2) rv32_emit32(rv32_r(0x00, (rs2), (rs1), 7, (rd), 0x33))

#define rv_mul(rd, rs1, rs2) rv32_emit32(rv32_r(0x01, (rs2), (rs1), 0, (rd), 0x33))
#define rv_mulh(rd, rs1, rs2) rv32_emit32(rv32_r(0x01, (rs2), (rs1), 1, (rd), 0x33))
#define rv_mulhu(rd, rs1, rs2) rv32_emit32(rv32_r(0x01, (rs2), (rs1), 3, (rd), 0x33))

#define rv_lui(rd, imm) rv32_emit32(rv32_u((imm), (rd), 0x37))
#define rv_auipc(rd, imm) rv32_emit32(rv32_u((imm), (rd), 0x17))
#define rv_jal(rd, imm) rv32_emit32(rv32_j((imm), (rd), 0x6f))
#define rv_jalr(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 0, (rd), 0x67))

#define rv_beq(rs1, rs2, imm) rv32_emit32(rv32_b((imm), (rs2), (rs1), 0, 0x63))
#define rv_bne(rs1, rs2, imm) rv32_emit32(rv32_b((imm), (rs2), (rs1), 1, 0x63))
#define rv_blt(rs1, rs2, imm) rv32_emit32(rv32_b((imm), (rs2), (rs1), 4, 0x63))
#define rv_bge(rs1, rs2, imm) rv32_emit32(rv32_b((imm), (rs2), (rs1), 5, 0x63))
#define rv_bltu(rs1, rs2, imm) rv32_emit32(rv32_b((imm), (rs2), (rs1), 6, 0x63))
#define rv_bgeu(rs1, rs2, imm) rv32_emit32(rv32_b((imm), (rs2), (rs1), 7, 0x63))

#define rv_lb(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 0, (rd), 0x03))
#define rv_lh(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 1, (rd), 0x03))
#define rv_lw(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 2, (rd), 0x03))
#define rv_lbu(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 4, (rd), 0x03))
#define rv_lhu(rd, rs1, imm) rv32_emit32(rv32_i((imm), (rs1), 5, (rd), 0x03))
#define rv_sb(rs2, rs1, imm) rv32_emit32(rv32_s((imm), (rs2), (rs1), 0, 0x23))
#define rv_sh(rs2, rs1, imm) rv32_emit32(rv32_s((imm), (rs2), (rs1), 1, 0x23))
#define rv_sw(rs2, rs1, imm) rv32_emit32(rv32_s((imm), (rs2), (rs1), 2, 0x23))

#define rv_fence_i() rv32_emit32(0x0000100f)
#define rv_ret() rv_jalr(RV_X0, RV_RA, 0)
#define rv_mv(rd, rs) rv_addi((rd), (rs), 0)
#define rv_nop() rv_addi(RV_X0, RV_X0, 0)

static inline bool rv32_imm_fits_i(s32 imm) {
  return imm >= -2048 && imm <= 2047;
}

static inline void rv_li_at(u8 **tptr, u32 rd, uintptr_t value) {
  if (rv32_imm_fits_i((s32)value)) {
    rv32_emit32_at(tptr, rv32_i((s32)value, RV_X0, 0, rd, 0x13));
    return;
  }

  u32 upper = (u32)(value + 0x800U) & 0xfffff000U;
  s32 lower = (s32)((value - upper) & 0xfffU);
  if (lower & 0x800)
    lower |= ~0xfff;

  rv32_emit32_at(tptr, rv32_u(upper, rd, 0x37));
  if (lower)
    rv32_emit32_at(tptr, rv32_i(lower, rd, 0, rd, 0x13));
}

static inline void rv_load_ptr_at(u8 **tptr, u32 rd, const void *ptr) {
  rv_li_at(tptr, rd, (uintptr_t)ptr);
}

static inline void rv_call_ptr_at(u8 **tptr, const void *target) {
  rv_load_ptr_at(tptr, RV_T0, target);
  rv32_emit32_at(tptr, rv32_i(0, RV_T0, 0, RV_RA, 0x67));
}

static inline void rv_jump_ptr_at(u8 **tptr, const void *target) {
  rv_load_ptr_at(tptr, RV_T0, target);
  rv32_emit32_at(tptr, rv32_i(0, RV_T0, 0, RV_X0, 0x67));
}

static inline void rv_tailcall_ptr_at(u8 **tptr, const void *target) {
  rv_jump_ptr_at(tptr, target);
}

static inline void rv_load_u32_abs_at(u8 **tptr, u32 rd, const void *address) {
  rv_load_ptr_at(tptr, RV_T0, address);
  rv32_emit32_at(tptr, rv32_i(0, RV_T0, 2, rd, 0x03));
}

static inline void rv_store_u32_abs_at(u8 **tptr, u32 rs, void *address) {
  rv_load_ptr_at(tptr, RV_T0, address);
  rv32_emit32_at(tptr, rv32_s(0, rs, RV_T0, 2, 0x23));
}

static inline void rv_addi_checked_at(u8 **tptr, u32 rd, u32 rs, s32 imm) {
  if (rv32_imm_fits_i(imm)) {
    rv32_emit32_at(tptr, rv32_i(imm, rs, 0, rd, 0x13));
  } else {
    rv_li_at(tptr, RV_T0, (uintptr_t)imm);
    rv32_emit32_at(tptr, rv32_r(0x00, RV_T0, rs, 0, rd, 0x33));
  }
}

#define rv_li(rd, value) rv_li_at(&translation_ptr, (rd), (uintptr_t)(value))
#define rv_load_ptr(rd, ptr) rv_load_ptr_at(&translation_ptr, (rd), (ptr))
#define rv_call_ptr(target) rv_call_ptr_at(&translation_ptr, (target))
#define rv_jump_ptr(target) rv_jump_ptr_at(&translation_ptr, (target))
#define rv_tailcall_ptr(target) rv_tailcall_ptr_at(&translation_ptr, (target))
#define rv_load_u32_abs(rd, address) rv_load_u32_abs_at(&translation_ptr, (rd), (address))
#define rv_store_u32_abs(rs, address) rv_store_u32_abs_at(&translation_ptr, (rs), (address))
#define rv_addi_checked(rd, rs, imm) rv_addi_checked_at(&translation_ptr, (rd), (rs), (imm))

static inline void rv_patch_jal(u32 *insn, const void *target) {
  s32 imm = (s32)((const u8 *)target - (const u8 *)insn);
  u32 rd = (*insn >> 7) & 0x1f;
  *insn = rv32_j(imm, rd, 0x6f);
}

static inline void rv_patch_branch(u32 *insn, const void *target) {
  s32 imm = (s32)((const u8 *)target - (const u8 *)insn);
  u32 rs1 = (*insn >> 15) & 0x1f;
  u32 rs2 = (*insn >> 20) & 0x1f;
  u32 funct3 = (*insn >> 12) & 0x07;
  *insn = rv32_b(imm, rs2, rs1, funct3, 0x63);
}

#endif
