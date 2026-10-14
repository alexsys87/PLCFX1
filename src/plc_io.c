/**
 * @file plc_io.c
 * @brief GPIO driver: debounced X inputs, Y outputs and their scan-synchronised images.
 *
 * Inputs are debounced every millisecond (X_filter, from the timer interrupt) but are copied
 * into the PLC image only once per scan (PLC_IO_SampleInputs) so that a scan sees a stable
 * snapshot. Outputs follow the Y image only while the PLC is in RUN; a STOP request gates the
 * pins immediately, before another scan has run.
 *
 * Bench-only 3.3 V GPIO. Never connect 24 V PLC wiring directly.
 */
#include "plc_io.h"
#include "plc_memory_map.h"
#include "plc_special_devices.h"
#include "plc_runtime.h"

u16 X_DIY = 10U;                                   /* debounce time in 1 ms ticks */

static volatile uint8_t stable_inputs;             /* debounced X0..X7 */
static uint8_t input_age[8];                       /* ticks the raw level has differed from the stable one */
static const uint8_t output_pins[8] = {0, 1, 5, 6, 7, 8, 9, 10};  /* Y0..Y7 on GPIOB pin numbers */

/**
 * Debounce step, called every millisecond: a changed input level becomes the stable level once it
 * has differed for X_DIY consecutive ticks (at once when X_DIY is 0).
 */
void X_filter(void)
{
    uint8_t sample = (uint8_t)GPIOA->IDR;
    uint8_t i;
    uint16_t delay = X_DIY;

    if (delay > 255U) delay = 255U;
    for (i = 0; i < 8U; i++) {
        uint8_t mask = (uint8_t)(1U << i);

        if ((sample & mask) == (stable_inputs & mask)) {
            input_age[i] = 0U;                     /* raw level agrees: restart the debounce timer */
        } else if (delay == 0U || ++input_age[i] >= delay) {
            stable_inputs ^= mask;                 /* the new level lasted long enough: accept it */
            input_age[i] = 0U;
        }
    }
}

/** Latch the debounced inputs into the X image; called once per scan. */
void PLC_IO_SampleInputs(void)
{
    /* Freeze physical inputs for the next complete scan, not in the IRQ. */
    PLC_8BIT(Addr_X) = stable_inputs;
}

/**
 * Drive Y0..Y7 from the Y image while the PLC runs (RUN requested, M8000 on and M8001 off);
 * in every other state all output pins are driven low.
 */
void PLC_IO_Refresh(void)
{
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
    GPIOB->BSRR = on | (off << 16);                /* one write sets and resets all pins atomically */
}

/**
 * Configure the X inputs (pull-down) and the Y outputs (2 MHz push-pull, driven low first) and
 * clear the debounce state and the X/Y images.
 */
void PLC_IO_config(void)
{
    uint8_t i;
    uint32_t mask = 0U;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    (void)RCC->APB2ENR;                            /* read back: wait for the clock to start */

    /* X0..X7: PA0..PA7, input pull-down, active high. */
    GPIOA->BRR = 0xFFU;
    GPIOA->CRL = 0x88888888UL;

    /* Y0..Y7: drive the pins low first, then switch them to 2 MHz push-pull outputs. */
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
