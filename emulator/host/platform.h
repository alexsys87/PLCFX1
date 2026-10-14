/**
 * @file    platform.h
 * @brief   Windows host replacement for inc/platform.h used by the PLC emulator.
 *
 * The emulator compiles the real firmware sources (src/plc_serial_protocol.c and the
 * selected interpreter profile) on a PC. This header stands in for the CMSIS device
 * header: it provides the integer names, the SRAM base address, the interrupt-mask
 * intrinsics and the few USART1/DMA1 registers that the protocol code touches.
 *
 * The project force-includes this file (/FI) into every translation unit. inc/platform.h
 * uses the same include guard, so when a firmware header later includes "platform.h"
 * from the inc/ directory, that STM32 version is skipped.
 *
 * Peripheral registers are ordinary C objects owned by the emulator (emu_board.c); only
 * the PLC register image and the program flash live at their real fixed addresses
 * (see emu_memory.c).
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

#ifndef TRUE
#define TRUE true
#define FALSE false
#endif

/* Start of the on-chip SRAM; the PLC image is mapped at SRAM_BASE + 0x2000 by the emulator. */
#define SRAM_BASE 0x20000000UL

/* MSVC knows no GCC attributes. The firmware uses them only to place PLC_ROM in a
 * linker section, which has no meaning on the host. */
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x)
#endif

/* ---- Peripheral registers used by plc_serial_protocol.c --------------------------------- */

/** USART register block, cut down to the registers the protocol code uses. */
typedef struct { volatile u32 SR, DR, BRR, CR1, CR2, CR3, GTPR; } emu_usart_regs;
/** DMA interrupt status and flag-clear registers. */
typedef struct { volatile u32 ISR, IFCR; } emu_dma_regs;
/** One DMA channel. */
typedef struct { volatile u32 CCR, CNDTR, CPAR, CMAR; } emu_dma_channel_regs;

extern emu_usart_regs emu_usart1;
extern emu_dma_regs emu_dma1;
extern emu_dma_channel_regs emu_dma1_channel4;

#define USART1 (&emu_usart1)
#define DMA1 (&emu_dma1)
#define DMA1_Channel4 (&emu_dma1_channel4)

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

/* ---- Core intrinsics --------------------------------------------------------------------- */

/* Data memory barrier. The emulator is single-threaded, so a compiler barrier is enough. */
#if defined(_MSC_VER) && !defined(__clang__)
void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#define __DMB() _ReadWriteBarrier()
#else
#define __DMB() __asm__ volatile ("" ::: "memory")
#endif

/*
 * Interrupt-mask intrinsics (emu_board.c). The emulator delivers "interrupts" (serial
 * bytes, DMA completion, the 1 ms tick) only between main-loop steps, so PRIMASK is just
 * recorded; the emulator logs an error if a critical section is left unbalanced.
 */
u32 __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(u32 value);

#endif /* PLC_PLATFORM_H */
