/**
 * @file    emu_serial.h
 * @brief   COM port of the emulator (one end of a virtual null-modem pair).
 *
 * The port replaces USART1 of the board. Reads never block; writes block until the reply
 * is handed to the driver or the write time-out expires. Every failure and every line
 * error reported by the driver (framing, parity, overrun, break) is logged.
 */
#ifndef EMU_SERIAL_H
#define EMU_SERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Line errors of the bytes returned by the last emu_serial_read() (USART SR bits). */
typedef struct {
  bool framing;
  bool parity;
  bool overrun;
  bool brk;
} emu_serial_errors;

/**
 * Open @p name ("COM5", "\\\\.\\COM12" or a full device path) at @p baud, 8N1, no flow control.
 * @return true on success; the reason is logged on failure.
 */
bool emu_serial_open(const char *name, unsigned long baud);

/** Close the port (no effect when it is not open). */
void emu_serial_close(void);

/** True while the port is open. */
bool emu_serial_is_open(void);

/**
 * Read up to @p size bytes that have already arrived, without waiting.
 * @param errors  receives the line errors the driver reported; may be NULL.
 * @return the number of bytes read (0 when none), or -1 on a read failure.
 */
int emu_serial_read(uint8_t *buffer, size_t size, emu_serial_errors *errors);

/**
 * Write @p length bytes.
 * @return true when all bytes were written; a failure or time-out is logged.
 */
bool emu_serial_write(const uint8_t *data, size_t length);

/**
 * Try to reopen the port after a failure (e.g. the virtual port pair was removed and
 * recreated). Rate-limited to one attempt per second.
 */
bool emu_serial_try_reopen(void);

#endif /* EMU_SERIAL_H */
