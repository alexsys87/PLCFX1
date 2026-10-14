/**
 * @file    emu_io.c
 * @brief   Host version of src/plc_io.c: debounced virtual inputs and virtual outputs.
 *
 * The scan-related behaviour is the same as on the board: inputs are debounced every
 * millisecond, frozen into the X image once per scan, and outputs follow the Y image only
 * while the PLC runs (RUN requested, M8000 on, M8001 off).
 */
#include "emu_io.h"

#include "emu_log.h"
#include "plc_io.h"
#include "plc_memory_map.h"
#include "plc_runtime.h"
#include "plc_special_devices.h"

u16 X_DIY = 10U; /* debounce time in 1 ms ticks */

static u8 raw_inputs;            /* levels on the virtual X pins */
static volatile u8 stable_inputs;
static u8 input_age[8];
static u8 output_pins;           /* levels on the virtual Y pins */
static bool outputs_known;
static bool run_led, err_led, leds_known;

void emu_io_set_input(unsigned index, bool level) {
  if (index > 7U) return;
  if (level) raw_inputs |= (u8)(1U << index);
  else raw_inputs &= (u8)~(1U << index);
  LOG_INFO(EMU_CAT_IO, "X%u := %u", index, level ? 1U : 0U);
}

void emu_io_toggle_input(unsigned index) {
  if (index <= 7U) emu_io_set_input(index, !(raw_inputs & (1U << index)));
}

u8 emu_io_inputs(void) { return raw_inputs; }
u8 emu_io_outputs(void) { return output_pins; }

void emu_io_set_leds(bool run, bool err) {
  if (leds_known && run == run_led && err == err_led) return;
  if (!leds_known || run != run_led) LOG_INFO(EMU_CAT_IO, "RUN LED %s", run ? "ON" : "OFF");
  if (!leds_known || err != err_led) {
    if (err) LOG_WARN(EMU_CAT_IO, "ERR LED ON (a PLC fault is latched)");
    else LOG_INFO(EMU_CAT_IO, "ERR LED OFF");
  }
  run_led = run;
  err_led = err;
  leds_known = true;
}

bool emu_io_run_led(void) { return run_led; }
bool emu_io_err_led(void) { return err_led; }

/* ---- Firmware API (inc/plc_io.h) ------------------------------------------------------------ */

void X_filter(void) {
  u8 i;
  u16 delay = X_DIY;

  if (delay > 255U) delay = 255U;
  for (i = 0; i < 8U; i++) {
    u8 mask = (u8)(1U << i);
    if ((raw_inputs & mask) == (stable_inputs & mask)) {
      input_age[i] = 0U;
    } else if (delay == 0U || ++input_age[i] >= delay) {
      stable_inputs ^= mask;
      input_age[i] = 0U;
    }
  }
}

void PLC_IO_SampleInputs(void) { PLC_8BIT(Addr_X) = stable_inputs; }

void PLC_IO_Refresh(void) {
  u8 outputs = 0U, changed;
  unsigned i;

  if (PLC_Runtime_RunRequested() && (PLC_8BIT(Addr_M8) & 3U) == 1U) outputs = PLC_8BIT(Addr_Y);
  changed = (u8)(outputs ^ output_pins);
  if (!outputs_known) {
    LOG_DEBUG(EMU_CAT_IO, "Outputs initialised: Y7..Y0 = %02X", outputs);
  } else if (changed) {
    for (i = 0; i < 8U; i++)
      if (changed & (1U << i))
        LOG_TRACE(EMU_CAT_IO, "Y%u -> %u", i, (outputs >> i) & 1U);
  }
  output_pins = outputs;
  outputs_known = true;
}

void PLC_IO_config(void) {
  u8 i;
  raw_inputs = 0U;
  stable_inputs = 0U;
  for (i = 0; i < 8U; i++) input_age[i] = 0U;
  output_pins = 0U;
  outputs_known = false;
  PLC_8BIT(Addr_X) = 0U;
  PLC_8BIT(Addr_Y) = 0U;
}
