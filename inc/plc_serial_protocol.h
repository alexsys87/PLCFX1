/**
 * @file    plc_serial_protocol.h
 * @brief   Programming-port link of the PLC (USART1 + DMA) and retained-data storage.
 *
 * GX-style tools and tools/plc_link.py talk to the PLC with framed ASCII
 * commands (STX cmd payload ETX checksum). The frame format and every command
 * are described at the top of src/plc_serial_protocol.c.
 *
 * Execution model: RX_Process() runs in the USART1 interrupt and executes each
 * complete frame at once. The reply is left in tx_data[] and UART_SEND_READY is
 * set; the main loop then calls TX_Process(), which starts the DMA transfer.
 */
#ifndef PLC_SERIAL_PROTOCOL_H
#define PLC_SERIAL_PROTOCOL_H

#include "platform.h"

/** Reply frame. The DMA channel set up in App/main.c reads from this buffer. */
extern char tx_data[144];

/**
 * Set when a reply is waiting in tx_data[]. The main loop must call
 * TX_Process() while it is set; TX_Process() clears it when the DMA transfer
 * starts.
 */
extern volatile u8 UART_SEND_READY;

/**
 * Start the DMA transmission of the pending reply (main loop). Has no effect
 * while a transfer is still running or when there is nothing to send.
 */
void TX_Process(void);

/**
 * Consume one byte received on USART1 (called from the USART1 interrupt) and
 * advance the frame state machine. A complete frame with a valid checksum is
 * executed immediately.
 */
void RX_Process(void);

/**
 * Set the USART1 baud rate and enable the receiver, transmitter and receive
 * interrupt. The firmware configures 19200 baud in App/main.c; this helper
 * changes the rate later.
 * @param baud  bits per second; 0 is ignored.
 */
void usart(u16 baud);

/**
 * Load the factory values of the special registers D8000..D8129, put the
 * relay image into the STOP state and reset the program-download bookkeeping.
 * Called once at boot, before the serial port is enabled.
 */
void PLC_DATA_INIT(void);

/**
 * Back up D500..D949, C100..C149, T100..T149 and M512..M1023 to the retained
 * flash block. Not called by the current firmware; it is reserved for a
 * power-fail hook and covered by the host tests.
 */
void PLC_DATA_KEEP(void);

/**
 * Restore the devices saved by PLC_DATA_KEEP() when the retained block has a
 * valid marker and CRC; otherwise leave them unchanged. Not called by the
 * current firmware.
 */
void PLC_DATA_RECOVER(void);

#endif /* PLC_SERIAL_PROTOCOL_H */
