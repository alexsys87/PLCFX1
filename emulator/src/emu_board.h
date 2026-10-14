/**
 * @file    emu_board.h
 * @brief   Emulated core services and the firmware entry points the emulator drives.
 */
#ifndef EMU_BOARD_H
#define EMU_BOARD_H

#include "platform.h"

/* Interrupt handlers and globals of the firmware that have no prototype in a header. */
void USART1_IRQHandler(void);
void DMA1_Channel4_IRQHandler(void);
extern char tx_data[144];
extern volatile u16 PLC_RUN_TIME;

/** True when PRIMASK is clear, i.e. an emulated interrupt may be delivered now. */
bool emu_irq_enabled(void);

/**
 * Called by the main loop after each firmware call: PRIMASK must be clear again. A set
 * PRIMASK means the firmware left a critical section open; this is logged as an error
 * (naming @p where) and repaired so that the emulator keeps running.
 */
void emu_check_primask(const char *where);

#endif /* EMU_BOARD_H */
