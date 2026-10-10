# Connect, upload, verify and run a first program

## Status and scope

This is a **bench-only STM32F103C8 experiment**, not a certified industrial PLC.
The protocol retains this repository's inherited FX-style ASCII framing. **GX
Developer and GX Works2 compatibility is UNVERIFIED**. The reported legacy FX1N
model value (`0x6266`) is not evidence of compatibility. The example instruction
words match this project's local interpreter, not independently verified
Mitsubishi bytecode. No board or live programming-software test has been done.

No hardware is needed for the host tests below. An actual connection later needs
a correctly flashed BluePill and a USB-to-**3.3 V TTL UART** adapter. Do not use
an RS232 voltage-level adapter, a 5 V UART, a 24 V PLC signal or an RS422 cable
directly. Keep machinery and other real loads disconnected.

## Wiring and serial settings

| BluePill | USB UART |
| --- | --- |
| PA9 (USART1 TX) | RX |
| PA10 (USART1 RX) | TX |
| GND | GND |

Power the board from one suitable source; do not join two power supplies through
the adapter's VCC pin. Use **19200 baud, 8 data bits, no parity, 1 stop bit**, no
flow control. This is the project's setting, not a claim about commercial FX
programming-port defaults. UART noise no longer changes baud rate. Close other
serial applications before opening the port. The programming UART is not an
SWD firmware-flashing interface.

Reset boots into **STOP**, even if the saved instruction stream is corrupt.
The software RUN/STOP commands do not require a PB8 strap. Y0 is PB0, not the
on-board PC13 status LED. For an optional blink test connect only a small LED
with a suitable current-limiting resistor between PB0 and GND. Start with M0
monitoring, which needs no load at all.

## Host-only evidence

From the repository root, on Linux with a C compiler and Python 3:

```sh
python3 tools/test_protocol.py
```

This compiles real `src/PLC_Com.c` with ASan/UBSan and exercises complete framed
requests, DMA ownership, malformed checksums/lengths, range errors, staging,
Flash-error NAKs, retained-data CRC and stream resynchronization. A Python client
then connects to that C implementation over process pipes and performs
identify → STOP → upload → flush → Flash readback → RUN → monitor → STOP,
including a full 6000-word upload. The same client test is repeated with the
**actual ladder interpreter**, checking M0, rejected malformed programs and
subsequent recovery. A combined C integration test checks both examples and
M8013/Y0 using simulated 1 ms ticks and the runtime clock relays (the actual
TIM2 ISR is not part of that host fixture). These are not electrical UART, DMA hardware,
Flash timing, power-interruption or industrial safety tests.

## First live connection (when a board becomes available)

Install the optional live serial dependency:

```sh
python3 -m pip install pyserial
python3 tools/plc_link.py --port COM3 identify
# Linux example: replace COM3 with /dev/ttyUSB0
```

Expected identity: model code `0x6266`, capacity `6000` words, state `STOP`.
Use `--trace` before the subcommand to see the wire bytes. A timeout means the
port, wiring, voltage levels, firmware, clock or baud settings need checking;
the tool does not guess other baud rates or repeatedly retry writes.

Upload the simplest program, retaining STOP until explicitly requested:

```sh
python3 tools/plc_link.py --port COM3 upload examples/always_on_m0.hex
python3 tools/plc_link.py --port COM3 run
python3 tools/plc_link.py --port COM3 monitor --seconds 3
python3 tools/plc_link.py --port COM3 stop
```

`LD M8000; OUT M0; END` should set M0 while RUN is active. M0 may remain set in
STOP; it is an internal relay, not an output-safety indication. Y outputs are
cleared/gated on STOP. The CLI's monitor reads RUN/M8000, M8013, M0 and Y0.

An automated bench smoke sequence attempts STOP in a `finally` block:

```sh
python3 tools/plc_link.py --port COM3 smoke examples/always_on_m0.hex --seconds 3
python3 tools/plc_link.py --port COM3 smoke examples/clock_y0.hex --seconds 3
```

For the second example, `LD M8013; OUT Y0; END`, the M8013 clock and Y0 should
alternate on roughly half-second boundaries (a one-second cycle). Test only
after the no-load M0 test succeeds. `smoke` recognizes the exact known example
bytes and **fails with a nonzero exit code if their behavior is absent**. For M0
it clears the coil while STOP first, then requires it ON after RUN; for the clock
example it requires both clock and Y0 phases and matching observations of both.
Use the default three-second monitor window. Unknown programs require an
explicit `--expect m0`, `--expect clock-y0`, or `--expect none`; the last option
tests transport only and makes no ladder-success claim.

`smoke` uploads, verifies, RUNs, monitors and finally STOPs; `upload --run`
deliberately leaves RUN active. A cable loss,
process kill or OS failure can prevent final STOP: this is not a safety system.

Save the exact first example back from Flash:

```sh
python3 tools/plc_link.py --port COM3 readback saved.bin --bytes 8
```

## Bytecode and storage limits

Examples are **plain hexadecimal bytes** with optional `#` comments, not Intel
HEX firmware files or GX project files. `.bin` input is raw binary. Words are
little endian:

| Local instruction | Words |
| --- | --- |
| LD M8000 | `01C2 8E00` |
| LD M8013 | `01C2 8E0D` |
| OUT M0 | `C000` |
| OUT Y0 | `CC00` |
| END | `000F` |

The interpreter supports only the documented bounded subset in
`docs/runtime-subset.md`. No compiler or translator for arbitrary ladder
programs is provided. END must be the final uploaded word. Invalid instruction
streams are rejected by the runtime before executing a scan; a protocol ACK to
RUN is only an accepted request, so the client subsequently reads the state.
RUN/STOP commands enqueue a request applied at a main-loop scan boundary, not
an ISR read/modify/write of the runtime-owned M8000..M8007 status byte.
Explicit RUN requests clear old runtime fault state and revalidate the stream.
An invalid stream therefore faults to STOP again rather than executing it.

STM32F103C8 has **64 KB Flash and 20 KB RAM**. The program partition is 14 KB at
`0x0800C000..0x0800F7FF`; retention is separate at `0x0800F800..0x0800FFFF`.
The 92-byte inherited parameter header precedes code at offset `0x005C` (wire
address `0x805C`). `MAXSTEP=6000` limits the body to 12000 bytes. The header
capacity and D8002/D8102 advertise 6 thousand words, not 20000 steps. Raising
this to 20000 does not fit the C8's partition alongside firmware. D8001/D8101
share the legacy identity code; that value remains an emulation convention.

Uploads preserve the existing header/parameter bytes and unrelated Flash bytes.
The firmware image/linker must place the `.plc_rom` factory header at the program
base when initially flashing. There is **no newly invented on-wire image header
or persistent program-valid CRC**: do not mistake the retained-data CRC for
program-image integrity. Download is in-place, not power-loss atomic. Boot STOP
and runtime validation reduce risk but cannot detect all corrupted bytecode that
still decodes to valid instructions. After an interrupted download, upload and
verify the entire intended program again before RUN.

The CLI sends 60-byte aligned writes, flushes staging with E42 and compares the
entire uploaded body to Flash before RUN. A failed comparison/NAK is an error,
not success. Uploads do not automatically erase the tail beyond END. The END
terminates scanning, so that tail is not executed. Do not pipeline requests:
input arriving while a response owns the DMA buffer is dropped.

## Wire conventions retained

Requests: `STX command ASCII-hex-payload ETX checksum`. The checksum is the low
byte of the sum from command through ETX, represented by two uppercase ASCII
hex digits. STX is excluded. Data replies use STX, ASCII data, ETX and a checksum
over ASCII data through ETX; successful non-data commands return ACK (`06`),
rejections return NAK (`15`). ENQ (`05`) receives ACK without changing RUN.
Ordinary byte addresses are big endian in request payloads; returned word bytes
and instruction bytes are little endian. E7/E8 special force requests encode
the linear bit address low byte first (`E7250E` STOP, `E7240E` RUN).

Relevant implemented operations: `0` RAM read, `1` RAM write, `7`/`8` linear-bit
force, E00 parameter/monitor read, E10 RAM parameter write, E01 program read,
E11 program staging write, E42/`B` flush, E61 discard staging, E60 clear program.
Unsupported commands, invalid hex, oversized or mismatched frames and unsafe
ranges return NAK. Raw RAM writes crossing status byte `0x01C0` and generic bit
forces targeting M8000..M8007 are rejected, avoiding lost RUN/STOP races with
main-loop status updates. E60 requests STOP. Generic bit forcing no longer unexpectedly
starts RUN. Low-level raw RAM/bit writes remain a trusted bench interface; do
not expose this unauthenticated port to untrusted senders.
