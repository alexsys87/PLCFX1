# Experimental FX1N instruction profile

**Use the default minimal firmware for the first upload and ordinary tests.**
The experimental image carries the broad FX1S/FX1N-style instruction layer. The
restored PR2 interpreter had empty, missing and defective handlers and an operand
decoder that mis-sized operands; both were **rewritten**. The result is verified by
host-compiled known-answer tests, a per-slot fuzzer and a GNU Arm build only. It is
not validated on a physical board, against GX Developer/GX Works output, for stack
high-water, or as a safe controller. Test with all loads disconnected.

## Selection and building

This is one firmware source tree with two **compile-time** profiles. There is no
RUN-time mode command and no fallback from an unsupported minimal instruction
into the experimental code. The two interpreters have different accumulator,
branch-stack and program-counter rules; sharing decoded operands between them
would be wrong.

Install GNU Arm GCC and newlib, then run from the repository root:

```sh
# Checked 21-mnemonic subset; also the default with no --runtime argument:
python3 tools/build_bluepill.py --runtime minimal

# Explicit opt-in to the FX1N instruction layer described here:
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

For an IAR build, add `PLC_RUNTIME_EXPERIMENTAL=1` under **C/C++ Compiler >
Preprocessor > Defined symbols** of the chosen configuration, clean and rebuild;
remove it (or set it to `0`) for minimal. As committed, the IAR **Debug**
configuration already defines it and **Release** does not, so Debug builds the
experimental profile (`tools/check_project.py` reports this as a mismatch with its
"minimal-only" expectation). All sources always stay in the project: with the option set,
`src/plc_runtime_minimal.c` compiles to an empty unit and the `src/plc_exp_*.c` modules
provide the interpreter; with the option cleared the modules are the empty units. Do not
exclude any file from the build. The option must be consistent across a build.
**An actual IAR build has not been run**, so the GNU size results below must not be
assumed for IAR.

## Source layout

The experimental interpreter is a set of plain `.c` modules, one per instruction family,
that share three headers. Each module starts with a description of its instructions and
documents every handler; the module list below is also the order of the groups in
`inc/plc_exp_handlers.h`.

| File | Contents |
|---|---|
| `inc/plc_exp_internal.h` | Interfaces shared by the modules: operand type, decoder and helpers, scan state |
| `inc/plc_exp_guard.h` | Bounds-checked access to the PLC image |
| `inc/plc_exp_handlers.h` | Prototypes of every dispatch target, grouped by module |
| `src/plc_exp_dispatch.c` | The three dispatch tables `FUNC_BASIC`, `FUNC_EXT_F`, `FUNC_EXT_0` (original opcode numbering) |
| `src/plc_exp_scan.c` | Scan cycle, RUN/STOP, fault publication, timer advance, `PLC_Main_Parser()` |
| `src/plc_exp_operands.c` | Bit and word device access and the operand decoder |
| `src/plc_exp_logic.c` | Contacts, coils, SET/RST, PLS/PLF, MC/MCR, STL/RET and the opcode routers |
| `src/plc_exp_timers.c` | `OUT Tn`, `OUT Cn`, `RST Tn/Cn` |
| `src/plc_exp_flow.c` | CJ, CALL/SRET, FOR/NEXT, WDT and the interrupt placeholders |
| `src/plc_exp_compare_move.c` | CMP/ZCP, MOV, CML, SMOV, BMOV, FMOV, XCH, BCD/BIN |
| `src/plc_exp_arithmetic.c` | ADD, SUB, MUL, DIV, INC, DEC, NEG, WAND/WOR/WXOR with the flags M8020..M8022 |
| `src/plc_exp_rotate_shift.c` | ROR/ROL/RCR/RCL, SFTR/SFTL, WSFR/WSFL, SFWR/SFWL |
| `src/plc_exp_data.c` | ZRST, DECO/ENCO, SUM, BON, MEAN, SQR, ANS/ANR, Gray code, 7-segment, SORT/SER/ABSD, CCD, ZPUSH/ZPOP |
| `src/plc_exp_float.c` | Single-precision arithmetic, conversion, comparison and math functions |
| `src/plc_exp_time.c` | TCMP/TZCP, TADD/TSUB, HTOS/STOH, TRD/TWR, HOUR |
| `src/plc_exp_compare_contacts.c` | LD/AND/OR with `=`, `>`, `<`, `<>`, `<=`, `>=` in 16 and 32 bit |
| `src/plc_exp_unsupported.c` | The rejected instructions listed below |

Each module is empty when `PLC_RUNTIME_EXPERIMENTAL` is 0 (a one-line marker typedef keeps the
translation unit legal), so the whole `src/` directory can be compiled for either profile.

## What changed from the restored PR2 handlers

Provenance of the dispatch tables is PR2 commit
`4023823a1b9c0edc083288664dda0b04f28a961c`, file `src/ladder.c` (since renamed; the tables now live in
`src/plc_exp_dispatch.c`). All 544 table slots and the seven `FUNC_FF` cases are retained unchanged, so
opcodes keep their numbers; `tools/instruction_inventory.py` verifies that against
`docs/legacy-instruction-inventory.json`. The handlers and the operand layer (the `src/plc_exp_*.c`
modules) are new code.

Defects **measured by running the restored PR2 interpreter** (host build, GCC 13.3, `-O0` and
`-O2`; each now has a regression test):

* `MOV` into a bit group (`K2M0`) left the PLC image and stopped with a bounds fault: the group
  helper OR-ed the unmasked type byte of the operand into the bit address.
* `ADD` never set the zero/borrow/carry flags (`30000 + 30000` left `M8022` clear).
* `DIV` by zero raised SIGFPE on the host (undefined behaviour; a Cortex-M3 returns 0 silently
  unless the divide trap is enabled).
* `DMUL` computed `100000 * 100000` in 32-bit arithmetic (high word 0 instead of 2), and its true
  path read the destination with a two-word helper while its false path skips 12 words (three
  four-word operands); with a four-word destination the leftover words ran as an instruction
  (unsupported fault).
* Nested `CALL`/`SRET` lost the return address: the inner subroutine returned into the outer
  call site and ran 7997 times until the dispatch budget stopped it.
* `RST D5` did not clear `D5`.
* The Z2 index pair read `D8184` twice and the Z3 pair used `D8185:D8186` instead of `D8187:D8186`.

Found by reading the code (fixed and tested, not separately measured): the Z7 pair used
`D8193:D8194` instead of `D8195:D8194`; `1000 + x << 7` mis-parsed in D1000 counter presets;
application instructions addressed C200..C255 at `C0 + 2n` while the counter coil keeps them in
the 32-bit area; `get_value() - get_value()` has unspecified evaluation order (GCC 13 happened
to give the right difference on the host, other compilers need not); `MEAN` used an uninitialised
sum (a compiler warning); time compares tested hours, minutes and seconds independently;
`FOR/NEXT`, `MC/MCR`, `WDT` and the shift/FIFO group were unsupported, empty or missing. The
operand decoder is now one routine: every operand is decoded and range checked before the rung
condition is looked at, so a true and a false rung always consume the same words.

| Handlers (237 callable non-ERR targets) | PR2 restoration | Now |
|---|---:|---:|
| Body that acts on its operands | 147 (unvalidated) | **192** (all with a known-answer test) |
| Empty bodies / originally missing definitions | 40 / 36 | 0 / 0 |
| Explicit unsupported (`PLC_PROG_ERROR`) | 13 | **43** |
| Intentional no-op | 1 (`NOP`) | 2 (`NOP`, `WDT`) |

Of the 89 handlers that were empty, missing or explicitly unsupported, 48 are now
implemented, `WDT` is a defined no-op and 40 remain explicit faults (listed below).
`DHSCS`, `MTR` and `REFF` had bodies but depend on hardware this board lacks; they are
now explicit faults too. The 237 targets are 232 named handlers, three routers
(`FUNC_0`, `FUNC_F`, `FUNC_FF`) and `FUNC_TIMER`/`FUNC_COUNTER`. `FUNC_FF` keeps seven
cases: ANB, ORB, MPS, MRD, MPP, INV (implemented) and POP (explicit fault; it has no
defined FX1N semantics). Counts are repository dispatch targets, not FNC numbers and
not a claim of GX Developer compatibility.

## Implemented instructions

* **Contacts/coils:** LD, LDI, AND, ANI, OR, ORI, LDP, LDF, ANDP, ANDF, ORP, ORF, ANB,
  ORB, MPS, MRD, MPP, INV, OUT, SET, RST (one-word and extended forms), PLS, PLF,
  `OUT Tn`, `OUT Cn`, `RST Tn/Cn`, `RST` of a word device, MC/MCR, STL/RET, END/FEND, NOP.
* **Flow:** CJ, CALL, SRET, FOR, NEXT, WDT.
* **Compare/move:** CMP, ZCP, MOV, SMOV, CML, BMOV, FMOV, XCH, BCD, BIN (and the
  32-bit `D` forms), contact compares LD/AND/OR with `=`, `>`, `<`, `<>`, `<=`, `>=`
  in 16 and 32 bit.
* **Arithmetic/logic:** ADD, SUB, MUL, DIV, INC, DEC, WAND, WOR, WXOR, NEG (16/32 bit).
* **Rotate/shift:** ROR, ROL, RCR, RCL (16/32 bit), SFTR, SFTL, WSFR, WSFL, SFWR, SFWL
  (FIFO read, FX `SFRD`).
* **Data:** ZRST, DECO, ENCO, SUM, BON, MEAN, SQR, FLT, INT, SEGD, GRY, GBIN, SWAP, WSUM,
  WTOB, BTOW, UNI, DIS, SORT, SORT2, SER, ABSD, ALT, CCD, ANS, ANR, ZPUSH, ZPOP (16/32 bit
  where the `D` forms exist).
* **Floating point (single precision):** DEMOV, DECMP, DEZCP, DEADD, DESUB, DEMUL, DEDIV,
  DESQR, DENEG, DSIN, DCOS, DTAN, DASIN, DACOS, DATAN, DRAD, DDEG, DEXP, DLOGE, DLOG10.
* **Time:** TCMP, TZCP, TADD, TSUB, HTOS, STOH, TRD, TWR, HOUR.

## Rejected instructions (stop the PLC with fault 8)

They fail visibly instead of silently doing nothing:

* hardware this board does not have: I/O refresh and scanning `REF`, `REFF`, `MTR`, `TKY`, `HKY`, `DSW`, `ARWS`, `SEGL`; pulse and
  high-speed counter units `DHSCS`, `DHSCR`, `DHSZ`, `SPD`, `DSPD`, `PLSY`, `PWM`, `PLSR`; special blocks, serial and analog `FROM`, `TO`, `RS`, `RS2`, `PR`, `VRRD`, `VRSC`, `RD3A`, `WR3A`;
  interrupts `IRET`, `EI`, `DI` (no interrupt sources exist, so `EI`/`DI` must not silently
  succeed);
* no verified semantics in this repository: `IST`, `INCD`, `TTMR`, `STMR`, `RAMP`, `ROTS`, `ASC`, `PRUN`, `ASCI`, `HEX`, `PID`, `DESTR`, `DEVAL`, `EBCD`, `EBIN`.

## Operand encoding (the repository's local convention, not verified against GX output)

Every operand is two program words, `ttLL` / `uuHH`: the high bytes `tt`, `uu` give the type and
the low bytes `LL`, `HH` the number (`HHLL`).

| Operand | Words | Notes |
|---|---|---|
| K / H constant | `80LL 80HH` / `82LL 80HH` | 16 bit |
| Bit device | `84LL 80HH` | linear bit address of the PLC image, same map as the minimal profile |
| `KnM`, `KnY`, `KnX`, `KnS` | `84LL (80+2n)HH` | n = 1..4 (16 bit) or 1..8 (32 bit); a group must lie inside one area |
| D8000 / T / C / D / D1000 | `86LL 80HH` / `82` / `84` / `86` / `88` | D0..D1999 contiguous; C0..C199 16 bit; C200..C255 32 bit only |
| P pointer | `88LL 80HH` | the value is **2 n** for label `Pn`; the label word is `B0nn` |
| V/Z indexed | types `90..96` (V), `A0..A6` (Z) | Z2/Z3/Z7 pairs use D8185:D8184, D8187:D8186, D8195:D8194 |

In a **32-bit (`D`) instruction every operand is two such items (four words)**: K/H carry the
low and high 16 bits, a device carries `Dn` then `Dn+1`; bit operands, pointers and counts
stay single items. Result operands that a 16-bit instruction writes as 32 bit
(`MUL`, `WSUM`) are a single item. `OUT Tn/Cn` presets are `K` or a `D` item holding an even
**byte** offset (the minimal profile's convention); C200+ presets are 32-bit `K`/`D`.
`STL Sn` carries n in the low 10 bits of the word and addresses step bit `S0 + n`.

Special relays M8000..M8255 can always be read. A program may drive only the mode and user relays
M8030..M8059 and M8100..M8255 (for example M8049, M8161, M8165, M8168, M8200+); status, clock, flag
and error relays are read-only to ladder code. X inputs and timer/counter contacts cannot be written either.

## Behaviour that is easy to get wrong

* ADD/SUB set zero M8020, borrow M8021 and carry M8022 from the exact result; ROR/ROL/RCR/RCL
  and SFWR use M8022. Division by zero, an out-of-range float or a bad operand is an operand
  fault (STOP), not a silent result.
* Pulse execution (`...P`, opcode bit `0x1000`) runs on the rising edge of the rung.
* A false rung decodes and validates operands but changes nothing, except `FOR/NEXT`, `CALL`
  bookkeeping and timer/counter coils, which follow their own rules.
* Master control and step gating apply to every contact that can turn a rung ON: `LD`, `LDI`, `LDP`,
  `LDF`, `STL` and also the parallel contacts (`OR`, `ORI`, `ORP`, `ORF`, comparison `OR`). Behind an
  OFF `MC` or inside an inactive step they cannot switch the rung ON, so the coils there are driven OFF.
* STL: the active step is ON; coils in an inactive step drop on the next scan; `SET Sn`/`OUT Sn`
  inside a step moves to the new step and resets the old one. MC/MCR nest 8 levels (CALL 8,
  FOR 8, at most 128 labels); `END` with an open level is a stack fault.
* C0..C199 count rising edges, C200..C234 are 32-bit up/down counters driven by M8200+n
  (ON = down), C235..C255 (high speed) fault.
* `ANS` uses its timer `T0..T199` for the delay, `HOUR` integrates whole elapsed seconds between
  scans, and `SORT`/`SORT2` finish in one scan with M8029. These follow the FX definitions but were
  not compared with a real PLC.

## Fault reporting

Errors latch in `PLC_Runtime_LastError()` and `PLC_Runtime_ErrorStep()`, M8065 and
D8067/D8068/D8069 (zero-based program-word offset). Codes: 1 opcode, 2 operand, 3 bounds,
4 stack (CALL/FOR/MC), 5 no END, 8 unsupported instruction, 9 per-scan dispatch budget
(24000 instructions). Codes 6 and 7 are kept for compatibility but no handler raises them
any more. A later explicit RUN acknowledges the latch and retries; an invalid program
faults again. Main owns RUN/STOP, M8000/M8001 and the output image; a fault clears the whole
64-byte Y image, STOP keeps M/S/D/counters, and there is no I/O refresh inside an instruction.
The scan adapter advances timers (T0..199 100 ms, T200..245 10 ms, T246..249 1 ms
retentive, T250..255 100 ms retentive) and refreshes the timer contact together with the value.

## Known limitations

* The program is **not prevalidated as a whole** before side effects (the minimal profile
  does that). Each instruction validates its operands, then acts, so a bad instruction late
  in the program stops the PLC after earlier instructions already ran in that scan.
* Labels are found by raw words `B0nn`, not by an instruction-boundary map, and the first
  definition wins. Dispatch is limited to 24000 instructions per scan, but long `ZRST`, `SORT`,
  `BMOV`, `FOR` bodies and libm calls are only as bounded as their own limits (<= 512 words,
  32 x 6 sort table, 64 averaged values). STOP is checked at instruction boundaries.
* The logic-block stack (pending `LD` blocks for `ANB`/`ORB`) and the `MPS` stack are one byte each,
  as in the original code: at most 8 levels. A deeper nesting silently loses the oldest level and is
  not reported as a fault.
* Float functions use newlib's single-precision `sinf`, `cosf`, `expf`, `logf`, `atan2f`,
  `sqrtf`; accuracy is about 1e-6 relative on the host, unmeasured on the target.
* The 43 rejected instructions above, and **no GX Developer interoperability**
  (pointer, `MC` and `MCR` operands, `STL` and 32-bit item layouts are repository conventions).
* No independent watchdog or emergency-stop guarantee. Keep disconnected from loads.

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

Measured with GNU Arm GCC 13.2.1 `-Os`, newlib and the real reset/vector adapter
(`tools/build_bluepill.py`); the PR2 restoration is shown for comparison:

| Allocation | Minimal | Experimental (PR2 restoration) | Experimental now |
|---|---:|---:|---:|
| Allocated firmware Flash, including alignment/data load | 9784 | 42240 | **36404** |
| `.data` | 8 | 352 | 304 |
| `.bss` | 2904 | 4712 | 3352 |
| Ordinary static RAM span, including padding | 2912 | 5064 | 3656 |
| Reserved stack | 2048 | 2048 | 2048 |
| Static gap below stack | 2208 | 56 | **1464** |

Experimental `.text` is 35856 bytes, vectors 236, unwind index 8. The gap is **not extra
stack**: there is no measured stack high-water. `-fstack-usage` shows the largest handler frame
is 128 bytes (`SORT`) and the scan -> dispatch -> handler -> operand accessor chain adds up to
roughly 0.4 KB before libm calls and interrupts; that is a static estimate, not a measurement.
`python3 tools/check_arm_build.py --runtime experimental --optimization=-O2` links the same sources
at `-O2` into 41784 bytes of allocated Flash (`-O1`: 39704), which also fits the 48 KB code region
(the PR2 restoration exceeded it by 17.8 KB); the flashable image is built with `-Os`. IAR sizes may
differ. No partition or 6000-step limit was enlarged.

## Reproducible verification

```sh
python3 tools/instruction_inventory.py
python3 tools/check_project.py --preprocess   # reports the pre-existing IAR Debug define, see above
python3 tools/test_runtime.py
python3 tools/test_protocol.py
python3 tools/test_protocol.py --runtime experimental
python3 tools/test_experimental.py
python3 tools/test_io.py
python3 tools/test_rtc.py
python3 tools/build_bluepill.py
python3 tools/build_bluepill.py --runtime experimental
```

`tools/test_experimental.py` builds `tools/tests/runtime/test_experimental.c` with ASan/UBSan and
runs: known-answer tests for every implemented handler (expected values come from the FX
instruction definitions, for example flags, carry/borrow, signed division, rotation through
carry, overlapping `BMOV`, nested `CALL`, STL transitions, MC, timers and counters, float
functions, time carry/borrow); exhaustive truth tables for all 36 comparison contacts and the
logic-block instructions; false-rung skipping with 32-bit operands; operand and special-relay
rules; and a fixed-seed fuzzer that drives every fuzzable table slot with random typed operands
(167 slots reach the instruction body; 9 context-dependent slots and the 43 rejected
instructions are covered by dedicated tests). The test suite was also mutation-checked: 14 injected
defects (reversed `SUB`, dropped carry flag, wrong operand length, no ACC restore on `SRET`,
edge-less counters, ...) were all caught. It runs unchanged at `-O0`, `-O2` and `-O3`. These are
host-model tests; they do not substitute for physical-board measurements.

**Recommendation:** keep minimal as the normal image. Use experimental only with loads
disconnected, and capture real GX Developer bytecode for the instructions you rely on before
trusting the local operand conventions.
