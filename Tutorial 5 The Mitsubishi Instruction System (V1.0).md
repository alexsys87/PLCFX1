# Tutorial 5: The Mitsubishi Instruction System (V1.0)

Oct 10, 2026 · @Alex

## 5.1 The Mitsubishi Instruction System

The Mitsubishi instruction system has been studied in considerable depth in China, and as a result Mitsubishi-compatible clone PLCs made in China are also the most numerous. The reason is that Mitsubishi PLCs are relatively simple, mainly in the following respects:

1. The communication protocol is simple, with only about 4–5 commands (read/write, force on/force off, etc.).
2. Communication commands use absolute addressing, so the internal structure is easy to infer.

### 5.1.1 Mitsubishi instruction byte codes

The layout of Mitsubishi's instruction byte codes is quite unreasonable, but from another angle it also shows that early PLCs were relatively weak in function and control capability. As PLCs developed, their functions kept growing, but to preserve compatibility they were usually extended by patching; by today's standards this looks highly unreasonable. This tutorial uses the FX1S series PLC as its example.

An instruction byte code is a string of binary code formed by arranging the opcode, variable addresses and constants of an instruction in a certain order, byte by byte (or word by word).

The variable addresses in Mitsubishi instruction byte codes are absolute addresses, divided as follows:

| Register name | Bit address | Offset | Actual address (ppp) | Space (maximum) |
| --- | --- | --- | --- | --- |
| S | `(S0~127 = 0~2F)` | 0 | `0000~007F` | 128 \* 8 = 1024 bits |
| X (octal) | `(X000~017 = 0~01)` | 80 | `0080~09F` | 32 \* 8 = 256 |
| Y (octal) | `(Y000~015 = 0~01)` | A0 | `00A0~00BF` | 32 \* 8 = 256 |
| T | `(T0~63: 0~3F)` | C0 | `00C0~00FF` | 64 \* 4 = 256 |
| M | `(M0~511 = 0~40)` | 100 | `0100~01BF` | 192 \* 8 = 1536 |
| C | `(C0~31: 0~1F)` | 1C0 | `01C0~029F` | 224 |
| PY | () | 2A0 | `02A0~02BF` | 32 \* 8 = 256 |
| OT | () | 2C0 | `02C0~02FF` | 64 \* 4 = 256 |
| PM | () | 300 | `0300~03BF` | 192 \* 8 = 1536 (`M1536~M3071`) |
| OC | () | 3C0 | `03C0~04BF` | 256 |
| RT | () | 4C0 | `04C0~05BF` | 256 |
| RC | () | 5C0 | `05C0~07FF` | 256 |
| TV | () | 800 | `0800~09FF` | 256 |
| CV16 | () | A00 | `0A00~0BFF` | 256 |
| CV32 | () | C00 | `0C00~0DFF` |  |
| `D8000~8255` | () | E00 | `0E00~0FFF` | 256 |
| `D0~D255` | `(0~1FF)` | 1000 | `0~11FF` |  |

### 5.1.2 Mitsubishi instruction formats

The instruction type of a Mitsubishi instruction byte code is determined by its first word, whose highest 4 bits are defined as follows:

| Top 4 bits | Meaning | Note |
| --- | --- | --- |
| 0 | Application instruction |  |
| 1 | P application instruction |  |
| 2 | LD |  |
| 3 | LDI |  |
| 4 | AND |  |
| 5 | ANI |  |
| 6 | OR |  |
| 7 | ORI |  |
| 8 | Data/address | Multi-word instructions; valid from the second word onward |
| A | Address | Valid only for M1536–M3071; an offset of 200 must be added |
| C | OUT | Valid only for Y, M |
| D | SET | Valid only for Y, M |
| E | RST | Valid only for Y, M |
| F | Pure single-word instruction |  |

#### 5.1.2.1 Single-word format

In single-word format, one word (a 16-bit binary number) represents one instruction, for example:

**Pure single-word instructions**

| Instruction | Code |
| --- | --- |
| END | 000F |
| ANB | FFF8 |
| ORB | FFF9 |
| MPS | FFFA |
| MRD | FFFB |
| MPP | FFFC |
| INV | FFFD |
| NOP | FFFF |

**Single-word instructions**

| Instruction | Code | Note |
| --- | --- | --- |
| LD | 2000+ppp | Except extended Mp |
| LDI | 3000+ppp | Except extended Mp |
| AND | 4000+ppp | Except extended Mp |
| ANI | 5000+ppp | Except extended Mp |
| OR | 6000+ppp | Except extended Mp |
| ORI | 7000+ppp | Except extended Mp |
| OUTYM | C000+ppp | Valid only for Y, M |
| SETYM | D000+ppp | Valid only for Y, M |
| RSTYM | E000+ppp | Valid only for Y, M |

#### 5.1.2.2 Double-word format

These are extended from the class-0 application instructions.

**Class 0**

| Instruction | Opcode (word 1) | Operand (following words) | Note |
| --- | --- | --- | --- |
| OUT | 0002 | 8000+ppp | Valid only for M8xxx |
| OUT | 0002 | A000+ppp | Valid only for Mp |
| SET | 0003 | 8000+ppp | Valid only for M8xxx |
| SET | 0003 | A000+ppp | Valid only for Mp |
| RST | 0004 | 8000+ppp | Valid only for M8xxx |
| RST | 0004 | A000+ppp | Valid only for Mp |
| OUTS | 0005 | 8000+ppp | Valid only for S |
| SETS | 0006 | 8000+ppp | Valid only for S |
| RSTS | 0007 | 8000+ppp | Valid only for S |
| PLS | 0008 | 8000+ppp | Valid only for Y, M |
| PLF | 0009 | 8000+ppp | Valid only for Y, M |
| MC | 000A | 8000+(N) 8000+ppp | Valid only for Y, M |
| MCR | 000B | 8000+(N) | N = 0–7 |
| RSTTC | 000C | 8000+ppp | Valid only for T, C, Cp |
| RSTD | 000D | 8m00+xx 8n00+yy | Valid only for D, including Z, V |

**Class 01**

| Instruction | Opcode (word 1) | Operand (word 2) | Note |
| --- | --- | --- | --- |
| MLD | 01C2 | A000+ppp | Valid only for Mp |
| MLDI | 01C3 | A000+ppp | Valid only for Mp |
| MAND | 01C4 | A000+ppp | Valid only for Mp |
| MANI | 01C5 | A000+ppp | Valid only for Mp |
| MOR | 01C6 | A000+ppp | Valid only for Mp |
| MORI | 01C7 | A000+ppp | Valid only for Mp |
| MLDP | 01CA | 8000+ppp | Except extended Mp |
| MLDP | 01CA | A000+ppp | Valid only for Mp |
| MLDF | 01CB | 8000+ppp | Except extended Mp |
| MLDF | 01CB | A000+ppp | Valid only for Mp |
| MANDP | 01CC | 8000+ppp | Except extended Mp |
| MANDP | 01CC | A000+ppp | Valid only for Mp |
| MANDF | 01CD | 8000+ppp | Except extended Mp |
| MANDF | 01CD | A000+ppp | Valid only for Mp |
| MORP | 01CE | 8000+ppp | Except extended Mp |
| MORP | 01CE | A000+ppp | Valid only for Mp |
| MORF | 01CF | 8000+ppp | Except extended Mp |
| MORF | 01CF | A000+ppp | Valid only for Mp |

#### 5.1.2.3 Three-word format

| Instruction | Word 1 | Word 2 | Word 3 | Word 4 | Word 5 |
| --- | --- | --- | --- | --- | --- |
| OUT T K | 0000+(T) | VV00+xx | VV00+yy |  |  |
| OUT C K | 0000+(C) | VV00+xx | VV00+yy |  |  |
| OUT Cp K | 0000+(Cp) | VV00+xx | VV00+yy | 8000+zz | 8000+ww |

#### 5.1.2.4 Formats longer than three words

Besides the basic instructions above, Mitsubishi has another class of instructions called application instructions, whose format is:

Opcode + variable address 1 (or constant 1) + variable address 2 (or constant 2) + …

Variable address formats in Mitsubishi application instructions:

- 16-bit variable address format = `VV00+xx, VV00+yy`
- 32-bit variable address format = `VV00+xx, VV00+yy, 8000+zz, 8000+ww`
- Bit (point) device address format = `VV00+xx, VV00+yy`

Mitsubishi's way of representing variables is very strange. A 16-bit variable takes 2 words: the high 8 bits of the first word and the high 8 bits of the second word together form the variable-type descriptor, and the low 8 bits of the first word and the low 8 bits of the second word together form the variable address or constant.

A 32-bit variable takes 4 words: the high 8 bits of the first word and the high 8 bits of the second word form the variable-type descriptor; the high 8 bits of the third word and of the fourth word are fixed at 0x80; and the low 8 bits of the first, second, third and fourth words together form the 32-bit variable address or constant.

This arrangement artificially adds to the complexity of parsing, lowers instruction execution speed and wastes program memory. The variable-type descriptor table (VV) follows.

| No. | 1st word, high 8 bits | 2nd word, high 8 bits | Variable type | Remarks |
| --- | --- | --- | --- | --- |
| 1 | 80 | 80 | K | Constant |
| 2 | 82 | 80 | H | Constant |
| 3 | 84 | 80 | Bit variable (X, Y, M, S, M8, C, T) |  |
| 4 | 84 | 82 | K1 | 4 bit |
| 5 | 84 | 84 | K2 | 8 bit |
| 6 | 84 | 86 | K3 | 12 bit |
| 7 | 84 | 88 | K4 | 16 bit |
| 8 | 84 | 8A | K5 | 20 bit |
| 9 | 84 | 8C | K6 | 24 bit |
| 10 | 84 | 8E | K7 | 28 bit |
| 11 | 84 | 90 | K8 | 32 bit |
| 12 | 86 | 80 | D8xxx |  |
| 13 | 86 | 82 | Txxx |  |
| 14 | 86 | 84 | Cxxx |  |
| 15 | 86 | 86 | Dxxx | `D < 1000` |
| 16 | 86 | 88 | Dxxxx | `D > 1000` |
| 17 | 88 | 80 | Pxxx |  |
| 18 | 8A | 80 | ASCII CHAR |  |
| 19 | 90 | 80 | KxxxxV |  |
| 20 | 94 | 82 | K1(X,Y,M,S)V |  |
| 21 | 94 | 84 | K2(X,Y,M,S)V |  |
| 22 | 94 | 86 | K3(X,Y,M,S)V |  |
| 23 | 94 | 88 | K4(X,Y,M,S)V |  |
| 24 | 94 | 8A | K5(X,Y,M,S)V |  |
| 25 | 94 | 8C | K6(X,Y,M,S)V |  |
| 26 | 94 | 8E | K7(X,Y,M,S)V |  |
| 27 | 94 | 90 | K8(X,Y,M,S)V |  |
| 28 | A4 | 82 | K1(X,Y,M,S)Z |  |
| 29 | A4 | 84 | K2(X,Y,M,S)Z |  |
| 30 | A4 | 86 | K3(X,Y,M,S)Z |  |
| 31 | A4 | 88 | K4(X,Y,M,S)Z |  |
| 32 | A4 | 8A | K5(X,Y,M,S)Z |  |
| 33 | A4 | 8C | K6(X,Y,M,S)Z |  |
| 34 | A4 | 8E | K7(X,Y,M,S)Z |  |
| 35 | A4 | 90 | K8(X,Y,M,S)Z |  |
| 36 | A6 | 80 | D8xxxZ |  |
| 37 | A6 | 82 | TxxxZ |  |
| 38 | A6 | 84 | CxxxZ |  |
| 39 | A6 | 86 | DxxxZ | `D < 1000` |
| 40 | A6 | 88 | DxxxxZ | `D > 1000` |

### 5.1.3 Mitsubishi instruction table

**Program flow**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 00 | CJ | 0x0010 | Conditional jump | 2 | ○ | ○ | ○ |
| 01 | CALL | 0x0012 | Subroutine call | 2 | ○ | ○ | ○ |
| 02 | SRET | 0x0014 | Subroutine return | 2 | ○ | ○ | ○ |
| 03 | IRET | 0x0016 | Interrupt return | 2 | ○ | ○ | ○ |
| 04 | EI | 0x0018 | Enable interrupts | 2 | ○ | ○ | ○ |
| 05 | DI | 0x001A | Disable interrupts | 2 | ○ | ○ | ○ |
| 06 | FEND | 0x001C | End of main program | 2 | ○ | ○ | ○ |
| 07 | WDT | 0x001E | Watchdog timer refresh | 2 | ○ | ○ | ○ |
| 08 | FOR | 0x0020 | Start of loop and loop count | 2 | ○ | ○ | ○ |
| 09 | NEXT | 0x0022 | End of loop | 2 | ○ | ○ | ○ |

**Transfer and comparison**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 10 | CMP | 0x0024 | Compare | 7 | ○ | ○ | ○ |
| 11 | ZCP | 0x0026 | Zone compare | 7 | ○ | ○ | ○ |
| 12 | MOV | 0x0028 | Transfer | 5 | ○ | ○ | ○ |
| 13 | SMOV | 0x002A | Bit transfer | 11 | - | - | ○ |
| 14 | CML | 0x002C | Inverted transfer | 5 | - | - | ○ |
| 15 | BMOV | 0x002E | Block transfer | 7 | ○ | ○ | ○ |
| 16 | FMOV | 0x0030 | Multi-point transfer | 7 | - | - | ○ |
| 17 | XCH | 0x0032 | Exchange | 5 | - | - | ○ |
| 18 | BCD | 0x0034 | Binary to BCD conversion | 5 | ○ | ○ | ○ |
| 19 | BIN | 0x0036 | BCD to binary conversion | 5 | ○ | ○ | ○ |

**Arithmetic and logic operations**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 20 | ADD | 0x0038 | Binary addition | 7 | ○ | ○ | ○ |
| 21 | SUB | 0x003A | Binary subtraction | 7 | ○ | ○ | ○ |
| 22 | MUL | 0x003C | Binary multiplication | 7 | ○ | ○ | ○ |
| 23 | DIV | 0x003E | Binary division | 7 | ○ | ○ | ○ |
| 24 | INC | 0x0040 | Binary increment (add 1) | 3 | ○ | ○ | ○ |
| 25 | DEC | 0x0042 | Binary decrement (subtract 1) | 3 | ○ | ○ | ○ |
| 26 | WAND | 0x0044 | Word logical AND | 7 | ○ | ○ | ○ |
| 27 | WOR | 0x0046 | Word logical OR | 7 | ○ | ○ | ○ |
| 28 | WXOR | 0x0048 | Word logical XOR | 7 | ○ | ○ | ○ |
| 29 | NEG | 0x004A | Binary complement (two's complement) | 3 | - | - | ○ |

**Rotation and shift**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 30 | ROR | 0x004C | Rotate right | 5 | - | - | ○ |
| 31 | ROL | 0x004E | Rotate left | 5 | - | - | ○ |
| 32 | RCR | 0x0050 | Rotate right with carry | 5 | - | - | ○ |
| 33 | RCL | 0x0052 | Rotate left with carry | 5 | - | - | ○ |
| 34 | SFTR | 0x0054 | Bit shift right | 7 | ○ | ○ | ○ |
| 35 | SFTL | 0x0056 | Bit shift left | 7 | ○ | ○ | ○ |
| 36 | WSFR | 0x0058 | Word shift right | 9 | - | - | ○ |
| 37 | WSFL | 0x005A | Word shift left | 9 | - | - | ○ |
| 38 | SFWR | 0x005C | FIFO (first-in, first-out) write | 7 | ○ | ○ | ○ |
| 39 | SFRD | 0x005E | FIFO (first-in, first-out) read | 7 | ○ | ○ | ○ |

**Data processing**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 40 | ZRST | 0x0060 | Zone reset | 5 | ○ | ○ | ○ |
| 41 | DECO | 0x0062 | Decode | 7 | ○ | ○ | ○ |
| 42 | ENCO | 0x0064 | Encode | 7 | ○ | ○ | ○ |
| 43 | SUM | 0x0066 | Count ON bits | 5 | - | - | ○ |
| 44 | BON | 0x0068 | Check bit state | 7 | - | - | ○ |
| 45 | MEAN | 0x006A | Mean (average) | 7 | - | - | ○ |
| 46 | ANS | 0x006C | Alarm set | 7 | - | - | ○ |
| 47 | ANR | 0x006E | Alarm reset | 1 | - | - | ○ |
| 48 | SQR | 0x0070 | Square root | 5 | - | - | ○ |
| 49 | FLT | 0x0072 | Conversion between integer and floating-point number | 5 | - | - | ○ |

**High-speed processing**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 50 | REF | 0x0074 | I/O refresh | 5 | ○ | ○ | ○ |
| 51 | REFF | 0x0076 | Input filter time adjustment | 3 | - | - | ○ |
| 52 | MTR | 0x0078 | Matrix input | 9 | ○ | ○ | ○ |
| 53 | HSCS | 0x007A | Compare set (for high-speed counters) | 13 | ○ | ○ | ○ |
| 54 | HSCR | 0x007C | Compare reset (for high-speed counters) | 13 | ○ | ○ | ○ |
| 55 | HSZ | 0x007E | Zone compare (for high-speed counters) | 17 | - | - | ○ |
| 56 | SPD | 0x0080 | Pulse density | 7 | ○ | ○ | ○ |
| 57 | PLSY | 0x0082 | Pulse output at a specified frequency | 7 | ○ | ○ | ○ |
| 58 | PWM | 0x0084 | Pulse-width modulation output | 7 | ○ | ○ | ○ |
| 59 | PLSR | 0x0086 | Pulse output with acceleration/deceleration | 7 | ○ | ○ | ○ |

**Handy instructions**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 60 | IST | 0x0088 | State initialization | 7 | ○ | ○ | ○ |
| 61 | SER | 0x008A | Data search | 7 | - | - | ○ |
| 62 | ABSD | 0x008C | Cam control (absolute mode) | 9 | ○ | ○ | ○ |
| 63 | INCD | 0x008E | Cam control (incremental mode) | 9 | ○ | ○ | ○ |
| 64 | TTMR | 0x0090 | Teaching timer | 5 | ○ | ○ | ○ |
| 65 | STMR | 0x0092 | Special timer | 7 | - | - | ○ |
| 66 | ALT | 0x0094 | Alternate output | 3 | ○ | ○ | ○ |
| 67 | RAMP | 0x0096 | Ramp signal | 9 | ○ | ○ | ○ |
| 68 | ROTC | 0x0098 | Rotary table control | 9 | - | - | ○ |
| 69 | SORT | 0x009A | Data sort | 17 | - | - | ○ |
| 70 | TKY | 0x009C | Decimal key input | 7 | - | - | ○ |
| 71 | HKY | 0x009E | Hexadecimal key input | 9 | - | - | ○ |
| 72 | DSW | 0x00A0 | Digital switch | 9 | ○ | ○ | ○ |
| 73 | SEGD | 0x00A2 | Seven-segment decoding | 5 | - | - | ○ |
| 74 | SEGL | 0x00A4 | Seven-segment display | 7 | ○ | ○ | ○ |
| 75 | ARWS | 0x00A6 | Direction switch | 9 | - | - | ○ |
| 76 | ASC | 0x00A8 | Convert ASCII code to hexadecimal | 11 | - | - | ○ |
| 77 | PR | 0x00AA | ASCII code print | 5 | - | - | ○ |
| 78 | FROM | 0x00AC | BFM read | 9 | - | ○ | ○ |
| 79 | TO | 0x00AE | BFM write | 9 | - | ○ | ○ |
| 80 | RS | 0x00B0 | Serial data transfer | 9 | ○ | ○ | ○ |
| 81 | PRUN | 0x00B2 | Octal bit transfer | 5 | ○ | ○ | ○ |
| 82 | ASCI | 0x00B4 | Convert hexadecimal to ASCII code | 7 | ○ | ○ | ○ |
| 83 | HEX | 0x00B6 | Convert ASCII code to hexadecimal | 7 | ○ | ○ | ○ |
| 84 | CCD | 0x00B8 | Check code | 7 | ○ | ○ | ○ |
| 85 | VRRD | 0x00BA | Potentiometer variable input | 5 | ○ | ○ | ○ |
| 86 | VRSC | 0x00BC | Potentiometer variable range | 5 | ○ | ○ | ○ |
| 87 | - |  | - | 0 |  |  |  |
| 88 | PID | 0x00C0 | PID operation | 9 | ○ | ○ | ○ |
| 89 | - |  | - | 0 |  |  |  |

**Floating-point operations**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 110 | ECMP | 0x00EC | Binary floating-point comparison | 13 | - | - | ○ |
| 111 | EZCP | 0x00EE | Binary floating-point zone comparison | 17 | - | - | ○ |
| 118 | EBCD | 0x00FC | Binary floating-point → decimal floating-point | 9 | - | - | ○ |
| 119 | EBIN | 0x00FE | Decimal floating-point → binary floating-point | 9 | - | - | ○ |
| 120 | EADD | 0x0100 | Binary floating-point addition | 13 | - | - | ○ |
| 121 | EUSB | 0x0102 | Binary floating-point subtraction | 13 | - | - | ○ |
| 122 | EMUL | 0x0104 | Binary floating-point multiplication | 13 | - | - | ○ |
| 123 | EDIV | 0x0108 | Binary floating-point division | 13 | - | - | ○ |
| 127 | ESQR | 0x010E | Binary floating-point square root | 9 | - | - | ○ |
| 129 | INT | 0x0112 | Binary floating-point → binary integer | 5 | - | - | ○ |
| 130 | SIN | 0x0114 | Binary floating-point Sin operation | 9 | - | - | ○ |
| 131 | COS | 0x0116 | Binary floating-point Cos operation | 9 | - | - | ○ |
| 132 | TAN | 0x0118 | Binary floating-point Tan operation | 9 | - | - | ○ |
| 147 | SWAP | 0x0136 | High/low byte swap | 3 | - | - | ○ |

**Positioning**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 155 | ABS | 0x0146 | Read ABS current value | 13 | ○ | ○ | - |
| 156 | ZRN | 0x0148 | Return to origin | 9 | ○ | ○ | - |
| 157 | PLSV | 0x014A | Variable-speed pulse output | 9 | ○ | ○ | - |
| 158 | DRVI | 0x014C | Relative position control | 9 | ○ | ○ | - |
| 159 | DRVA | 0x014E | Absolute position control | 9 | ○ | ○ | - |

**Clock operations**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 160 | TCMP | 0x0150 | Clock data comparison | 11 | ○ | ○ | ○ |
| 161 | TZCP | 0x0152 | Clock data zone comparison | 9 | ○ | ○ | ○ |
| 162 | TADD | 0x0154 | Clock data addition | 7 | ○ | ○ | ○ |
| 163 | TSUB | 0x0156 | Clock data subtraction | 7 | ○ | ○ | ○ |
| 166 | TRD | 0x015C | Clock data read | 3 | ○ | ○ | ○ |
| 167 | TWR | 0x015E | Clock data write | 3 | ○ | ○ | ○ |
| 169 | HOUR | 0x0162 | Hour meter | 7 | ○ | ○ | - |

**Peripheral devices**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 170 | GRY | 0x0164 | Binary → Gray code | 5 | - | - | ○ |
| 171 | GBIN | 0x0166 | Gray code → binary | 5 | - | - | ○ |
| 176 | RD3A | 0x0170 | Analog module (FX0N-3A) read | 7 | - | ○ | - |
| 177 | WR3A | 0x0172 | Analog module (FX0N-3A) write | 7 | - | ○ | - |

**Contact comparison**

| FNC No. | Mnemonic | Opcode | Function | Length | FX1S | FX1N | FX2N/FX2NC |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `216~223` |  | `0x01C0~0x01CF` | Already used |  |  |  |  |
| 224 | `LD=` | 0x01D0 | Starting contact ON when `(S1) = (S2)` | 5 | ○ | ○ | ○ |
| 225 | `LD>` | 0x01D2 | Starting contact ON when `(S1) > (S2)` | 5 | ○ | ○ | ○ |
| 226 | `LD<` | 0x01D4 | Starting contact ON when `(S1) < (S2)` | 5 | ○ | ○ | ○ |
| 228 | `LD<>` | 0x01D8 | Starting contact ON when `(S1) <> (S2)` | 5 | ○ | ○ | ○ |
| 229 | `LD≤` | 0x01DA | Starting contact ON when `(S1) ≤ (S2)` | 5 | ○ | ○ | ○ |
| 230 | `LD≥` | 0x01DC | Starting contact ON when `(S1) ≥ (S2)` | 5 | ○ | ○ | ○ |
| 232 | `AND=` | 0x01E0 | Series contact ON when `(S1) = (S2)` | 5 | ○ | ○ | ○ |
| 233 | `AND>` | 0x01E2 | Series contact ON when `(S1) > (S2)` | 5 | ○ | ○ | ○ |
| 234 | `AND<` | 0x01E4 | Series contact ON when `(S1) < (S2)` | 5 | ○ | ○ | ○ |
| 236 | `AND<>` | 0x01E8 | Series contact ON when `(S1) <> (S2)` | 5 | ○ | ○ | ○ |
| 237 | `AND≤` | 0x01EA | Series contact ON when `(S1) ≤ (S2)` | 5 | ○ | ○ | ○ |
| 238 | `AND≥` | 0x01EC | Series contact ON when `(S1) ≥ (S2)` | 5 | ○ | ○ | ○ |
| 240 | `OR=` | 0x01F0 | Parallel contact ON when `(S1) = (S2)` | 5 | ○ | ○ | ○ |
| 241 | `OR>` | 0x01F2 | Parallel contact ON when `(S1) > (S2)` | 5 | ○ | ○ | ○ |
| 242 | `OR<` | 0x01F4 | Parallel contact ON when `(S1) < (S2)` | 5 | ○ | ○ | ○ |
| 244 | `OR<>` | 0x01F8 | Parallel contact ON when `(S1) <> (S2)` | 5 | ○ | ○ | ○ |
| 245 | `OR≤` | 0x01FA | Parallel contact ON when `(S1) ≤ (S2)` | 5 | ○ | ○ | ○ |
| 246 | `OR≥` | 0x01FC | Parallel contact ON when `(S1) ≥ (S2)` | 5 | ○ | ○ | ○ |

## 5.2 Using Mitsubishi instructions

- Basic application instruction = `(FNC No. n + 8) * 2`
- D application instruction = `(FNC No. n + 8) * 2 + 1`
- P application instruction = `(FNC No. n + 8) * 2 + 1000`

For example:

| Instruction | Code | D instruction | Code |
| --- | --- | --- | --- |
| ADD | 0038 | DADD | 0039 |
| SUB | 003A | DSUB | 003B |
| MUL | 003C | DMUL | 003D |
| DIV | 003E | DDIV | 003F |

| Instruction | Code |
| --- | --- |
| ADDP | 1038 |
| DADDP | 1039 |
| SUBP | 103A |
| DSUBP | 103B |
| MULP | 103C |
| DMULP | 103D |
| DIVP | 103E |
| DDIVP | 103F |

### 5.2.1 Calculating the actual addresses of Zp and Vp

Offset:

- Z actual address `yy xx = Z address * 2`; `Z = p*2 + 1C` (p = 0): even address
- V actual address `yy xx = V address * 2`; `V = p*2 + 1C + 1` (p = 0): odd address
- Zp actual address `yy xx = Zp address * 2`; `Zp = p*2 + B4` (p = 1–7): even address
- Vp actual address `yy xx = Vp address * 2`; `Vp = p*2 + B4 + 1` (p = 1–7): odd address

The actual addresses of Zp and Vp are as follows:

| Z register | V register |
| --- | --- |
| D8028 : Z (Z0) | D8029 : V (V0) |
| D8182 : Z1 | D8183 : V1 |
| D8184 : Z2 | D8185 : V2 |
| D8186 : Z3 | D8187 : V3 |
| D8188 : Z4 | D8189 : V4 |
| D8190 : Z5 | D8191 : V5 |
| D8192 : Z6 | D8193 : V6 |
| D8194 : Z7 | D8195 : V7 |

## Translator's notes

- Translated from the Chinese PDF `_Command_system_of_Mitsubishi_PLC.pdf`. Tables, hexadecimal codes and addresses are reproduced as printed, including apparent typos; nothing was corrected silently.
- FNC 121 is printed as EUSB in the source. Judging by its description (binary floating-point subtraction) and its neighbours EADD, EMUL and EDIV, it is probably meant to be ESUB.
- FNC 123 EDIV is listed with opcode 0x0108, but the formula in section 5.2 gives (123 + 8) \* 2 = 0x0106. Every other opcode in the instruction table agrees with that formula.
- FNC 76 ASC has the same description as FNC 83 HEX ("convert ASCII code to hexadecimal"), which looks like a copy slip. On Mitsubishi FX PLCs, ASC converts characters to ASCII code.
- FNC 147 SWAP sits in its own unlabeled cell in the source, right after the floating-point group; it is shown here at the end of that group.
- In the address table, the bit-address range for S (0\~2F) and the actual address for D0\~D255 (0\~11FF) look inconsistent with the rest of their rows (S0\~127 = 0\~7F; 1000 + 0\~1FF = 1000\~11FF).
- In the formulas of section 5.2, "+ 1000" is hexadecimal (for example ADDP = 1038 = 0038 + 1000).
- The source gives no legend for the PLC-model columns: ○ is read as supported and - as not supported. "Length" is the source's "instruction length", given without units.
