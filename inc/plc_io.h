/**
 * @file plc_io.h
 * @brief Board I/O of the BluePill PLC: X inputs, Y outputs, status LEDs and the I/O scan API.
 *
 * Bench hardware only: all pins are 3.3 V logic. Never connect 24 V PLC wiring directly.
 *
 *   X0..X7  PA0..PA7, inputs with pull-down, active high, debounced in software
 *   Y0..Y7  PB0, PB1, PB5, PB6, PB7, PB8, PB9, PB10, push-pull outputs (2 MHz)
 *   RUN LED PC13 (BluePill LED, active low), error LED PB12 (optional, active low)
 *
 * RUN/STOP is commanded through the programming protocol; there is no RUN jumper.
 */
#ifndef PLC_IO_H
#define PLC_IO_H

#include "platform.h"

/* ---- Bit-band access to single GPIO pins ------------------------------------------------ */

/** Bit-band alias of bit @p bitnum of the peripheral register at offset @p addr from 0x40000000. */
#define BIT_ADDR(addr, bitnum) \
  (*((volatile unsigned long *)(0x42000000 + ((addr) << 5) + ((bitnum) << 2))))

/* Peripheral offsets (from 0x40000000) of the GPIO output and input data registers. */
#define GPIOA_ODR_Addr 0x01080C
#define GPIOB_ODR_Addr 0x010C0C
#define GPIOC_ODR_Addr 0x01100C
#define GPIOD_ODR_Addr 0x01140C
#define GPIOA_IDR_Addr 0x010808
#define GPIOB_IDR_Addr 0x010C08
#define GPIOC_IDR_Addr 0x011008
#define GPIOD_IDR_Addr 0x011408

/* Single-pin output (Pxout) and input (Pxin) access; @p n must be below 16. */
#define PAout(n) BIT_ADDR(GPIOA_ODR_Addr, n)
#define PAin(n)  BIT_ADDR(GPIOA_IDR_Addr, n)
#define PBout(n) BIT_ADDR(GPIOB_ODR_Addr, n)
#define PBin(n)  BIT_ADDR(GPIOB_IDR_Addr, n)
#define PCout(n) BIT_ADDR(GPIOC_ODR_Addr, n)
#define PCin(n)  BIT_ADDR(GPIOC_IDR_Addr, n)
#define PDout(n) BIT_ADDR(GPIOD_ODR_Addr, n)
#define PDin(n)  BIT_ADDR(GPIOD_IDR_Addr, n)

/* ---- Board signals ---------------------------------------------------------------------- */

#define PLC_RUN 1U                 /**< RUN is requested by the protocol, not by a pin */
#define PVD     PAin(0)

#define RUN_LED_ON  PCout(13) = 0  /**< BluePill LED is active low */
#define RUN_LED_OFF PCout(13) = 1
#define ERR_LED_ON  PBout(12) = 0
#define ERR_LED_OFF PBout(12) = 1

/* X inputs (X0..X7). Extend as needed. */
#define X00 PAin(0)
#define X01 PAin(1)
#define X02 PAin(2)
#define X03 PAin(3)
#define X04 PAin(4)
#define X05 PAin(5)
#define X06 PAin(6)
#define X07 PAin(7)

/* Y outputs (Y0..Y7). Extend as needed. */
#define Y00 PBout(0)
#define Y01 PBout(1)
#define Y02 PBout(5)
#define Y03 PBout(6)
#define Y04 PBout(7)
#define Y05 PBout(8)
#define Y06 PBout(9)
#define Y07 PBout(10)

/* ---- I/O scan API (implemented in plc_io.c) --------------------------------------------- */

/** Input debounce time in 1 ms ticks (0 disables filtering, values above 255 are clamped). */
extern u16 X_DIY;

/** Debounce step; call every 1 ms (timer interrupt). */
void X_filter(void);

/** Configure the GPIO pins and clear the X/Y images. Outputs are low before they become outputs. */
void PLC_IO_config(void);

/** Latch the debounced inputs into the X image; call once at the start of every scan. */
void PLC_IO_SampleInputs(void);

/** Drive the output pins from the Y image, or force them low unless the PLC is running. */
void PLC_IO_Refresh(void);

#endif /* PLC_IO_H */
