/**
 * @file platform.h
 * @brief Host replacement for inc/platform.h in the serial protocol tests.
 *
 * Besides the integer names, the protocol code uses the interrupt-mask intrinsics and the
 * USART1 and DMA1 channel 4 registers. This header declares just those registers as plain
 * volatile structures placed at their real addresses; test_protocol.c maps host memory there
 * and plays the part of the peripherals by writing the status registers itself.
 * The structures are cut down to the leading registers that plc_serial_protocol.c uses; their
 * offsets and the bit positions below are those of the real STM32F103 peripherals.
 * tools/test_protocol.py puts this directory in front of inc/ on the include path.
 */
#ifndef PLC_PLATFORM_H
#define PLC_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>

/* Fixed-width integer names used throughout the PLC code. */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

/* Start of the on-chip SRAM; on the target the CMSIS device header provides it. */
#define SRAM_BASE 0x20000000UL

/* Reduced register blocks: USART1, the DMA1 interrupt registers and one DMA channel. */
typedef struct { volatile u32 SR, DR, BRR, CR1; } fake_usart;
typedef struct { volatile u32 ISR, IFCR; } fake_dma;
typedef struct { volatile u32 CCR, CNDTR, CPAR, CMAR; } fake_channel;

/* Peripherals at their real STM32F103 addresses (USART1 and DMA1 channel 4 carry the TX stream). */
#define USART1 ((fake_usart *)0x40013800UL)
#define DMA1 ((fake_dma *)0x40020000UL)
#define DMA1_Channel4 ((fake_channel *)0x40020044UL)

/* USART control bits: receiver, transmitter and receive interrupt enable, USART enable. */
#define USART_CR1_RE (1U << 2)
#define USART_CR1_TE (1U << 3)
#define USART_CR1_RXNEIE (1U << 5)
#define USART_CR1_UE (1U << 13)

/* USART status bits: parity, framing, noise and overrun errors, receive data register not empty. */
#define USART_SR_PE (1U << 0)
#define USART_SR_FE (1U << 1)
#define USART_SR_NE (1U << 2)
#define USART_SR_ORE (1U << 3)
#define USART_SR_RXNE (1U << 5)

/* DMA channel 4: channel enable, transfer complete and transfer error flags, clear-all flag. */
#define DMA_CCR_EN 1U
#define DMA_ISR_TCIF4 (1U << 13)
#define DMA_ISR_TEIF4 (1U << 15)
#define DMA_IFCR_CGIF4 (1U << 12)

/* Data memory barrier: only the compiler barrier is needed on the host. */
#define __DMB() __asm__ volatile ("" ::: "memory")

/* Interrupt-mask intrinsics. The fixture is single-threaded, so only the PRIMASK value is
 * modelled; no interrupt is ever delivered while it is set. */
static u32 fake_primask;
static inline u32 __get_PRIMASK(void) { return fake_primask; }
static inline void __disable_irq(void) { fake_primask = 1U; }
static inline void __set_PRIMASK(u32 value) { fake_primask = value; }
#endif /* PLC_PLATFORM_H */
