# Experimental restored instruction profile

**Use the default minimal firmware for the first upload and ordinary tests.**
The experimental image restores executable legacy dispatch/handler code, including
incomplete operations. It is not a full FX1N implementation, a compatibility
guarantee, or a safe controller. No physical-board, GX Developer/GX Works, or
stack-high-water validation has been performed. Test with all loads disconnected.

## Selection and building

This is one firmware source tree with two **compile-time** profiles. There is no
RUN-time mode command and no fallback from an unsupported minimal instruction
into legacy code. The legacy accumulator, branch stack and program counter have
different rules; sharing arbitrary decoded operands between them would be wrong.

Install GNU Arm GCC and newlib, then run from the repository root:

```sh
# Checked 21-mnemonic subset; also the default with no --runtime argument:
python3 tools/build_bluepill.py --runtime minimal

# Explicit opt-in to the restored legacy state machine:
python3 tools/build_bluepill.py --runtime experimental
```

Outputs are separate:

* `build/minimal/plc_fx1n_bluepill.{elf,hex,bin,map}`
* `build/experimental/plc_fx1n_bluepill.{elf,hex,bin,map}`

The build prints its selected profile, experimental warning and memory budget.
`--output DIR` is supported; a `.runtime-profile` marker prevents a subsequent
build from changing the profile in that directory. These images include the
factory PLC header/program at `0x0800C000`: programming the complete image can
replace the currently downloaded PLC program. Retained `F800..FFFF` is not linked.

Flash the chosen image at `0x08000000` using ST-Link/SWD. Boot remains software
STOP. Download/read back a known test program, then explicitly request RUN. The
existing programming connection and STOP commands are unchanged; there is no
physical RUN jumper. Reflash the minimal image to return to the checked profile.

For an IAR build, the existing Debug/Release projects still default to minimal.
In the chosen configuration's **C/C++ Compiler > Preprocessor > Defined symbols**,
add `PLC_RUNTIME_EXPERIMENTAL=1`, clean and rebuild. Remove it (or set it to `0`)
for minimal. `ladder.c` includes the selected `.inc` files automatically; do not
compile those files separately. This option must be consistent across a build.
The IAR project/linker input consistency checks pass, but **an actual IAR build
has not been run**, so the GNU size results below must not be assumed for IAR.

## What was restored, exactly

Provenance is PR2 commit `4023823a1b9c0edc083288664dda0b04f28a961c`,
`src/ladder.c`. Its complete original tables, feature groups and conditional
handler bodies are in `src/ladder_legacy.inc`; this is compiled and reachable in
experimental builds, not an archive. The existing corrected minimal interpreter
is retained unchanged inside the default branch of `src/ladder.c`.

Counts below are generated from actual preprocessed source, with all historical
`USE_*` groups enabled. `tools/instruction_inventory.py` verifies every original
table entry and FF switch case against the checked-in provenance snapshot in
`docs/legacy-instruction-inventory.json`. Counts are **repository dispatch
targets/slots, not official FNC numbers and not claims of functional support**.

| Original table, fully retained | Slots | Named non-ERR slots | ERR slots | Unique non-ERR targets in table |
|---|---:|---:|---:|---:|
| `FUNC_BASIC` | 16 | 13 | 3 | 12 |
| `FUNC_EXT_F` | 16 | 6 | 10 | 3 |
| `FUNC_EXT_0` application/extended table | 512 | 234 | 278 | 230 |
| Total slots | 544 | 253 | 291 | See union below |

The union contains **235 unique non-ERR table targets**, exactly as in PR2.
Three are dispatch routers (`FUNC_0`, `FUNC_F`, `FUNC_FF`), leaving **232 named
handlers**. Two additional OUT-family handlers, `FUNC_TIMER` and `FUNC_COUNTER`,
are invoked by opcode-family dispatch rather than their own table slots.

| Classification of the 232 named table handlers | Count |
|---|---:|
| Nonempty implementation bodies, unvalidated | 142 |
| Originally empty bodies, retained as explicit incomplete faults | 40 |
| Originally declared without a definition, now explicit missing-handler fault stubs | 36 |
| Original explicit unsupported/error bodies | 13 |
| Intentional NOP | 1 |

Including the three routers and two timer/counter bodies gives **237 callable
non-ERR targets: 147 nonempty bodies, 40 empty/incomplete, 36 originally missing,
13 explicit unsupported, 1 NOP**. PR2 had only 201 definitions among these 237;
36 weak declarations could otherwise become null function pointers. All 237 now
have definitions. Presence of a body is not completeness or correctness.

`FUNC_FF` also retains seven nested cases: ANB, ORB, MPS, MRD, MPP, INV and POP.
The first six have legacy logic bodies; POP was empty and now faults as
incomplete. These are not seven extra function-pointer table slots. The minimal
profile's 21 mnemonic count uses a different counting unit; do not subtract it
from a table-slot or function count to invent an extended support count.

## Port changes and fault reporting

The restoration uses CMSIS/register firmware only, with no StdPeriph dependency.
The original handler/table set is preserved, with limited, explicit port fixes:

* Main owns software RUN/STOP, M8000/M8001 status and the interrupt-safe request
  mailbox. There is no old `Addr_M8 == 0x09` whole-byte RUN gate, no physical RUN
  gate, and no I/O refresh inside an instruction. Main samples X and refreshes Y.
* A fault/STOP clears the entire 64-byte Y image, not two partially overlapping
  halfwords. Programming inhibits scans and outputs. STOP preserves internal
  M/S/D/counter data rather than using the historical destructive STOP reset.
* Every instruction fetch is checked against the 6000-word program window.
  RAM accesses are checked for image/bitmap bounds and natural alignment, with
  invalid accesses redirected to a sacrificial aligned object after faulting.
  These checks are containment measures, **not a complete operand validator**.
* Missing definitions and original empty bodies now fault rather than call null
  pointers or dispatch their operand words. Original unsupported bodies fault
  through the same adapter. POP is also explicitly incomplete.
* Single-bit extended operands preserve address bit 12 (`&3FFF`), rather than
  the historical `&2FFF` aliasing of X/T/S into M. Timer/counter reset markers are
  retained rather than stripped as bit operands. Application handlers own their
  operand PC, without the historical unconditional extra dispatcher increment.
* Numeric destination helpers strip the second word's type tag before combining
  address bytes; otherwise even a simple MOV to D0 could escape the PLC image.
  Timer/counter preset PC advances and one timer-index typo were corrected.
* PLS/PLF use the corrected minimal one-scan edge-coil behavior. Pulse storage is
  the reserved byte bitmap, not legacy bit-band accesses.
* Contradictory `AddrB_*` expressions are privately normalized to the current
  linear communication image, not re-exported. Historical timer bookkeeping uses
  otherwise unassigned bytes `0540..059F` in PLC RAM (not the ordinary C region).
* A scan-owned elapsed-tick adapter advances enabled timers and clock relays;
  there is no restored `TimerInc` ISR dependency. This does **not** establish the
  full legacy timer/counter semantics. Minimal retentive-timer guarantees apply
  only to minimal, not these old coil/reset handlers.
* STL count and MTR loop-index widths no longer wrap at 256; STL coil-array
  insertion is bounded. Label indexes are limited to the actual 128-entry table.
  Some unknown-tag temporaries are initialized, but not all legacy errors fixed.

Errors latch in `PLC_Runtime_LastError()` and `PLC_Runtime_ErrorStep()`, M8065,
and D8067/D8068/D8069 (zero-based program-word offset). Existing enum values 0..5
are unchanged; added values are 6 missing legacy definition, 7 incomplete legacy
body, 8 original unsupported handler, and 9 dispatch budget exceeded. A later
explicit RUN acknowledges the latch and retries; an invalid program faults again.

## Known limitations: this is not the checked minimal runtime

The experimental interpreter **does not prevalidate a complete program before
side effects**. Operand decoding, false-rung lengths, indexed/grouped operands,
double-word and float handlers remain inconsistent. Some malformed programs can
run preceding internal writes before stopping. Valid addresses may still select
the wrong logical device; checking image bounds cannot prove device semantics.
Unsupported tags can still produce incorrect results or consumption lengths.

Legacy accumulator/MPS stacks are packed bytes with no minimal-style balance or
depth checking. CJ/CALL/SRET keep the original defective save/return conventions.
Labels are identified by raw words, including possible operands, not by a proved
instruction-boundary map. Missing labels eventually fault on fetch; this is not
proof of valid control flow. Dispatch is limited to 24000 instructions per scan,
but **inner handler loops and math calls are not time-bounded by that budget**.
Software STOP is checked at instruction boundaries, not during every operation.
There is no independent watchdog or emergency-stop guarantee.

Known arithmetic defects include signed overflow, division by zero, shifts and
rotations with invalid widths, incomplete 32/64-bit transfers and float conversions,
incorrect grouped masks, and dubious time comparisons. Compiler warnings remain
visible, including possibly uninitialized grouped-mask/helper/MEAN values. These
warnings are not suppressed or evidence of test coverage. High-speed counters,
motion, communication, hardware expansion and many special operations remain
empty/missing/error stubs. The inventory lists each handler's exact status.

The tests execute selected real handlers, not every body. Neither successful
linking nor a table entry demonstrates Mitsubishi compatibility, cycle timing,
physical output behavior, or safety. Keep disconnected from loads.

## C8 memory contract and measured GNU build

Both profiles retain the existing non-overlapping partitions:

| Region | Address | Reservation |
|---|---|---:|
| Firmware code/data load | `08000000..0800BFFF` | 49152 bytes |
| PLC program | `0800C000..0800F7FF` | 14336 bytes; 6000 steps plus header |
| Retained flash, not linked | `0800F800..0800FFFF` | 2048 bytes |
| Ordinary C RAM, including stack | `20000000..20001BFF` | 7168 bytes |
| Pulse bitmap | `20001C00..20001FFF` | 1024 bytes |
| PLC image | `20002000..20004FFF` | 12288 bytes |

Measured with GNU Arm GCC `-Os`, newlib and the real reset/vector adapter:

| Allocation | Minimal | Experimental |
|---|---:|---:|
| Allocated firmware Flash, including alignment/data load | 9678 | 42240 |
| `.data` | 9 | 352 |
| `.bss` | 2908 | 4712 |
| Ordinary static RAM span, including padding | 2920 | 5064 |
| Reserved stack | 2048 | 2048 |
| Static gap below stack | 2200 | **56** |

Experimental `.text` is 41640 bytes, vectors 236, unwind index 8, and Flash
alignment padding 4. Its ordinary C allocation ends at `200013C8`; stack is
`20001400..20001BFF`. The 56-byte gap is very small and is **not extra stack**.
The linker rejects an oversized code region, static/stack overlap, or incorrect
PLC/bitmap reservations. There is no proven stack high-water, especially for
libm, nested calls and interrupt nesting. A future added static variable can fail
the link. Measured fit does not prove runtime stack fit. IAR/other compiler sizes
may differ. No partition or 6000-step limit was enlarged to hide overflow.

## Reproducible verification

```sh
python3 tools/instruction_inventory.py
python3 tools/check_project.py --preprocess
python3 tools/test_runtime.py
python3 tools/test_protocol.py
python3 tools/test_protocol.py --runtime experimental
python3 tools/test_experimental.py
python3 tools/test_io.py
python3 tools/test_rtc.py
python3 tools/build_bluepill.py
python3 tools/build_bluepill.py --runtime experimental
```

Default runtime: 1071 complete scans under ASan/UBSan, with its malformed-program
and interrupt-mailbox regressions. Default protocol, I/O and RTC tests also pass.
Experimental smoke fixtures execute MOV, ADD, DEC and CMP with real operands,
extended X mapping, PLS/PLF, STOP/restart/programming inhibit, original unsupported,
empty/missing handler and POP faults, invalid RAM address and program exhaustion.
Framed client integration passes in **both** profiles: connect/identify, STOP,
upload/flush/readback, RUN, actual M0 and M8013-to-Y0 execution with simulated ticks,
monitor, malformed-program recovery, 6000-word chunking and final STOP. These are
host-model tests; they do not substitute for physical-board measurements.

**Recommendation:** retain minimal as the normal image. Use experimental only
to inspect/repair needed legacy operations one at a time, with loads disconnected,
then add captured bytecode and hardware regressions before relying on any handler.
