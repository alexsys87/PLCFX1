# PLCFX1 — minimal FX1N-style bench PLC

Register-only firmware for **STM32F103C8 BluePill: 64 KB Flash / 20 KB SRAM**.
No HAL or Standard Peripheral Library. The immediate goal is a reproducible
**connect → STOP → upload → verify → RUN → monitor → STOP** bench test.

**This is a small FX1N-style subset, not a drop-in Mitsubishi FX1N replacement.**
GX Developer/GX Works2 connections and their generated machine code have not
been tested. The included Python client and example programs use this project's
inherited wire protocol and documented local instruction encoding. No actual
BluePill or load has been tested. Do not connect machinery or industrial signals.

## First test

See **[the connection guide](docs/connection-test.md)** for wiring, bytecode,
protocol details and recovery after a failed upload.

1. Build/flash MCU firmware through **ST-Link/SWD**, not the programming UART.
2. Connect a **3.3 V TTL** USB UART: adapter RX→PA9, TX→PA10, GND→GND.
3. Use 19200 baud, 8N1, no flow control. Boot is always **STOP**; no RUN jumper.
4. Upload a no-load M0 test, verify Flash, RUN and monitor:

```sh
python3 -m pip install pyserial
python3 tools/plc_link.py --port COM3 identify
python3 tools/plc_link.py --port COM3 smoke examples/always_on_m0.hex --seconds 3
```

On Linux use `/dev/ttyUSB0` instead of `COM3`. `smoke` attempts STOP on exit.
The example is `LD M8000; OUT M0; END`; success includes M0 becoming true.
The second example, `examples/clock_y0.hex`, copies M8013 to Y0/PB0. It needs
no load when monitored over UART. PC13 is the RUN status LED, **not Y0**.

## Interpreter

The previous broad but unsafe interpreter has been replaced with a bounded,
explicit subset. See **[runtime-subset.md](docs/runtime-subset.md)** for exact
commands, encodings and restrictions. Basic contact/coil logic, branch stacks,
PLS/PLF, timers T0..255, ordinary counters C0..199, MOV and INC form the initial
test surface. Unsupported instructions fail to STOP; they do not fall through
to operand words or empty handlers. Scan length, operand ranges and stacks are
checked. Physical outputs are refreshed only after a complete scan and gated
off in STOP. This is not a certified safety mechanism or a hardware watchdog.

The [Mitsubishi instruction tutorial](Tutorial%205%20The%20Mitsubishi%20FX1S%20and%20FX1N%20Instruction%20System%20(V2.0).md)
documents the official model requirements, not the firmware's implemented subset.

## Temporary BluePill pin map

All bench signals below are **3.3 V GPIO**, without PLC isolation/drivers.

| Function | Pins |
|---|---|
| X0..X7, active high, pull-down | PA0..PA7 |
| Y0..Y7, active high | PB0, PB1, PB5, PB6, PB7, PB8, PB9, PB10 |
| USART1 TX / RX | PA9 / PA10 |
| RUN LED, active low | PC13 |
| Optional error LED, active low | PB12 |
| SWD debug/flash | PA13 / PA14 |

Inputs have a default 10 ms debounce and are frozen before each scan. Outputs
are initialized low before GPIO mode changes. RUN/STOP is software-controlled;
PB8 is now Y5, not a RUN input. Do not connect 5 V UART, RS232 voltage levels,
RS422 or 24 V PLC wiring directly. Power the board from one suitable source.

Clock: 8 MHz HSE→72 MHz, with bounded HSI/PLL fallback. UART/TIM2 derive their
rates from the actual clock. TIM2 supplies a 1 ms tick; timer values are updated
in the main execution context. RTC LSE failure does not block boot, and RTC is
not needed for the M8013 test. RTC registers refresh once per second when valid.
CMSIS device files originated in `alexsys87/stm32_rework_station`; notices remain.

## Firmware builds

### IAR

Open **`ewarm/plc_fx1_stm32f103.eww`**, build Debug or Release and flash through
SWD. Native IAR compilation has not been performed in this environment.

### GNU Arm

With `arm-none-eabi-gcc`, newlib and binutils installed:

```sh
python3 tools/build_bluepill.py
```

`build/gcc/plc_fx1n_bluepill.{elf,hex,bin}` are flashable MCU images. The reset
adapter initializes C data/BSS and uses the checked-in STM32F103xB vector list.
Flash the BIN at **0x08000000**, or use the address-bearing Intel HEX. These
firmware files are **not** the raw-byte `.hex` PLC examples accepted by the client.
The older `check_arm_build.py` remains a diagnostic-only linker check.

## Memory and persistence

| Area | Range | Size |
|---|---|---:|
| Firmware | 08000000–0800BFFF | 48 KB |
| PLC header/program | 0800C000–0800F7FF | 14 KB |
| Legacy retained-data store | 0800F800–0800FFFF | 2 KB |
| Globals + 2 KB stack | 20000000–20001BFF | 7 KB |
| Pulse state | 20001C00–20001FFF | 1 KB |
| PLC register image | 20002000–20004FFF | 12 KB |

**Maximum program body: 6000 words**, plus the 92-byte parameter header.
20,000 words do not fit this fixed partition or pulse-state reservation. Do not
assume unofficial extra Flash on clone chips. The first test needs only four words.

The factory firmware image contains the header and an END-only program at C000.
Reflashing can overwrite saved PLC contents; a raw BIN also spans the gap before
the header. Back up any program first. Uploads rewrite Flash in place and are
**not power-loss atomic**; there is no persistent program-image CRC. Boot STOP,
runtime validation and full client readback do not detect every possible corrupted
but syntactically valid program. Re-upload after interruption before RUN.

Automatic retained-data save/restore is **not enabled in this minimal boot path**.
M/D state is volatile across reset. Legacy retention helpers remain independently
tested, but are not a guarantee of power-loss retention.

## Host validation

```sh
python3 tools/check_project.py --preprocess
python3 tools/check_arm_build.py
python3 tools/test_runtime.py
python3 tools/test_protocol.py
python3 tools/test_io.py
python3 tools/test_rtc.py --sanitize
python3 tools/build_bluepill.py
```

Tests cover actual runtime bytecode scans, malformed programs, timer/counter
behavior, protocol framing, Python-client uploads/readback, combined runtime/wire
transactions, modeled GPIO, and RTC calendars. Flash/UART/DMA are modeled in host
tests. Successful builds and tests **do not establish electrical operation,
commercial programming-software compatibility or industrial safety**.
