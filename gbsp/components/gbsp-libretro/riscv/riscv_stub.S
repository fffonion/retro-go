/*
 * gpSP RISC-V dynarec ABI stub.
 *
 * This file is intentionally not wired into the default ESP32-S31 build yet.
 * The C fallback remains active until RISCV_NATIVE_DYNAREC has a complete
 * emitter macro surface. Keep this file assembling while the backend grows.
 */

  .text
  .align 2

  .equ REG_PC,         (15 * 4)
  .equ REG_CPSR,       (16 * 4)
  .equ CPU_HALT_STATE, (18 * 4)

  .global execute_arm_translate_internal
  .type execute_arm_translate_internal, @function

/*
 * a0: cycle counter
 * a1: &reg[0]
 *
 * s0 keeps reg base and s1 keeps cycle counter for generated code. Native
 * blocks are expected to tail-jump back to riscv_lookup_pc or
 * riscv_return_to_main once their epilogue exists.
 */
execute_arm_translate_internal:
  addi sp, sp, -32
  sw ra, 28(sp)
  sw s0, 24(sp)
  sw s1, 20(sp)
  sw s2, 16(sp)
  sw s3, 12(sp)
  sw s4, 8(sp)
  sw s5, 4(sp)
  sw s6, 0(sp)

  mv s0, a1
  mv s1, a0

  lw t0, CPU_HALT_STATE(s0)
  bnez t0, riscv_fallback_interpreter

riscv_lookup_pc:
  lw a0, REG_PC(s0)
  lw t0, REG_CPSR(s0)
  andi t0, t0, 0x20
  beqz t0, 1f
  call block_lookup_address_thumb
  jr a0
1:
  call block_lookup_address_arm
  jr a0

riscv_fallback_interpreter:
  mv a0, s1
  call execute_arm
  li a0, 0

riscv_return_to_main:
  lw s6, 0(sp)
  lw s5, 4(sp)
  lw s4, 8(sp)
  lw s3, 12(sp)
  lw s2, 16(sp)
  lw s1, 20(sp)
  lw s0, 24(sp)
  lw ra, 28(sp)
  addi sp, sp, 32
  ret

  .size execute_arm_translate_internal, .-execute_arm_translate_internal
