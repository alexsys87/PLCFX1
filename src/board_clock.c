/**
 * @file    board_clock.c
 * @brief   Clock tree setup (HSE -> PLL -> 72 MHz) with bounded waits and safe fallbacks.
 */
#include "board_clock.h"

#define CLOCK_WAIT_LIMIT 0x20000UL  /* upper bound for every busy-wait loop, in loop iterations */

/**
 * Wait until (*reg & mask) == value.
 * @return false if the condition is not met within CLOCK_WAIT_LIMIT iterations.
 */
static bool wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value) {
    uint32_t timeout = CLOCK_WAIT_LIMIT;
    while ((*reg & mask) != value) {
        if (--timeout == 0U) return false;
    }
    return true;
}

/** Bring the clock tree up (HSE -> PLL -> 72 MHz) with the fallbacks described in board_clock.h. */
bool board_clock_init(void) {
    bool hse_ok;
    uint32_t cfgr;

    /* Two wait states, not the single LATENCY_2 bit, above 48 MHz. */
    FLASH->ACR = FLASH_ACR_PRFTBE | (2UL << FLASH_ACR_LATENCY_Pos);
    RCC->CR |= RCC_CR_HSEON;
    hse_ok = wait_bits(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY);
    cfgr = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 |
           RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_ADCPRE_DIV6;
    if (hse_ok) {
        cfgr |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;   /* 8 MHz HSE * 9 = 72 MHz */
    } else {
        RCC->CR &= ~RCC_CR_HSEON;
        cfgr |= RCC_CFGR_PLLMULL16; /* HSI/2 * 16 = 64 MHz. */
    }
    RCC->CFGR = cfgr;
    RCC->CR |= RCC_CR_PLLON;
    if (!wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) {
        RCC->CR &= ~RCC_CR_PLLON;
        SystemCoreClockUpdate(); /* Stay on reset HSI if PLL fails. */
        return false;
    }
    RCC->CFGR = cfgr | RCC_CFGR_SW_PLL;
    if (!wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL)) {
        /* The switch did not complete: go back to the internal oscillator. */
        RCC->CFGR &= ~RCC_CFGR_SW;
        if (!wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI)) {
            NVIC_SystemReset(); /* Do not use a clock whose source is unknown. */
        }
        RCC->CR &= ~RCC_CR_PLLON;
        SystemCoreClockUpdate();
        return false;
    }
    SystemCoreClockUpdate();
    return hse_ok;
}

/** APB1 timer clock: the bus clock, doubled when the APB1 prescaler divides it (PPRE1 >= 4). */
uint32_t board_apb1_timer_hz(void) {
    uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    uint32_t pclk = SystemCoreClock >> APBPrescTable[ppre];
    /* Timers on a divided APB bus run at twice the bus clock. */
    return ppre < 4U ? pclk : pclk * 2U;
}

/** APB2 bus clock: the system clock divided by the APB2 prescaler. */
uint32_t board_apb2_hz(void) {
    uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    return SystemCoreClock >> APBPrescTable[ppre];
}
