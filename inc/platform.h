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
