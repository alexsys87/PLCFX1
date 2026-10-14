/**
 * @file    emu_protocol.h
 * @brief   Observer of the programming-port traffic: tracing and NAK diagnosis.
 *
 * The firmware answers every bad request with a bare NAK. The observer follows the same
 * frame state machine as RX_Process(), keeps the last complete request and, when the reply
 * is a NAK, writes the request and the most likely reason to the log (bad checksum,
 * unknown command, address out of range, protected byte, flash failure, ...). It also
 * reports noise outside frames, frames restarted by a new STX and oversized frames.
 */
#ifndef EMU_PROTOCOL_H
#define EMU_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Log every request and reply at TRACE level (also needs the TRACE log level). */
void emu_proto_set_trace(bool on);
bool emu_proto_get_trace(void);

/** Observe one received byte before it is passed to the firmware. */
void emu_proto_on_rx_byte(uint8_t byte);

/** A line error (framing/parity/overrun) destroyed the frame in progress. */
void emu_proto_on_line_error(void);

/** Observe a reply that the firmware sends. */
void emu_proto_on_tx(const uint8_t *data, size_t length);

/** Report pending noise bytes (called periodically by the main loop). */
void emu_proto_flush_noise(void);

/** Counters for the status display. */
unsigned long emu_proto_requests(void);
unsigned long emu_proto_naks(void);

#endif /* EMU_PROTOCOL_H */
