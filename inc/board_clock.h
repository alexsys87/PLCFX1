#ifndef PLC_BOARD_CLOCK_H
#define PLC_BOARD_CLOCK_H
#include "platform.h"

/* BluePill: 8 MHz HSE -> 72 MHz. HSI fallback remains usable. */
bool board_clock_init(void);
uint32_t board_apb1_timer_hz(void);
uint32_t board_apb2_hz(void);
#endif
