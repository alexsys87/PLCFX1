/**
 * @file platform.h
 * @brief Host replacement for inc/platform.h in the runtime tests.
 *
 * tools/test_runtime.py and tools/test_experimental.py put this directory in front of inc/ on
 * the include path, so the interpreter compiles on a PC without the CMSIS device header. Only
 * what the PLC sources use is provided: the integer names, the SRAM base address and the
 * interrupt-mask intrinsics. The intrinsics are only declared here; each test defines them as
 * mocks that model the PRIMASK bit and check that every critical section is balanced.
 */
#ifndef PLC_PLATFORM_H
#define PLC_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>

/* Fixed-width integer names used throughout the PLC code. */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

/* Start of the on-chip SRAM; on the target the CMSIS device header provides it. */
#define SRAM_BASE 0x20000000UL

/* Interrupt-mask intrinsics (CMSIS on the target): read PRIMASK, mask interrupts, restore PRIMASK. */
u32 __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(u32 value);
#endif /* PLC_PLATFORM_H */
