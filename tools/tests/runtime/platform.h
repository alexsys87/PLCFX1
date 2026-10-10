#ifndef PLC_PLATFORM_H
#define PLC_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
#define SRAM_BASE 0x20000000UL
u32 __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(u32 value);
#endif
