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

## Current implementation status

- `riscv_codegen.h` has RV32I/RV32M encoders plus helper macros for loading
  32-bit constants, absolute C calls, tail calls, and absolute global u32
  loads/stores.
- `cpu_threaded.c` keeps the `RISCV_ARCH` path behind a fallback interpreter
  bridge while the full macro surface is being ported. This lets GBA builds
  stay valid on ESP32-S31 during incremental backend work.
- `RISCV_NATIVE_DYNAREC` can be defined manually for syntax probes. The
  translator now reaches the RISC-V macro surface; unsupported instruction
  families emit an abort call instead of silently generating wrong code.
- `riscv_stub.S` contains the first ABI stub for `execute_arm_translate_internal`
  and is now compiled as part of the component. Native execution is enabled only
  when the build environment defines `GBSP_RISCV_NATIVE_DYNAREC=1`.
- Direct ARM/Thumb branch placeholders now use real RV32 patchable branch
  emission. ARM `B/BL`, Thumb `B/BL`, Thumb PC-relative load, Thumb literal
  pool load, Thumb SP-relative add, Thumb SP adjust, Thumb high-register
  `ADD/MOV`, Thumb conditional branches, Thumb `ADD/SUB/CMP/CMN`, Thumb
  logical `MOV/AND/EOR/ORR/TST/BIC/MVN`, Thumb `MULS/NEG`, and Thumb immediate
  `LSL/LSR/ASR` have first-pass RV32 emission. Thumb single load/store
  instructions now route through the existing C memory helpers, and ARM/Thumb
  indirect branch entry points have a first-pass lookup/tail-jump path.
- Thumb register-shift `LSL/LSR/ASR/ROR`, `ADC/SBC`, block memory
  `PUSH/POP/LDM/STM`, and `SWI/HLE div` are covered by first-pass helper paths.
- ARM instructions not yet inlined use a one-instruction interpreter helper,
  which lets native builds link while the hot ARM emitters are ported
  incrementally.
- The RISC-V backend currently uses memory-backed GBA register accesses. This
  avoids cross-block register cache hazards until a full ABI cache protocol is
  added.
- The ESP target uses executable internal memory for JIT cache allocation and
  the ESP cache maintenance path in `platform_cache_sync`.
- Large GBA sprite priority scratch buffers are placed in external BSS on the
  ESP32-S31 target to keep internal DRAM below the linker limit.

## Remaining backend work

- Replace helper fallback paths with direct RV32 emitters for speed, starting
  with ARM ALU/load-store and Thumb register shifts/block memory.
- Add a cross-block register cache ABI and re-enable host-register allocation
  once entry/exit writeback rules are complete.
- Run on hardware with `GBSP_RISCV_NATIVE_DYNAREC=1`, collect the first crash
  PC if any, then tighten the helper paths into direct emitters.

## Notes

- The upstream gpSP backend set does not currently include RISC-V, so this port
  should be kept behind `RISCV_ARCH` until the translation macros are complete.
- RV32 conditional branches have a +/-4 KiB range and `jal` has a +/-1 MiB
  range. Long exits need an indirect jump sequence.
- Generated code must use the platform cache sync path in `cpu_threaded.c`
  before execution.
