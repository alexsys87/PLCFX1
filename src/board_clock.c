#include "board_clock.h"

#define CLOCK_WAIT_LIMIT 0x20000UL

static bool wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value) {
    uint32_t timeout = CLOCK_WAIT_LIMIT;
    while ((*reg & mask) != value) {
        if (--timeout == 0U) return false;
    }
    return true;
}

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
        cfgr |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
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

uint32_t board_apb1_timer_hz(void) {
    uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    uint32_t pclk = SystemCoreClock >> APBPrescTable[ppre];
    return ppre < 4U ? pclk : pclk * 2U;
}

uint32_t board_apb2_hz(void) {
    uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    return SystemCoreClock >> APBPrescTable[ppre];
}
