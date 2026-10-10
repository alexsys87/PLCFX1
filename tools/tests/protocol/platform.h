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
typedef struct { volatile u32 SR, DR, BRR, CR1; } fake_usart;
typedef struct { volatile u32 ISR, IFCR; } fake_dma;
typedef struct { volatile u32 CCR, CNDTR, CPAR, CMAR; } fake_channel;
#define USART1 ((fake_usart *)0x40013800UL)
#define DMA1 ((fake_dma *)0x40020000UL)
#define DMA1_Channel4 ((fake_channel *)0x40020044UL)
#define USART_CR1_RE (1U << 2)
#define USART_CR1_TE (1U << 3)
#define USART_CR1_RXNEIE (1U << 5)
#define USART_CR1_UE (1U << 13)
#define USART_SR_PE (1U << 0)
#define USART_SR_FE (1U << 1)
#define USART_SR_NE (1U << 2)
#define USART_SR_ORE (1U << 3)
#define USART_SR_RXNE (1U << 5)
#define DMA_CCR_EN 1U
#define DMA_ISR_TCIF4 (1U << 13)
#define DMA_ISR_TEIF4 (1U << 15)
#define DMA_IFCR_CGIF4 (1U << 12)
#define __DMB() __asm__ volatile ("" ::: "memory")
#endif
