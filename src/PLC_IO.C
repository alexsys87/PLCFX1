#include "plc_io.h"
#include "PLC_Dialogue.h"
#include "PLC_CONF.H"
#include "plc_runtime.h"

/* Bench-only 3.3 V GPIO. Never connect 24 V PLC wiring directly. */
u16 X_DIY = 10U;
static volatile uint8_t stable_inputs;
static uint8_t input_age[8];
static const uint8_t output_pins[8] = {0, 1, 5, 6, 7, 8, 9, 10};

void X_filter(void) {
    uint8_t sample = (uint8_t)GPIOA->IDR;
    uint8_t i;
    uint16_t delay = X_DIY;
    if (delay > 255U) delay = 255U;
    for (i = 0; i < 8U; i++) {
        uint8_t mask = (uint8_t)(1U << i);
        if ((sample & mask) == (stable_inputs & mask)) input_age[i] = 0U;
        else if (delay == 0U || ++input_age[i] >= delay) {
            stable_inputs ^= mask;
            input_age[i] = 0U;
        }
    }
}

void PLC_IO_SampleInputs(void) {
    /* Freeze physical inputs for the next complete scan, not in the IRQ. */
    PLC_8BIT(Addr_X) = stable_inputs;
}

void PLC_IO_Refresh(void) {
    uint32_t on = 0U, off = 0U;
    uint8_t i, outputs = 0U;
    /* A software STOP also gates hardware immediately, before another scan. */
    if (PLC_Runtime_RunRequested() && (PLC_8BIT(Addr_M8) & 3U) == 1U)
        outputs = PLC_8BIT(Addr_Y);
    for (i = 0; i < 8U; i++) {
        uint32_t mask = 1UL << output_pins[i];
        if (outputs & (1U << i)) on |= mask;
        else off |= mask;
    }
    GPIOB->BSRR = on | (off << 16);
}

void PLC_IO_config(void) {
    uint8_t i;
    uint32_t mask = 0U;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    (void)RCC->APB2ENR;
    /* X0..X7: PA0..PA7, input pull-down, active high. */
    GPIOA->BRR = 0xFFU;
    GPIOA->CRL = 0x88888888UL;
    for (i = 0; i < 8U; i++) mask |= 1UL << output_pins[i];
    GPIOB->BRR = mask;
    for (i = 0; i < 8U; i++) {
        uint32_t shift = (output_pins[i] & 7U) * 4U;
        if (output_pins[i] < 8U)
            GPIOB->CRL = (GPIOB->CRL & ~(0xFUL << shift)) | (0x2UL << shift);
        else
            GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << shift)) | (0x2UL << shift);
    }
    stable_inputs = 0U;
    for (i = 0; i < 8U; i++) input_age[i] = 0U;
    PLC_8BIT(Addr_X) = 0U;
    PLC_8BIT(Addr_Y) = 0U;
}
