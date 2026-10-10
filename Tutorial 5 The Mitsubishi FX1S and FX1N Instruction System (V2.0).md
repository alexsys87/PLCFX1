# Tutorial 5: Mitsubishi FX1S and FX1N Instruction System (V2.0)

## 1. Scope: a model-specific reference, not a compatibility claim

The name **PLCFX1** is a project name, not a Mitsubishi CPU model specification.
This tutorial uses **FX1S as the baseline**, because the previous tutorial explicitly
named FX1S. Differences for **FX1N** are identified separately. Do not silently
substitute FX3U, FX2N, the older unsuffixed FX series, or FX1NC for either profile.

This document describes the **intended Mitsubishi behavior**. It does not claim
that the STM32 interpreter implements every listed instruction correctly. Hardware
functions, retention, device ranges, instruction encoding and execution semantics
must each be validated independently.

### Official sources and how to use them

The primary source is Mitsubishi Electric's **FX Programming Manual II,
JY992D88101, revision E, April 2015**, covering FX1S, FX1N, FX2N and FX2NC:

- [Official complete programming manual, PDF](https://www.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy992d88101/jy992d88101e.pdf).
- [FX1S hardware manual, JY992D83901-P](https://www.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy992d83901/jy992d83901p.pdf).
- [FX1N hardware manual, JY992D89301-R](https://dl.mitsubishielectric.com/dl/fa/document/manual/plc_fx/jy992d89301/jy992d89301r.pdf).

Read chapters **2–3** for basic/STL programming, **4** for devices, **5** for
individual applied instructions, **6** for diagnostics, **7** for execution-time
tables, **8** for model capacities, **9** for system devices and **11.3** for the
alphabetical instruction/model index. The full PDF is the definitive detailed
reference for operand restrictions, variants, special flags and hardware conditions;
this tutorial is an original learning guide and complete model-filtered inventory,
not a reproduction of every diagram in the manual.

**FNC numbers are decimal manual identifiers, not hexadecimal machine opcodes.**
The previous tutorial mixed an FX1S introduction with a broader FX-family command
table and unofficial binary layouts. Those layouts have been removed. The official
manual does not establish the bytecode/address translation used by this project.
Generate test programs with the programming software configured for the exact
target CPU; do not derive opcodes by arithmetic on FNC numbers.

The source itself has editorial inconsistencies: some internal contents/index page
references are stale, the appendix spells ESQR as ESOR, and some capacity tables
contain typographical errors. Use the actual instruction heading and model marking,
not only the index or a headline instruction count. Source ambiguities are noted
below instead of being converted into invented compatibility rules.

## 2. Scan cycle and ladder evaluation

A normal program is evaluated cyclically: refresh inputs, execute the main program
in order, then perform END processing and output refresh. This is a logical model;
I/O refresh instructions, interrupts and hardware counters are explicit exceptions.

- A contact reads a bit; an output instruction writes or controls a device.
- `LD` starts a logical expression. `AND` adds a series condition and `OR` adds a
  parallel condition. The `I` forms negate the addressed contact.
- `OUT Y0` follows the current rung condition. `SET Y0` latches it ON; `RST Y0`
  clears it. A false SET/RST rung does not undo the previous state.
- Program order matters. Multiple writes to the same coil can overwrite each other;
  avoid double-coil designs unless their ordering is intentional.
- A PLC scan is not a fixed-duration timer tick. Do not implement timer elapsed time
  by counting scans, and do not confuse a one-scan pulse with a fixed-width hardware
  pulse output.

Example: output while X0 is ON and X1 is OFF.

```text
LD  X0
ANI X1
OUT Y0
END
```

Example: latch an internal relay with X0 and reset it with X1. If both are ON in the
same scan, the later RST wins in this example.

```text
LD  X0
SET M0
LD  X1
RST M0
LD  M0
OUT Y0
END
```

## 3. Devices and operands

### 3.1 Logical devices, not STM32 addresses

| Device | Meaning | FX1S baseline | FX1N difference |
|---|---|---|---|
| X / Y | Physical inputs / outputs; **octal numbering** | Actual base-unit points; at most 16 inputs and 14 outputs | Up to 128 total physical I/O; software addressing up to 128 X and 128 Y, not 256 installed points |
| M | Auxiliary bits; decimal numbering | M0–M511; default general M0–M383, latched M384–M511 | M0–M1535; default latched M384–M1535 |
| S | Step/state bits | S0–S127; S0–S9 initial states | S0–S999; S0–S9 initial states; retention differs |
| T | Timer contact and current value | T0–T63; see timer table below | T0–T255; resolution/retentive ranges differ |
| C | Counter contact and current value | Ordinary 16-bit C0–C31; hardware high-speed counters are separate | Ordinary 16-bit C0–C199 and 32-bit C200–C234, plus hardware high-speed counters |
| D | 16-bit data registers | D0–D255; default general D0–D127, latched D128–D255 | D0–D7999; general D0–D127 and D1000–D7999, latched D128–D999; parameterized file-register area overlaps the higher range |
| M8000… / D8000… | Special control/status devices | Documented functions within M8000–M8255 / D8000–D8255 | Same nominal ranges do not imply identical functions or writable devices |
| V / Z | Index registers | V0–V7 and Z0–Z7 | Same indexed-register notation |
| P | Program-flow labels | P0–P63 | P0–P127; instruction-specific limits still apply |
| I | Interrupt labels | Source/edge-coded labels, not ordinary coils | More input interrupt sources; use the exact model's interrupt table |
| N | MC/MCR nesting levels | N0–N7 | N0–N7 |

These are Mitsubishi capacities, **not the PLCFX1 memory map**. Retentive behavior
also depends on device class, PLC parameters and supported storage facilities.
Reserved special devices must not be treated as unrestricted user memory.
For high-speed C235–C255 selection, use chapter 4's actual model-specific input
assignment tables; not every counter number is available and several counters
compete for the same physical input resources.

### 3.2 Constants, grouped bits and word order

- `K` introduces a decimal constant; `H` introduces hexadecimal. Signed 16-bit
  arithmetic uses −32768…32767; signed 32-bit arithmetic uses
  −2147483648…2147483647. A hex bit pattern is not automatically a positive signed
  arithmetic value.
- `K1M0` groups four consecutive M bits; `K4M0` groups sixteen. Grouped-bit operands
  such as `KnX`, `KnY`, `KnM` and `KnS` are permitted only where the instruction's
  operand table allows them. A constant `K4` and a grouped device `K4M0` are different
  operand types. X/Y device numbering remains octal when grouping bits.
- A double word starting at D0 occupies **D0 (low word) and D1 (high word)**. Reserve
  the complete destination extent; a 32-bit result must not overwrite unrelated D1.
- Indexing such as `D10Z0` adds an index offset where that operand supports indexing.
  A 32-bit index combines the corresponding V/Z registers and is designated by Z;
  this is not permission to index every operand of every instruction.

### 3.3 Timers and counters

| Profile / range | Time unit | Behavior |
|---|---|---|
| FX1S T0–T62 | 100 ms by default | Non-retentive ON-delay |
| FX1S T32–T62 with M8028 ON | 10 ms | Selectable subrange, not additional timers |
| FX1S T63 | 1 ms | Non-retentive timer; do not confuse it with FX1N retentive timers |
| FX1N T0–T199 | 100 ms | Non-retentive ON-delay |
| FX1N T200–T245 | 10 ms | Non-retentive ON-delay |
| FX1N T246–T249 | 1 ms | Retentive/accumulating |
| FX1N T250–T255 | 100 ms | Retentive/accumulating |

For FX1S use chapter 4's T0–T62 allocation; the chapter 8 row says “63 points” but
prints T0–T55, an inconsistent endpoint. It must not be read as eight missing
timers. For FX1N the chapter 8 row also misprints the 1 ms time span: the normal
16-bit preset limit of 32767 corresponds to **32.767 seconds**, not 32767 seconds.

`OUT T0 K10` drives a timer with a ten-unit preset. For normal FX1S T0 this means
**1 second**. Its contact turns ON when the preset is reached; removing the drive
resets a non-retentive timer. A retentive timer preserves accumulated time when its
drive is removed and requires RST to clear it. `RST T0` clears both timer value and
completion state. Reading T0 as data accesses its current value, not its contact.

```text
LD  X0
OUT T0 K10
LD  T0
OUT Y0
END
```

An ordinary up-counter counts **OFF→ON transitions**, not every scan of a held-high
rung. `OUT C0 K3` reaches its target after three such events; `RST C0` clears the
value and contact. A 32-bit up/down counter is a different device class, with its
direction controlled through the documented special relay. High-speed counters
count physical input events independently of normal ladder scan timing.

## 4. Basic and step-ladder instruction reference

All entries in this section apply to both FX1S and FX1N. References: manual chapters
2–3 and the mnemonic/device tables in §7.1. P/I labels are included explicitly;
STL and RET are step-ladder instructions, not applied functions.

| Instruction(s) | Meaning and essential rule |
|---|---|
| LD, LDI | Start a rung/block with a normal/inverted contact |
| AND, ANI | Series normal/inverted contact |
| OR, ORI | Parallel normal/inverted contact |
| LDP, LDF | Start a block with a rising/falling-edge contact |
| ANDP, ANDF | Series rising/falling-edge contact |
| ORP, ORF | Parallel rising/falling-edge contact |
| ANB | AND the current logical block with the preceding block |
| ORB | OR two logical blocks |
| MPS | Push the current logical result onto the branch stack |
| MRD | Read the saved result without removing it |
| MPP | Read and remove the saved result; balance each MPS |
| INV | Invert the current logical result, not an addressed device |
| OUT | Drive Y/M/S where allowed, or a timer/counter with its preset; X is not writable |
| SET | Latch an allowed Y/M/S device ON; special devices have restrictions |
| RST | Clear an allowed bit, timer/counter value/contact, or writable D/V/Z device |
| PLS, PLF | Generate a one-operation-cycle pulse in an allowed Y/M device on the rising/falling rung edge; not a special-M target |
| MC, MCR | Enter/leave a master-control block with N0–N7 nesting |
| NOP | No operation |
| END | Finish the program scan and perform END processing |
| STL | Begin the program associated with an S state |
| RET | Finish the STL region and return to ordinary ladder evaluation; **not SRET** |
| P label | CJ/CALL destination declaration; **not the P suffix** |
| I label | Interrupt-routine entry declaration, with source/edge-specific numbering |

Edge contacts and PLS/PLF require previous-state memory. The output pulse must
terminate on the following operation cycle even when the signal stays ON. Startup,
STOP/RUN transitions and skipped code need the manual's specific edge rules; a
simple global previous-input bitmap is not sufficient proof of conformance.

When MC is false, the controlled region is not just jumped over while every output
holds its old value. Normal coils and non-retentive timers are reset as documented;
counters, retentive timers and SET/RST-controlled devices have different behavior.
MCR at a level resets that level and deeper levels. Avoid mixing MC/MCR and STL
without checking chapter 3's restrictions.

## 5. Applied-instruction forms

For an instruction **whose own variant table permits these forms**:

| Example | Meaning |
|---|---|
| MOV D0 D10 | 16-bit transfer on every scan while enabled |
| MOVP D0 D10 | 16-bit transfer once on the enable condition's rising edge |
| DMOV D0 D10 | 32-bit transfer: D0/D1 → D10/D11 |
| DMOVP D0 D10 | 32-bit transfer once on that rising edge |

`D` is a width modifier, **not** a pulse flag or a floating-point flag. `P` is an
edge-execution modifier, **not** an additional operand. Not every command accepts
both, and pulse-output instructions such as PLSY are not ordinary MOVP-like data
operations. Consult the instruction's 16-bit / 32-bit / PULSE-P marking and operand
table before generating variants. An inactive instruction must still be decoded
and skipped by its correct encoded length.

### 5.1 Full model-filtered applied-command inventory

The following inventory is taken from the **model-shaded columns in §11.3**, not
from all the commands in chapter 5. “Both” means documented availability on FX1S
and FX1N, **not implementation in PLCFX1**. Each FNC is decimal. Instruction-specific
hardware/firmware restrictions still apply.

The three inline-comparison families are expanded after the table so no relation
is hidden by a range label. Detailed semantics, operands and variants remain in the
linked full manual; the descriptions below identify each instruction's purpose.

| Decimal FNC | Instruction / family | Profile | Purpose | Body reference |
|---|---|---|---|---|
| 0 | CJ | Both | Jumps to the identified program pointer when active. | §5.1.1, p. 5-5 |
| 1 | CALL | Both | Executes the subroutine beginning at the identified program pointer. | §5.1.2, p. 5-7 |
| 2 | SRET | Both | Returns program flow from a subroutine to the step after its CALL. | §5.1.3, p. 5-9 |
| 3 | IRET | Both | Returns from the active interrupt routine to the interrupted program. | §5.1.4, p. 5-10 |
| 4 | EI | Both | Enables interrupt inputs to be processed. | §5.1.4, p. 5-10 |
| 5 | DI | Both | Disables processing of interrupt routines. | §5.1.4, p. 5-10 |
| 6 | FEND | Both | Marks the end of the main program block and start of the subroutine/interrupt area. | §5.1.5, p. 5-12 |
| 7 | WDT | Both | Refreshes the PLC watchdog timer during a program scan. | §5.1.6, p. 5-13 |
| 8 | FOR | Both | Marks the start of a loop and specifies its repeat count. | §5.1.7, p. 5-14 |
| 9 | NEXT | Both | Marks the end of a FOR-NEXT loop. | §5.1.7, p. 5-14 |
| 10 | CMP | Both | Compares two data values and reports less-than, equal and greater-than results. | §5.2.1, p. 5-18 |
| 11 | ZCP | Both | Compares a data value against a range and reports below, within or above the range. | §5.2.2, p. 5-19 |
| 12 | MOV | Both | Moves data from one storage area to another. | §5.2.3, p. 5-20 |
| 15 | BMOV | Both | Copies a specified block of consecutive data elements to a new destination. | §5.2.6, p. 5-23 |
| 18 | BCD | Both | Converts binary data to its BCD equivalent. | §5.2.9, p. 5-26 |
| 19 | BIN | Both | Converts BCD data to its binary equivalent. | §5.2.10, p. 5-27 |
| 20 | ADD | Both | Adds two source values and stores the result. | §5.3.1, p. 5-30 |
| 21 | SUB | Both | Subtracts one source value from another and stores the result. | §5.3.2, p. 5-31 |
| 22 | MUL | Both | Multiplies two source values and stores the result. | §5.3.3, p. 5-32 |
| 23 | DIV | Both | Divides one source value by another and stores the result. | §5.3.4, p. 5-33 |
| 24 | INC | Both | Increments the designated device value by one when executed. | §5.3.5, p. 5-34 |
| 25 | DEC | Both | Decrements the designated device value by one when executed. | §5.3.6, p. 5-35 |
| 26 | WAND | Both | Performs a logical AND on source word bit patterns and stores the result. | §5.3.7, p. 5-36 |
| 27 | WOR | Both | Performs a logical OR on source word bit patterns and stores the result. | §5.3.8, p. 5-37 |
| 28 | WXOR | Both | Performs a logical exclusive-OR on source word bit patterns and stores the result. | §5.3.9, p. 5-38 |
| 34 | SFTR | Both | Copies source bits into a controlled bit stack, shifting existing data to the right. | §5.4.5, p. 5-47 |
| 35 | SFTL | Both | Copies source bits into a controlled bit stack, shifting existing data to the left. | §5.4.6, p. 5-48 |
| 38 | SFWR | Both | Writes source data into a FIFO shift-register stack. | §5.4.9, p. 5-51 |
| 39 | SFRD | Both | Reads data from and reduces a FIFO shift-register stack. | §5.4.10, p. 5-52 |
| 40 | ZRST | Both | Resets a range of like devices in one operation. | §5.5.1, p. 5-55 |
| 41 | DECO | Both | Decodes a source value to identify and turn on the corresponding bit in a destination range. | §5.5.2, p. 5-56 |
| 42 | ENCO | Both | Encodes the location of the highest active bit as a numerical position. | §5.5.3, p. 5-57 |
| 50 | REF | Both | Forces an immediate refresh of specified inputs or outputs. | §5.6.1, p. 5-68 |
| 52 | MTR | Both | Multiplexes a bank of inputs into specified device sets (input matrix). | §5.6.3, p. 5-70 |
| 53 | HSCS | Both | Sets a selected output when a high-speed counter equals the specified test value. | §5.6.4, p. 5-72 |
| 54 | HSCR | Both | Resets a selected output when a high-speed counter equals the specified test value. | §5.6.5, p. 5-74 |
| 56 | SPD | Both | Detects encoder pulses within a specified time frame for speed measurement. | §5.6.7, p. 5-78 |
| 57 | PLSY | Both | Outputs a specified quantity of pulses at a specified frequency. | §5.6.8, p. 5-79 |
| 58 | PWM | Both | Generates a pulse train with defined pulse characteristics. | §5.6.9, p. 5-80 |
| 59 | PLSR | Both | Outputs a specified pulse quantity while ramping up to a set frequency and back down to stop. | §5.6.10, p. 5-81 |
| 60 | IST | Both | Automatically sets up a multi-mode STL operating system. | §5.7.1, p. 5-85 |
| 62 | ABSD | Both | Absolute drum sequencer that generates multiple output patterns in response to counter data. | §5.7.3, p. 5-88 |
| 63 | INCD | Both | Incremental drum sequencer that generates an output sequence in response to counter data. | §5.7.4, p. 5-89 |
| 66 | ALT | Both | Inverts the status of the designated device on each operation. | §5.7.7, p. 5-92 |
| 67 | RAMP | Both | Ramps a device from one value to another over a specified number of steps. | §5.7.8, p. 5-93 |
| 72 | DSW | Both | Multiplexes the reading of digital thumbwheel switches. | §5.8.3, p. 5-104 |
| 74 | SEGL | Both | Writes data to multiplexed single-digit seven-segment displays with latch control. | §5.8.5, p. 5-107 |
| 78 | FROM | FX1N only | Reads data from the buffer memory of an attached special-function block. | §5.8.9, p. 5-112 |
| 79 | TO | FX1N only | Writes data to the buffer memory of an attached special-function block. | §5.8.10, p. 5-114 |
| 80 | RS | Both | Controls serial communications to and from the programmable controller. | §5.9.1, p. 5-118 |
| 81 | PRUN | Both | Controls FX parallel-link adapters by moving bit data to/from link data areas; its operation uses octal device addressing. | §5.9.2, pp. 5-119–5-120 |
| 82 | ASCI | Both | Converts hexadecimal data to the equivalent ASCII codes. | §5.9.3, p. 5-121 |
| 83 | HEX | Both | Converts ASCII data to its hexadecimal equivalent. | §5.9.4, p. 5-122 |
| 84 | CCD | Both | Checks vertical parity of a data stack and sums the stack data. | §5.9.5, p. 5-123 |
| 85 | VRRD | Both | Reads an analog value from one of the FX-8AV volume inputs. | §5.9.6, p. 5-124 |
| 86 | VRSC | Both | Reads a selected FX-8AV volume input as a set position value from 0 to 10. | §5.9.7, p. 5-125 |
| 88 | PID | FX1N only | Processes a data input through a PID control loop to calculate a corrective output. | §5.9.8, p. 5-126 |
| 155 | ABS (DABS form) | Both | Reads absolute position data from a compatible servo motor. | §5.13.6, p. 5-164 |
| 156 | ZRN | Both | Returns the machine to its zero/home point after startup or initial setting. | §5.13.7, p. 5-165 |
| 157 | PLSV | Both | Outputs variable-speed pulses with a rotation-direction output. | §5.13.8, p. 5-167 |
| 158 | DRVI | Both | Performs incremental (relative) positioning. | §5.13.9, p. 5-168 |
| 159 | DRVA | Both | Performs absolute positioning from a zero/home point. | §5.13.10, p. 5-170 |
| 160 | TCMP | Both | Compares two times and reports less-than, equal and greater-than results. | §5.14.1, p. 5-175 |
| 161 | TZCP | Both | Compares a time against a specified time range and reports the relation to that range. | §5.14.2, p. 5-176 |
| 162 | TADD | Both | Adds two time values to produce a new time value. | §5.14.3, p. 5-177 |
| 163 | TSUB | Both | Subtracts one time value from another to produce a new time value. | §5.14.4, p. 5-178 |
| 166 | TRD | Both | Reads the current real-time-clock value to a group of registers. | §5.14.5, p. 5-179 |
| 167 | TWR | Both | Sets the real-time clock to the value stored in a group of registers. | §5.14.6, p. 5-180 |
| 169 | HOUR (DHOUR form) | Both | Hour meter that accumulates time and activates an alarm output at the set period. | §5.14.7, p. 5-181 |
| 176 | RD3A | FX1N only | Reads an analog value from an FX0N-3A or FX2N-2AD analog block (FX1N compatibility is limited to FX0N-3A). | §5.15.3, p. 5-187 |
| 177 | WR3A | FX1N only | Writes data to an FX0N-3A, FX2N-2AD or FX2N-2DA analog block (FX1N compatibility is limited to FX0N-3A). | §5.15.4, p. 5-188 |
| 224-230 | LD comparison family | Both | Initial comparison contact that is active when the selected comparison between two source values is true. | §5.17.1, p. 5-218 |
| 232-238 | AND comparison family | Both | Serial comparison contact that is active when the selected comparison between two source values is true. | §5.17.2, p. 5-219 |
| 240-246 | Or comparison family | Both | Parallel comparison contact that is active when the selected comparison between two source values is true. | §5.17.3, p. 5-220 |

### 5.2 Inline comparison contacts

These six relations are available in each family, with permitted 16-/32-bit forms:

| Relation | Load | AND | OR | Decimal FNC: LD / AND / OR |
|---|---|---|---|---|
| Equal | LD= | AND= | OR= | 224 / 232 / 240 |
| Greater | LD> | AND> | OR> | 225 / 233 / 241 |
| Less | LD< | AND< | OR< | 226 / 234 / 242 |
| Not equal | LD<> | AND<> | OR<> | 228 / 236 / 244 |
| Less or equal | LD<= | AND<= | OR<= | 229 / 237 / 245 |
| Greater or equal | LD>= | AND>= | OR>= | 230 / 238 / 246 |

The intervening FNC values **227, 235 and 243 are not an extra relation**. The D
prefix selects the documented double-word comparison, e.g. `DLD>= D0 K100000`.
These commands contribute a Boolean rung result; they do not write three result
bits like CMP. See the actual inline-comparison section in chapter 5 (its section
number differs from the stale internal contents listing).

```text
LD>= D0 K100
AND< D0 K200
OUT M10
END
```

M10 is ON for the signed 16-bit interval **100 ≤ D0 < 200**.

### 5.3 Commands outside these profiles

The following named rows in the same official index are **not marked as available
on FX1S or FX1N**. Their appearance in a shared FX-family manual is not evidence
that FX1S/FX1N must execute them:

SMOV (FNC 13), CML (FNC 14), FMOV (FNC 16), XCH (FNC 17), NEG (FNC 29), ROR (FNC 30), ROL (FNC 31), RCR (FNC 32), RCL (FNC 33), WSFR (FNC 36), WSFL (FNC 37), SUM (FNC 43), BON (FNC 44), MEAN (FNC 45), ANS (FNC 46), ANR (FNC 47), SQR (FNC 48), FLT (FNC 49), REFF (FNC 51), HSZ (FNC 55), SER (FNC 61), TTMR (FNC 64), STMR (FNC 65), ROTC (FNC 68), SORT (FNC 69), TKY (FNC 70), HKY (FNC 71), SEGD (FNC 73), ARWS (FNC 75), ASC (FNC 76), PR (FNC 77), ECMP (FNC 110), EZCMP (FNC 111), EBCD (FNC 118), EBIN (FNC 119), EADD (FNC 120), ESUB (FNC 121), EMUL (FNC 122), EDIV (FNC 123), ESQR (FNC 127), INT (FNC 129), SIN (FNC 130), COS (FNC 131), TAN (FNC 132), SWAP (FNC 147), GRY (FNC 170), GBIN (FNC 171).

EXTR (FNC 180), described elsewhere in chapter 5 but omitted from that alphabetical
index, is likewise an FX2N/FX2NC external-ROM function, not an FX1S/FX1N feature.
The corrected mnemonic **ESQR** is used here; the index misprints it as ESOR.
Do not borrow unsupported floating-point, trigonometric or data-manipulation
commands from FX2N/FX3U just because the interpreter contains a similarly named
function.

### 5.4 Counting caveat

Chapter 8 gives headline counts of **85 applied instructions for FX1S** and **89
for FX1N**, with larger counts when variants are included. The §11.3 index yields
69 FX1S family rows and 74 FX1N family rows; expanding each of its three six-relation
comparison rows gives **84 and 89 named entries** respectively. The FX1S difference
is a source inconsistency, not a license to invent an 85th command. This tutorial
publishes the explicit list and the discrepancy rather than claiming that a
headline count proves completeness or compatibility. Use the individual instruction
model markings to resolve any disputed function.

## 6. Practical programming patterns

### 6.1 Continuous execution versus one-shot execution

```text
LD   X0
INC  D0
LD   X1
INCP D1
END
```

While X0 stays ON, D0 increments each enabled scan. D1 increments once when X1
changes OFF→ON, then waits for a new edge. `DINC` operates on a double word; it
must not accidentally acquire INCP's edge behavior. Signed overflow/status handling
must follow the arithmetic instruction and its documented flags.

### 6.2 Compare result bits

```text
LD  M8000
CMP D0 K100 M10
END
```

For CMP, **M10 means D0 > 100, M11 means D0 = 100, M12 means D0 < 100**. Reserve
all three consecutive bits. ZCP likewise uses three bits, but for below / inside /
above the specified zone; the ordering is not interchangeable with CMP.

### 6.3 Integer result storage and block transfers

- `ADD D0 D1 D10` writes a 16-bit sum to D10; `DADD D0 D2 D10` reads/writes
  double-word pairs. Sources D0 and D1 would overlap if used as independent
  double-word operands.
- `MUL D0 D1 D10` writes a **32-bit** result to D10/D11 even though its sources
  are 16-bit. DMUL writes a 64-bit result to four words.
- `DIV D0 D1 D10` stores quotient in D10 and remainder in D11. DDIV uses a
  double-word quotient and double-word remainder. Check a zero divisor and reserve
  the entire result area.
- `BMOV D0 D10 K3` transfers D0,D1,D2 to D10,D11,D12. Ordinary D-word stepping
  is consecutive; internal byte offsets must not turn it into D10,D12,D14.
- `BCD` and `BIN` convert integer and packed BCD; neither is a floating-point
  conversion. Invalid BCD digits are an error, not arbitrary hexadecimal data.

### 6.4 Subroutines and scan termination

```text
LD   X0
CALL P0
FEND
P0
LD   M8000
INC  D0
SRET
END
```

This increments D0 on each scan in which P0 is called. Main-program FEND prevents
fall-through into the subroutine. CALL saves a return address; SRET restores it.
Nested calls require separate return-stack entries. The documented maximum is five
levels including the initial CALL. RET is only the STL terminator. Do not reuse a
pointer for incompatible CJ/CALL purposes or use a special end-jump pointer as a
normal subroutine label. Timers inside skipped, called or interrupt code require
the instruction/device-specific rules; ordinary timer examples cannot simply be
moved into a subroutine unchanged.

### 6.5 Hardware-bound functions

REF, MTR, HSCS/HSCR, SPD, PLSY, PWM, PLSR, ZRN, PLSV, DRVI/DRVA, RS and other
external-I/O instructions depend on actual hardware resources, assignments and
timing. A relay-output unit cannot generate high-speed pulses through relay
contacts. Mitsubishi's pulse frequency limits and terminal assignments are not
STM32 BluePill specifications. FROM/TO access special-function block buffer memory;
they do not mean arbitrary reads/writes of STM32 addresses.

## 7. Applying this reference to PLCFX1 on BluePill

The current project targets **STM32F103C8, 64 KB Flash / 20 KB SRAM**. Its README
documents **6000 program steps** and ordinary **D0–D1999** storage. These are custom
project limits, not official FX1S's 2K or FX1N's 8K program capacity, and not proof
of the corresponding Mitsubishi device semantics.

The firmware's physical PLC-input/output hooks are still incomplete. It also has
incomplete instruction handlers and known interpreter defects. Consequently:

1. **Absent model feature:** a command excluded from FX1S is not a defect in an
   FX1S-only target merely because FX2N/FX3U supports it.
2. **Missing implementation:** a command required by the chosen profile but mapped
   to a stub, missing handler or error path is not supported by PLCFX1.
3. **Incorrect implementation:** a shared basic command with wrong timer presets,
   RST behavior, edge lifetime, P/D decoding, word stepping or return-stack behavior
   is an implementation defect, not a difference between FX generations.
4. **Encoding mismatch:** matching a mnemonic or decimal FNC number does not prove
   compatible encoded programs, device addressing or programming protocol.
5. **Hardware limitation:** implementing the arithmetic part of a positioning or
   high-speed-counter function does not implement its external signaling.

The earlier comparison with the old tutorial was coverage of that tutorial's mixed
list, **not a percentage of FX1S or FX1N compatibility**. Treat its reproductions
as defect evidence to verify against this source, not as a vendor specification.

### Minimum conformance tests before claiming a supported profile

- Generate programs using the exact selected Mitsubishi CPU profile. Verify binary
  decoding, operand lengths and PC advancement with both true and false enables.
- Exercise each permitted operand class and boundary; distinguish contact bits from
  timer/counter current values and special registers from ordinary D registers.
- Verify all normal/P/D/DP forms actually allowed by the instruction's model table.
- Test edges over several scans, held conditions, STOP/RUN and skipped/called code.
- Test RST of bits, T/C values and contacts, D/V/Z, and MC/MCR inactive behavior.
- Test time bases, reset/retention, carry/borrow/status flags, signed bounds, division
  errors and the complete multiword result extent.
- Test nested CALL/SRET, balanced branch stacks, STL transitions and error handling.
- Validate physical I/O, pulse generation, interrupts, retention and communications
  on the actual board. IAR/GNU compilation alone does not establish compatibility.

## 8. Revision notes

V2.0 replaces the unofficial mixed-family bytecode tutorial with an English,
model-specific FX1S baseline and explicit FX1N differences. It adds official source
links, the complete index-derived command inventory, all inline-comparison
relations, basic/STL semantics, practical examples and clearly separated firmware
limitations. **This is a documentation-only change; it does not repair or certify
the instruction interpreter.**
