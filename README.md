# PLCFX1

STM32F103 firmware implementing a Mitsubishi FX-style PLC instruction runtime,
serial programming protocol, retained data storage and real-time clock support.

## IAR Embedded Workbench

Open **`ewarm/plc_fx1_stm32f103.eww`**. The project and its debugger/C-STAT
sidecars use the same descriptive name and remain together in `ewarm/`.
Source and include paths are relative to that directory. Debug and Release
share target definitions and include paths; Release retains its optimization
settings. Build products and local IDE state are ignored by Git.

## Layout

- `App/`: startup application and timer/USART1 initialization.
- `src/`, `inc/`: PLC interpreter, communication, RTC and board I/O hooks.
- `ST/`: STM32 device definitions, system clock initialization and startup.
- `CMSIS/`: Cortex-M3 support and compiler backends.
- `STM32F10x_StdPeriph_Driver/`: required DMA, Flash, GPIO, RCC and timer modules;
  the RTC header supplies register constants used by the application.
- `tools/check_project.py`: dependency/path and comment validation.

Unused display drivers, display tests, unconfigured Modbus/USART3 code,
standalone examples, duplicate instruction fragments and unrelated CMSIS cores
have been removed. Instruction handlers referenced by dispatch tables remain.
Vendor notices and relevant instruction documentation are retained.

## Validation

```sh
python3 tools/check_project.py
python3 tools/check_project.py --preprocess
```

The second command requires GCC and checks all C include dependencies in both
configurations. It does **not** compile or link with IAR or test the hardware.

## Existing hardware limitations

The original IAR target/linker selects **STM32F103C8**, while the firmware
defines **STM32F10X_HD** and uses the HD startup file. Original source notes
require at least **64 KB RAM and 128 KB Flash** and mention RDT6/VET6 devices.
These settings were not guessed or silently changed: confirm the actual MCU
and memory map before building and flashing.

Board input filtering, output refresh and I/O configuration were already
disabled in the original source. Their called hooks remain explicitly empty;
this cleanup does not restore hardware I/O functionality.
