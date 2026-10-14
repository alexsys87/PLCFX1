/**
 * @file    emu_io.h
 * @brief   Virtual X inputs, Y outputs and status LEDs of the emulated board.
 *
 * Replaces src/plc_io.c (the firmware API is declared in inc/plc_io.h). The "pins" are
 * variables: X0..X7 are set from the console, Y0..Y7 and the RUN/ERR LEDs are reported
 * in the log whenever they change.
 */
#ifndef EMU_IO_H
#define EMU_IO_H

#include "platform.h"

/** Set or toggle the raw level of input X@p index (0..7); it passes the debounce filter. */
void emu_io_set_input(unsigned index, bool level);
void emu_io_toggle_input(unsigned index);

/** Raw input levels (bit n = Xn) and the output pin levels (bit n = Yn). */
u8 emu_io_inputs(void);
u8 emu_io_outputs(void);

/** Update the RUN and ERR LED states (logged on change). */
void emu_io_set_leds(bool run_led, bool err_led);
bool emu_io_run_led(void);
bool emu_io_err_led(void);

#endif /* EMU_IO_H */
