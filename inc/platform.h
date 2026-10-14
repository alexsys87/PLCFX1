/**
 * @file    platform.h
 * @brief   Target platform header: CMSIS device header and the integer names used by the PLC code.
 *
 * Host tests substitute their own platform.h (see tools/tests/runtime and
 * tools/tests/protocol) so that the PLC sources can run on a PC without the
 * STM32 device header.
 */
#ifndef PLC_PLATFORM_H
#define PLC_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx.h"

/* Fixed-width names retained by the FX instruction interpreter. */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#ifndef TRUE
#define TRUE true
#define FALSE false
#endif

#endif /* PLC_PLATFORM_H */
