# PLCFX1

Register-only STM32F103C8 **BluePill** firmware implementing a Mitsubishi FX-style
PLC instruction runtime, serial programming protocol, retained data storage and
real-time clock support. No HAL or Standard Peripheral Library is used.

## IAR Embedded Workbench

Open **`ewarm/plc_fx1_stm32f103.eww`**. The project and its debugger/C-STAT
sidecars use the same descriptive name and remain together in `ewarm/`.
Source and include paths are relative to that directory. Debug and Release
share target definitions and include paths; Release retains its optimization
settings. Build products and local IDE state are ignored by Git.

## Hardware and CMSIS

- Device: `STM32F103xB`, STM32F103C8, Cortex-M3, 64 KB Flash / 20 KB SRAM.
- Clock: 8 MHz HSE to 72 MHz. If HSE fails, HSI/2 PLL provides 64 MHz;
  if PLL fails, reset HSI remains active. USART and timer rates follow the
  actual clock rather than assuming 72 MHz.
- USART1: PA9 TX / PA10 RX, initial 19200 baud, 8N1. DMA1 channel 4 handles TX.
  The existing programming handshake can select 9600 baud.
- TIM2: 1 ms PLC tick. JTAG is disabled; **SWD remains available**.
- RUN input: PB8, internal pull-down (disconnected means STOP).
- RUN LED: PC13, active-low BluePill LED. Optional error LED: PB12, active-low.
- RTC: 32.768 kHz LSE required. Startup waits are bounded; LSE failure leaves
  calendar support unavailable rather than hanging indefinitely or pretending
  LSI is accurate. A valid battery-backed counter survives MCU resets.

`CMSIS/device/` contains the STM32F103xB header, IAR startup and system files
from [stm32_rework_station](https://github.com/alexsys87/stm32_rework_station).
Existing Cortex-M3 CMSIS core/compiler support remains in `CMSIS/`.
Vendor copyright/license notices are preserved. `platform.h` only supplies
fixed-width legacy type aliases used by the interpreter.

`App/main.c` initializes GPIO, RCC, DMA, USART and TIM through registers;
`src/board_clock.c`, `src/plc_flash.c` and `src/RTC.C` implement the remaining
hardware support directly through CMSIS register definitions.

## C8 memory layout

| Area | Address range | Size |
|---|---|---|
| Firmware code/constants | `0x08000000–0x0800BFFF` | 48 KB |
| PLC header/program, seven 2 KB blocks | `0x0800C000–0x0800F7FF` | 14 KB |
| Retained PLC data, CRC + completion marker | `0x0800F800–0x0800FFFF` | 2 KB |
| Runtime globals, stack and heap | `0x20000000–0x20001BFF` | 7 KB |
| Program pulse-state bitmap | `0x20001C00–0x20001FFF` | 1 KB |
| PLC registers | `0x20002000–0x20004FFF` | 12 KB |

The IAR linker file is `ewarm/bluepill_plc.icf`. Stack reservation is 2 KB;
heap reservation is zero. Fixed PLC RAM areas are excluded from C runtime
initialization and explicitly cleared at startup.

The factory PLC header is part of the firmware download image at C000.
Reflashing firmware can therefore replace that PLC storage page. Mass erase
is disabled in the project, but preservation of other program/retained pages
still depends on the actual programmer; back up PLC contents before flashing.

**Maximum program size is 6000 steps**, reduced from 8000 to fit guaranteed
C8 Flash. Ordinary data registers are D0000–D1999. Do not depend on unofficial
128 KB Flash found in some BluePill clones. Each 2 KB storage update erases
both of its 1 KB pages; writes cannot target firmware or exceed C8 Flash.
The historical retained-block identifier 10 is mapped to the final 2 KB, not
to an out-of-range physical block. Retained records require a valid CRC and
completion marker before recovery. Flash writes pause interrupts and are not
power-fail atomic; an interrupted retained update can lose the previous backup.
The board still needs a real power-fail signal and sufficient energy reserve
if `PLC_DATA_KEEP()` is to be called automatically at power loss.

## Validation

```sh
python3 tools/check_project.py
python3 tools/check_project.py --preprocess
python3 tools/check_arm_build.py
python3 tools/test_rtc.py --sanitize
python3 tools/test_protocol.py
```

The dependency check uses GCC; the ARM check requires `arm-none-eabi-gcc`
with newlib. It compiles every translation unit **as C**, including `.C` files,
and links a temporary diagnostic image with the C8 memory reservations and a
vector adapter derived from the IAR startup table. It is **not flashable** and
does not validate IAR assembly/runtime initialization. Build Debug and Release
in IAR and inspect their linker maps before flashing.

Host RTC tests check 44,195 Gregorian dates and modeled register failures.
Host protocol tests use ASan/UBSan and cover 141 framed cases plus receive
overflow, DMA ownership and retained-data staging. Flash is stubbed in those
tests, not electrically emulated. Programming is stop-and-wait: bytes arriving
while a response owns the DMA buffer are dropped; retry with a fresh STX.

## Remaining limitations

Board input filtering, output refresh and I/O configuration were already
disabled in the original source. Their called hooks remain explicitly empty;
this register port does not invent an external PLC I/O circuit or pin mapping.
The original interpreter also contains incomplete/weak instruction handlers
and is not a fully audited or memory-safe execution engine. Protocol boundary
checks are not a substitute for validating ladder programs and device indexes.
GNU compilation still reports legacy interpreter warnings, including potentially
uninitialized values and signed comparisons; this port does not claim to resolve
every instruction-runtime defect.
Use only supported programs within the documented memory limits. Verify clock
failure, UART programming, page rewrite, retention, RTC and tick timing on the
actual board; no physical BluePill test or native IAR build has been performed.
