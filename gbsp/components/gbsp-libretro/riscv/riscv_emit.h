/* gameplaySP RISC-V backend entry point.
 *
 * ESP32-S31 is RV32 with separate data/instruction cache maintenance through
 * ESP-IDF. This header is the place where cpu_threaded.c will bind the ARM and
 * Thumb translation macros to RV32 emission helpers.
 */

#ifndef RISCV_EMIT_H
#define RISCV_EMIT_H

#include "riscv_codegen.h"

#define RISCV_BACKEND_IN_PROGRESS 1

#endif
