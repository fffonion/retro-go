# gpSP RISC-V Backend Notes

## ESP32-S31 host profile

- Host ISA: 32-bit RISC-V, dual core, up to 320 MHz.
- Useful CPU features exposed by ESP-IDF SoC caps: FPU, hardware loop, PIE,
  SIMD instructions, PMP/PMA, configurable MMU pages.
- JIT memory: allocate internal executable RAM with `MALLOC_CAP_EXEC`.
- Cache maintenance: write generated code back as data, invalidate the matching
  instruction-cache range, then call the compiler cache clear builtin.
- Preferred SIMD data alignment: 16 bytes.

## Bring-up order

1. Keep emitted code in internal executable RAM; avoid PSRAM for translated
   blocks until call latency and cache behavior are measured.
2. Implement the RV32IM integer backend first. ARM7TDMI translation is mostly
   integer ALU, branches, loads, stores, and multiply.
3. Add ABI glue for generated blocks: prologue, epilogue, cycle update,
   block exits, and C helper calls.
4. Port ARM and Thumb ALU/data-transfer emit macros from the existing ARM/MIPS
   backends into RV32 helpers.
5. After correctness tests pass, use S31 SIMD/HW-loop features only for hot
   batched routines such as pixel conversion and audio mixing.

## Notes

- The upstream gpSP backend set does not currently include RISC-V, so this port
  should be kept behind `RISCV_ARCH` until the translation macros are complete.
- RV32 conditional branches have a +/-4 KiB range and `jal` has a +/-1 MiB
  range. Long exits need an indirect jump sequence.
- Generated code must use the platform cache sync path in `cpu_threaded.c`
  before execution.
