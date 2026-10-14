/**
 * @file    board_clock.h
 * @brief   System clock setup for the BluePill board and clock queries for the peripherals.
 */
#ifndef PLC_BOARD_CLOCK_H
#define PLC_BOARD_CLOCK_H
#include "platform.h"

/**
 * Configure the system clock: 8 MHz crystal (HSE) multiplied by the PLL to 72 MHz,
 * AHB /1, APB1 /2, APB2 /1, ADC /6, two flash wait states. Every wait is bounded,
 * so a missing crystal cannot hang the boot.
 *
 * If the crystal does not start, the PLL runs from the internal oscillator
 * instead (HSI/2 * 16 = 64 MHz); if the PLL cannot be used at all the part stays
 * on the 8 MHz internal oscillator.
 *
 * @return true only when the system runs from the crystal at 72 MHz; false after
 *         any fallback. SystemCoreClock reflects the clock actually in use.
 */
bool board_clock_init(void);

/** Clock frequency of the APB1 timers in Hz (twice the APB1 clock when APB1 is divided). */
uint32_t board_apb1_timer_hz(void);

/** Clock frequency of the APB2 peripherals (USART1, GPIOA, ...) in Hz. */
uint32_t board_apb2_hz(void);
#endif
