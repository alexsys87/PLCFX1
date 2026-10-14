/**
 * @file plc_exp_guard.h
 * @brief Bounds-checked access to the PLC image for the experimental interpreter.
 *
 * Every RAM access made by an experimental instruction goes through plc_exp_memory():
 * the address must be naturally aligned and lie inside the reserved PLC image or the
 * pulse bitmap, otherwise the PLC stops with PLC_RUNTIME_BOUNDS and the access is
 * redirected to a scratch object. The macros below replace the raw accessors of
 * plc_memory_map.h, so code that includes this header cannot reach peripheral
 * registers or ordinary C variables through a wrong operand.
 *
 * Loop latency is *not* handled here; it is bounded by the per-scan dispatch budget in
 * plc_exp_scan.c. Include this header after plc_memory_map.h and plc_special_devices.h.
 */
#ifndef PLC_EXP_GUARD_H
#define PLC_EXP_GUARD_H

/* Resolved through the include path (not the directory of this header) so that host tests can
 * substitute a mock platform.h for the STM32 device header. */
#include <platform.h>
#include "plc_runtime.h"

/** Latch the first runtime fault and stop the current scan. */
void plc_exp_fault(plc_runtime_error error);

/** Return the program word at the interpreter's program counter (faults when out of range). */
u16 plc_exp_peek_word(void);

/** Validate an access of @p size bytes at @p address; returns the address or a scratch object. */
volatile void *plc_exp_memory(uintptr_t address, u32 size, u32 alignment);

/** Test/set the per-instruction edge-detection bit stored in the pulse bitmap. */
u32 plc_exp_pulse_test(u32 step);
void plc_exp_pulse_put(u32 step, u8 on);

/** Set or clear one bit of the PLC image (M8xxx writes are made atomic). */
void plc_exp_bit_put(u32 bit, u8 on);

/* Replace the raw accessors of plc_memory_map.h by the checked ones. */
#undef PLC_Bu
#undef PLC_Hu
#undef PLC_Wu
#undef PLC_Hs
#undef PLC_Ws
#undef PLC_Du
#undef PLC_Wf
#define PLC_Bu(x) (*(volatile u8 *)plc_exp_memory((uintptr_t)(x), 1U, 1U))
#define PLC_Hu(x) (*(volatile u16 *)plc_exp_memory((uintptr_t)(x), 2U, 2U))
#define PLC_Wu(x) (*(volatile u32 *)plc_exp_memory((uintptr_t)(x), 4U, 4U))
#define PLC_Hs(x) (*(volatile s16 *)plc_exp_memory((uintptr_t)(x), 2U, 2U))
#define PLC_Ws(x) (*(volatile s32 *)plc_exp_memory((uintptr_t)(x), 4U, 4U))
#define PLC_Du(x) (*(volatile int64_t *)plc_exp_memory((uintptr_t)(x), 8U, 8U))
#define PLC_Wf(x) (*(volatile float *)plc_exp_memory((uintptr_t)(x), 4U, 4U))

#endif /* PLC_EXP_GUARD_H */
