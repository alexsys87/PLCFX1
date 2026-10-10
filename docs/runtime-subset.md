# Minimal FX1N-style runtime: supported local encoding

This is an auditable educational subset, **not an FX1N replacement**, an official
instruction-set emulator, or a safety-rated controller. No hardware validation or
GX Developer/GX Works interoperability claim follows from the host tests.

## Integration and execution

`PLC_Runtime_Init()` initializes private state and pulse history without changing
the PLC register image or RUN request. Call it after RAM/protocol initialization.
`PLC_Main_Parser()` also initializes lazily, so a first scan does not depend on a
previous STOP. `PLC_ProInstructParse()` is a compatibility wrapper around it.
`PLC_Runtime_Tick1ms()` must run once per millisecond; it only increments a time
counter. Do not retain the old `TimerInc` ISR path.

Main must sample its filtered physical inputs into the X image before a scan,
then call the parser and finally `PLC_IO_Refresh()`. The interpreter does not
sample GPIO or flush physical outputs itself. No PB8 physical RUN gate exists.
RUN is M8000=1, M8001=0; STOP wins if both are set. Other bits in that byte do not
gate RUN. `PLC_UPDATE_PROG` inhibits execution and clears Y. Programming protocol
boot policy is STOP; this module does not override that policy.

USART/other ISR code must call `PLC_Runtime_RequestRun(u8 run)` rather than
writing the first special-M byte. This publishes a volatile one-byte mailbox:
zero requests STOP, nonzero requests RUN, and the latest pending request wins.
At parser entry main atomically reads and clears the mailbox in a short
PRIMASK-preserving critical section. Only main applies transitions to M8000/M8001.
A RUN arriving during already-started STOP cleanup survives for the next parser;
a pending STOP aborts validation/execution at instruction boundaries. Status-bit
RMWs in the first special-M byte also use short PRIMASK-preserving critical
sections. Interrupts remain enabled for the rest of the scan. No ISR/raw protocol
write or force may alter this reserved status byte.

`PLC_Runtime_RunRequested()` reports the effective pending request, or the
current image when no request is pending. Output refresh should require **both**
this function and the current M8000=1/M8001=0 image: pending STOP inhibits outputs
even if the previous scan's Y image is still high, while pending RUN alone must
not enable them before main applies it. `PLC_NEW_PROG` and `PLC_UPDATE_PROG` are
volatile across main/ISR. Runtime initialization does not discard the mailbox,
so a command arriving around startup is not silently lost.

Each scan first decodes the entire program through END, validating every opcode,
operand and stack operation before user instructions execute. A second bounded
pass executes it. Both passes consume multiword operands rather than dispatching
operand words. There are no jumps, loops or calls. Bounds are `MAXSTEP` words from
`PLC_START_ADDR` (currently 6000), 8 logic-block frames and 8 MPS branch frames.
No END, truncated operands, unsupported instruction or invalid address faults.

Faults immediately set software STOP and zero the entire 64-byte Y image; main's
subsequent output refresh applies the safe state. M8065, D8067=error enum and
D8068/D8069=zero-based word offset provide diagnostics. `LastError` and `ErrorStep`
expose the same information. An explicit later RUN acknowledges the latch and
revalidates; `PLC_NEW_PROG` also clears it. A still-invalid program stops again.
STOP is checked between executed instructions. This is not an external emergency
stop, independent watchdog, or transactional rollback of M/D changes: a runtime
data-value fault may follow earlier internal writes, but Y is cleared before the
normal physical output refresh.

STOP preserves M, S, D, counters, and retentive timer values; it clears Y, timer
enables, counter edge history, nonretentive timer values/completion, and pulse
history. Restart exposes M8002 for the first scan and M8003 for later scans.
PLS fires on a true first scan; PLF does not falsely fire from startup-low.

## Exact machine words

Words below are hexadecimal 16-bit words; the programming protocol stores each
word little-endian. These are the **repository's existing dispatch encodings**,
not guessed official FNC numbers. Ordinary bit operands are linear bit offsets
in the `Addr_*` communication RAM map. The obsolete `AddrB_*` aliases are not used.

| Instruction | Encoding and length |
|---|---|
| LD/LDI/AND/ANI/OR/ORI | `2bbb`/`3bbb`/`4bbb`/`5bbb`/`6bbb`/`7bbb`, one word |
| Extended contacts | `01C2`..`01C7`, then `8000 | linear_bit`, two words |
| OUT/SET/RST | `Cbbb`/`Dbbb`/`Ebbb`, one word |
| Extended OUT/SET/RST | `0002`/`0003`/`0004` (also existing aliases `0005`/`0006`/`0007`), then marked bit operand |
| PLS/PLF | `0008`/`0009`, then marked bit operand |
| ANB/ORB/MPS/MRD/MPP/INV | `FFF8`/`FFF9`/`FFFA`/`FFFB`/`FFFC`/`FFFD` |
| NOP | `0000` only |
| END | `000F` (also existing dispatch alias `001C`) |
| OUT Tn | `0600 | n`, then two-word preset |
| OUT Cn | `0E00 | n`, then two-word preset; n must be below 200 |
| RST Tn/Cn | `000C`, then `8600 | n` / `8E00 | n` |
| MOV | `0028`, two-word source, two-word D destination |
| INC | `0040`, two-word D destination; unsigned 16-bit wrap |

Marked bit operands strip `8000` using `&3FFF`. The old decoder's `&2FFF`
incorrectly lost address bit 12 and aliased T/X/S contacts into M; preserving that
bug would not preserve the intended linear device encoding. A `8000` marker is
required, and reserved/gap addresses are rejected. Basic operands above `0FFF`
must use the extended form. High-nibble B labels/no-ops, POP, STL, indexed/grouped
operands, pulse-modified application instructions, comparisons, arithmetic other
than INC, 32-bit operations, communication instructions and every other opcode
are explicitly rejected.

| Device | Linear bit base | Read / ordinary coil write |
|---|---:|---|
| M0..3071 | `0000` | yes / yes |
| Y0..511 | `0C00` | yes / yes |
| M8000..8255 | `0E00` | yes / no |
| C0..199 completion | `0F00` | yes / reset instruction only |
| T0..255 completion | `1000` | yes / reset instruction only |
| X0..511 | `1200` | yes / no |
| S0..4095 | `1400` | yes / yes |

These broad RAM image ranges reflect the existing local map, **not a claim that
all these device counts belong to a real FX1N**. Only X0..X7/Y0..Y7 are connected
by the current board I/O profile. X/Y labels are normally presented in octal;
the encoded offset uses their numeric bit index. Special relays beyond the few
implemented status/clock functions are merely readable image bits.

Two-word numeric sources combine the low byte of each word as a 16-bit number.
K is `80LL 80HH`; H is `82LL 80HH` (H only for MOV). Register operands are
`86LL TT HH` expressed as two words `86LL` and `TTHH`: tags `80` D8000,
`82` T, `84` C, `86` D0..999, `88` D1000..1999. The combined low bytes are a
**register number**; D1000 block uses an index relative to D1000. T/C numbers are
bounded to 256/200. MOV reads these sources but writes D0..1999 only; INC also
writes D0..1999 only.

The original timer/counter preset decoder has a different D representation:
`86LL 86HH` or `86LL 88HH` contains an **even byte offset** relative to D0 or
D1000. This convention is retained, checked, and tested separately, rather than
silently substituting MOV's convention. Presets accept K0..32767 or these D
sources; negative, odd, out-of-range and unsupported-tag presets fault even
if the rung is false. A downloaded GX project must be inspected to establish
which convention it emits; no captured commercial-compiler fixture was supplied.

## Timers, counters and edges

Timer completion/enable/value/preset use the same linear `Addr_TO`, `Addr_TE`,
`Addr_TVal`, `Addr_TMax` image exposed by communication. Scan-time updates consume
elapsed 1 ms ticks, per-timer fractions and the previous scan's enable image:
T0..199=100 ms; T200..245=10 ms; T246..249=1 ms; T250..255=100 ms.
T246..255 retain their value and fractional time while disabled/STOP; other
timers reset when disabled/STOP. RST clears either kind. Completion is `value >=
preset`; K0 completes immediately when enabled. Values saturate at the preset.
Timer state is scan-granular, not asynchronously changed by the ISR. Time while
STOP is not credited on restart; interrupted/disabled IRQ ticks cannot be recovered.

C0..199 are saturating **up counters** on false-to-true OUT-rung edges, with
completion at `value >= preset`; false rungs retain value and clear edge history.
RST clears value, completion and edge history. Down-direction special relays,
retention-range configuration and all C200..255 32-bit/high-speed counters are
unsupported. Do not infer complete Mitsubishi C100..199 direction semantics.

PLS/PLF remember each instruction's input at its word offset in the reserved
1 KB pulse bitmap. They write the destination every scan, giving exactly one
scan on a rising/falling edge, and recover correctly across STOP/RUN/program change.
Multiple writes to one coil follow program order. LD after an output begins a
new logic block; nested LD before output is combined explicitly using ANB/ORB.
Unbalanced block/branch stacks are rejected, not silently truncated.

M8011/12/13/14 are scan-visible 50% square waves with periods 10/100/1000/60000 ms
from initialization, first half low, and continue their phase while STOP. No
missed tick compensation is provided. The unsigned 32-bit millisecond clock
wraps after about 49.7 days; modulo-derived clock relay phase can jump at wrap.

## Tested complete-scan fixtures

`python3 tools/test_runtime.py` compiles the **actual ladder.c**, not copied helper
functions, using Linux fixed-address guarded RAM/Flash and ASan/UBSan. It tests
startup, software STOP/RUN, update inhibit, status bits, clock relay output scans,
all basic contact families, extended X/S/T/C/special-M mapping, full-range coils,
logic/branch stacks, PLS/PLF startup and STOP recovery, timer bases/retention,
byte-offset D presets, C0/C199 edges/reset, T255, MOV/INC, faults, missing/truncated
END, operand-as-END, invalid markers/ranges/writes, stack overflow, and 1000
deterministic malformed programs. Sanitizers check host accesses/alignment, not
Flash electrical behavior, IRQ latency, GPIO wiring or industrial safety.

Mocked PRIMASK operations additionally inject a simulated USART RUN during STOP
cleanup, STOP immediately after the atomic mailbox exchange, and STOP during
M8002 status-bit RMW. These regressions verify no acknowledged request is lost,
no post-STOP scan output survives, and already-disabled IRQ state is preserved.

Confirmed programming samples:

* `LD M8000; OUT M0; END`: `01C2 8E00 C000 000F`.
* `LD M8013; OUT Y0; END`: `01C2 8E0D CC00 000F`.
* `LD M0; OUT T0 K2; END`: `2000 0600 8002 8000 000F`.
* `LD M0; OUT C0 K2; END`: `2000 0E00 8002 8000 000F`.
