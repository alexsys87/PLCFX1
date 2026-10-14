/**
 * @file plc_exp_handlers.h
 * @brief Prototypes of every dispatch target of the experimental interpreter.
 *
 * Each handler has the signature `void NAME(void)`: it reads its operands from the
 * program (via the operand layer), then acts on the PLC image. The dispatch tables in
 * plc_exp_dispatch.c refer to these names; opcode numbering is documented there.
 *
 * The prototypes are grouped by the module that defines the handler. The comment after a
 * prototype is the instruction in Mitsubishi notation (S = source, D = destination,
 * n = count). A name with the prefix D is the 32-bit form of the instruction without it
 * (DMOV is the 32-bit MOV); the float instructions (DEADD, DSIN, ...) always work on
 * 32-bit floating-point numbers. A few names differ from the mnemonic because the plain
 * name is taken: BSET implements SET, FUNC_TIMER and FUNC_COUNTER implement OUT Tn and
 * OUT Cn, RST_D and RST_TC implement RST on a word device and on a timer or counter.
 * FUNC_0, FUNC_F and FUNC_FF are routers that select a handler from the opcode bits.
 */
#ifndef PLC_EXP_HANDLERS_H
#define PLC_EXP_HANDLERS_H

/* ====================================================================== */
/* plc_exp_logic.c                                                        */
/* Contacts, coils, logic blocks, master control and step ladder          */
/* ====================================================================== */

void ERR(void);           /* unassigned opcode (ERR slot of a dispatch table): stops the PLC */
void NOP(void);           /* no operation; also the label words B0nn and the word 0000 */
void FUNC_0(void);        /* router for the words 0000..1FFF (extended and application instructions) */
void LD(void);            /* LD bit: load a normally open contact */
void LDI(void);           /* LDI bit: load a normally closed contact */
void AND(void);           /* AND bit: normally open contact in series */
void ANI(void);           /* ANI bit: normally closed contact in series */
void OR(void);            /* OR bit: normally open contact in parallel */
void ORI(void);           /* ORI bit: normally closed contact in parallel */
void OUT(void);           /* OUT bit: drive a coil (a step transition for an S device inside STL) */
void BSET(void);          /* SET bit: latch a coil ON (SET is an enumerator of the device header) */
void RST(void);           /* RST bit: clear a coil */
void FUNC_F(void);        /* router for the words F000..FFFF (STL, RET and the logic-block group) */
void STL(void);           /* STL Sn: step-ladder contact, opens a step block */
void RET(void);           /* RET: leave the step block opened by STL */
void FUNC_FF(void);       /* logic-block group FFnn: ANB, ORB, MPS, MRD, MPP, INV */
void PLS(void);           /* PLS bit: one-scan pulse on the rising edge of the rung */
void PLF(void);           /* PLF bit: one-scan pulse on the falling edge of the rung */
void MC(void);            /* MC bit: open a master-control level */
void MCR(void);           /* MCR: close the innermost master-control level */
void RST_D(void);         /* RST D: clear a word device */
void END(void);           /* END, FEND: end of the main program, finishes the scan */
void LDP(void);           /* LDP bit: load a rising-edge contact */
void LDF(void);           /* LDF bit: load a falling-edge contact */
void ANDP(void);          /* ANDP bit: rising-edge contact in series */
void ANDF(void);          /* ANDF bit: falling-edge contact in series */
void ORP(void);           /* ORP bit: rising-edge contact in parallel */
void ORF(void);           /* ORF bit: falling-edge contact in parallel */

/* ====================================================================== */
/* plc_exp_timers.c                                                       */
/* Timers and counters                                                    */
/* ====================================================================== */

void RST_TC(void);        /* RST Tn, RST Cn: reset a timer or a counter */
void FUNC_TIMER(void);    /* OUT Tn: timer coil with its preset */
void FUNC_COUNTER(void);  /* OUT Cn: counter coil with its preset */

/* ====================================================================== */
/* plc_exp_flow.c                                                         */
/* Program flow                                                           */
/* ====================================================================== */

void CJ(void);            /* CJ Pn: conditional jump */
void CALL(void);          /* CALL Pn: call a subroutine */
void SRET(void);          /* SRET: return from a subroutine */
void IRET(void);          /* IRET: return from an interrupt routine, rejected */
void EI(void);            /* EI: enable interrupts, rejected */
void DI(void);            /* DI: disable interrupts, rejected */
void WDT(void);           /* WDT: watchdog refresh, a no-operation here */
void FOR(void);           /* FOR n: start of a loop that runs n times */
void NEXT(void);          /* NEXT: end of the innermost loop */

/* ====================================================================== */
/* plc_exp_compare_move.c                                                 */
/* Comparison, data transfer and conversion                               */
/* ====================================================================== */

void CMP(void);           /* CMP S1 S2 D: compare, three result bits (16 bit) */
void DCMP(void);          /* DCMP S1 S2 D: compare, three result bits (32 bit) */
void ZCP(void);           /* ZCP S1 S2 S D: zone compare, three result bits (16 bit) */
void DZCP(void);          /* DZCP S1 S2 S D: zone compare, three result bits (32 bit) */
void MOV(void);           /* MOV S D: copy a value (16 bit) */
void DMOV(void);          /* DMOV S D: copy a value (32 bit) */
void SMOV(void);          /* SMOV S m1 m2 D n: move BCD digits */
void CML(void);           /* CML S D: copy the bitwise complement (16 bit) */
void DCML(void);          /* DCML S D: copy the bitwise complement (32 bit) */
void BMOV(void);          /* BMOV S D n: copy a block of 16-bit words */
void FMOV(void);          /* FMOV S D n: fill a block with one value (16 bit) */
void DFMOV(void);         /* DFMOV S D n: fill a block with one value (32 bit) */
void XCH(void);           /* XCH D1 D2: exchange two values (16 bit) */
void DXCH(void);          /* DXCH D1 D2: exchange two values (32 bit) */
void BCD(void);           /* BCD S D: binary to BCD (16 bit) */
void DBCD(void);          /* DBCD S D: binary to BCD (32 bit) */
void BIN(void);           /* BIN S D: BCD to binary (16 bit) */
void DBIN(void);          /* DBIN S D: BCD to binary (32 bit) */

/* ====================================================================== */
/* plc_exp_arithmetic.c                                                   */
/* Integer arithmetic and bitwise word operations                         */
/* ====================================================================== */

void ADD(void);           /* ADD S1 S2 D: D = S1 + S2 (16 bit) */
void DADD(void);          /* DADD S1 S2 D: D = S1 + S2 (32 bit) */
void SUB(void);           /* SUB S1 S2 D: D = S1 - S2 (16 bit) */
void DSUB(void);          /* DSUB S1 S2 D: D = S1 - S2 (32 bit) */
void MUL(void);           /* MUL S1 S2 D: D = S1 * S2, result twice as wide (16 bit) */
void DMUL(void);          /* DMUL S1 S2 D: D = S1 * S2, result twice as wide (32 bit) */
void DIV(void);           /* DIV S1 S2 D: D = S1 / S2, remainder behind the quotient (16 bit) */
void DDIV(void);          /* DDIV S1 S2 D: D = S1 / S2, remainder behind the quotient (32 bit) */
void INC(void);           /* INC D: D = D + 1 (16 bit) */
void DINC(void);          /* DINC D: D = D + 1 (32 bit) */
void DEC(void);           /* DEC D: D = D - 1 (16 bit) */
void DDEC(void);          /* DDEC D: D = D - 1 (32 bit) */
void WAND(void);          /* WAND S1 S2 D: D = S1 AND S2 (16 bit) */
void DWAND(void);         /* DWAND S1 S2 D: D = S1 AND S2 (32 bit) */
void WOR(void);           /* WOR S1 S2 D: D = S1 OR S2 (16 bit) */
void DWOR(void);          /* DWOR S1 S2 D: D = S1 OR S2 (32 bit) */
void WXOR(void);          /* WXOR S1 S2 D: D = S1 XOR S2 (16 bit) */
void DWXOR(void);         /* DWXOR S1 S2 D: D = S1 XOR S2 (32 bit) */
void NEG(void);           /* NEG D: D = -D (16 bit) */
void DNEG(void);          /* DNEG D: D = -D (32 bit) */

/* ====================================================================== */
/* plc_exp_rotate_shift.c                                                 */
/* Rotation, shift and FIFO                                               */
/* ====================================================================== */

void ROR(void);           /* ROR D n: rotate right (16 bit) */
void DROR(void);          /* DROR D n: rotate right (32 bit) */
void ROL(void);           /* ROL D n: rotate left (16 bit) */
void DROL(void);          /* DROL D n: rotate left (32 bit) */
void RCR(void);           /* RCR D n: rotate right through the carry (16 bit) */
void DRCR(void);          /* DRCR D n: rotate right through the carry (32 bit) */
void RCL(void);           /* RCL D n: rotate left through the carry (16 bit) */
void DRCL(void);          /* DRCL D n: rotate left through the carry (32 bit) */
void SFTR(void);          /* SFTR S D n1 n2: shift a bit string right */
void SFTL(void);          /* SFTL S D n1 n2: shift a bit string left */
void WSFR(void);          /* WSFR S D n1 n2: shift a word table right */
void WSFL(void);          /* WSFL S D n1 n2: shift a word table left */
void SFWR(void);          /* SFWR S D n: FIFO write */
void SFWL(void);          /* SFWL S D n: FIFO read (Mitsubishi SFRD) */

/* ====================================================================== */
/* plc_exp_data.c                                                         */
/* Data processing and tables                                             */
/* ====================================================================== */

void ZRST(void);          /* ZRST D1 D2: clear a range of devices */
void DECO(void);          /* DECO S D n: decode a number into one bit of a bit field */
void ENCO(void);          /* ENCO S D n: encode the highest ON bit as a number */
void SUM(void);           /* SUM S D: count the ON bits (16 bit) */
void DSUM(void);          /* DSUM S D: count the ON bits (32 bit) */
void BON(void);           /* BON S D n: test bit n (16 bit) */
void DBON(void);          /* DBON S D n: test bit n (32 bit) */
void MEAN(void);          /* MEAN S D n: average of n values (16 bit) */
void DMEAN(void);         /* DMEAN S D n: average of n values (32 bit) */
void ANS(void);           /* ANS S m D: annunciator set after a delay */
void ANR(void);           /* ANR: annunciator reset */
void SQR(void);           /* SQR S D: integer square root (16 bit) */
void DSQR(void);          /* DSQR S D: integer square root (32 bit) */
void SER(void);           /* SER S1 S2 D n: search a table (16 bit) */
void DSER(void);          /* DSER S1 S2 D n: search a table (32 bit) */
void ABSD(void);          /* ABSD S1 S2 D n: absolute drum sequencer (16 bit) */
void DABSD(void);         /* DABSD S1 S2 D n: absolute drum sequencer (32 bit) */
void ALT(void);           /* ALT D: toggle a bit on every execution */
void SORT(void);          /* SORT S m1 m2 D n: sort a table, ascending */
void SEGD(void);          /* SEGD S D: seven-segment decoder */
void CCD(void);           /* CCD S D n: sum and parity check of n bytes */
void ZPUSH(void);         /* ZPUSH D: save the index registers on a stack */
void ZPOP(void);          /* ZPOP D: restore the index registers from the stack */
void WSUM(void);          /* WSUM S D n: sum of n values, wider result (16 bit) */
void DWSUM(void);         /* DWSUM S D n: sum of n values, wider result (32 bit) */
void WTOB(void);          /* WTOB S D n: split words into bytes */
void BTOW(void);          /* BTOW S D n: pack bytes into words */
void UNI(void);           /* UNI S D n: unite the low nibbles of n words */
void DIS(void);           /* DIS S D n: disperse a word into n nibbles */
void SWAP(void);          /* SWAP D: exchange the bytes (16 bit) */
void DSWAP(void);         /* DSWAP D: exchange the bytes (32 bit) */
void SORT2(void);         /* SORT2 S m1 m2 D n: sort a table, direction set by M8165 (16 bit) */
void DSORT2(void);        /* DSORT2 S m1 m2 D n: sort a table, direction set by M8165 (32 bit) */
void GRY(void);           /* GRY S D: binary to Gray code (16 bit) */
void DGRY(void);          /* DGRY S D: binary to Gray code (32 bit) */
void GBIN(void);          /* GBIN S D: Gray code to binary (16 bit) */
void DGBIN(void);         /* DGBIN S D: Gray code to binary (32 bit) */

/* ====================================================================== */
/* plc_exp_float.c                                                        */
/* Floating point (the E-instructions work on 32-bit floats)              */
/* ====================================================================== */

void FLT(void);           /* FLT S D: integer to float (16 bit) */
void DFLT(void);          /* DFLT S D: integer to float (32 bit) */
void DECMP(void);         /* DECMP S1 S2 D: compare two floats, three result bits */
void DEZCP(void);         /* DEZCP S1 S2 S D: zone compare of a float */
void DEMOV(void);         /* DEMOV S D: copy a float */
void DEADD(void);         /* DEADD S1 S2 D: D = S1 + S2 */
void DESUB(void);         /* DESUB S1 S2 D: D = S1 - S2 */
void DEMUL(void);         /* DEMUL S1 S2 D: D = S1 * S2 */
void DEDIV(void);         /* DEDIV S1 S2 D: D = S1 / S2 */
void DEXP(void);          /* DEXP S D: e to the power S */
void DLOGE(void);         /* DLOGE S D: natural logarithm */
void DLOG10(void);        /* DLOG10 S D: logarithm to base 10 */
void DESQR(void);         /* DESQR S D: square root */
void DENEG(void);         /* DENEG D: change the sign */
void INT(void);           /* INT S D: float to integer (16 bit) */
void DINT(void);          /* DINT S D: float to integer (32 bit) */
void DSIN(void);          /* DSIN S D: sine (radians) */
void DCOS(void);          /* DCOS S D: cosine (radians) */
void DTAN(void);          /* DTAN S D: tangent (radians) */
void DASIN(void);         /* DASIN S D: arc sine (radians) */
void DACOS(void);         /* DACOS S D: arc cosine (radians) */
void DATAN(void);         /* DATAN S D: arc tangent (radians) */
void DRAD(void);          /* DRAD S D: degrees to radians */
void DDEG(void);          /* DDEG S D: radians to degrees */

/* ====================================================================== */
/* plc_exp_time.c                                                         */
/* Time and real-time clock                                               */
/* ====================================================================== */

void TCMP(void);          /* TCMP S1 S2 S3 S D: compare a time with a reference time */
void TZCP(void);          /* TZCP S1 S2 S D: zone compare of a time */
void TADD(void);          /* TADD S1 S2 D: D = S1 + S2 (time) */
void TSUB(void);          /* TSUB S1 S2 D: D = S1 - S2 (time) */
void HTOS(void);          /* HTOS S D: time to seconds */
void STOH(void);          /* STOH S D: seconds to time */
void TRD(void);           /* TRD D: read the real-time clock */
void TWR(void);           /* TWR S: set the real-time clock */
void HOUR(void);          /* HOUR S D1 D2: hour meter */

/* ====================================================================== */
/* plc_exp_compare_contacts.c                                             */
/* Comparison contacts (LD, AND and OR with = > < <> <= >=)               */
/* ====================================================================== */

void LD_EQU(void);        /* LD= S1 S2: load, true when S1 = S2 (16 bit) */
void DLD_EQU(void);       /* LDD= S1 S2: load, true when S1 = S2 (32 bit) */
void LD_GTR(void);        /* LD> S1 S2: load, true when S1 > S2 (16 bit) */
void DLD_GTR(void);       /* LDD> S1 S2: load, true when S1 > S2 (32 bit) */
void LD_LSS(void);        /* LD< S1 S2: load, true when S1 < S2 (16 bit) */
void DLD_LSS(void);       /* LDD< S1 S2: load, true when S1 < S2 (32 bit) */
void LD_NEQ(void);        /* LD<> S1 S2: load, true when S1 <> S2 (16 bit) */
void DLD_NEQ(void);       /* LDD<> S1 S2: load, true when S1 <> S2 (32 bit) */
void LD_LEQ(void);        /* LD<= S1 S2: load, true when S1 <= S2 (16 bit) */
void DLD_LEQ(void);       /* LDD<= S1 S2: load, true when S1 <= S2 (32 bit) */
void LD_GEQ(void);        /* LD>= S1 S2: load, true when S1 >= S2 (16 bit) */
void DLD_GEQ(void);       /* LDD>= S1 S2: load, true when S1 >= S2 (32 bit) */
void AND_EQU(void);       /* AND= S1 S2: series, true when S1 = S2 (16 bit) */
void DAND_EQU(void);      /* ANDD= S1 S2: series, true when S1 = S2 (32 bit) */
void AND_GTR(void);       /* AND> S1 S2: series, true when S1 > S2 (16 bit) */
void DAND_GTR(void);      /* ANDD> S1 S2: series, true when S1 > S2 (32 bit) */
void AND_LSS(void);       /* AND< S1 S2: series, true when S1 < S2 (16 bit) */
void DAND_LSS(void);      /* ANDD< S1 S2: series, true when S1 < S2 (32 bit) */
void AND_NEQ(void);       /* AND<> S1 S2: series, true when S1 <> S2 (16 bit) */
void DAND_NEQ(void);      /* ANDD<> S1 S2: series, true when S1 <> S2 (32 bit) */
void AND_LEQ(void);       /* AND<= S1 S2: series, true when S1 <= S2 (16 bit) */
void DAND_LEQ(void);      /* ANDD<= S1 S2: series, true when S1 <= S2 (32 bit) */
void AND_GEQ(void);       /* AND>= S1 S2: series, true when S1 >= S2 (16 bit) */
void DAND_GEQ(void);      /* ANDD>= S1 S2: series, true when S1 >= S2 (32 bit) */
void OR_EQU(void);        /* OR= S1 S2: parallel, true when S1 = S2 (16 bit) */
void DOR_EQU(void);       /* ORD= S1 S2: parallel, true when S1 = S2 (32 bit) */
void OR_GTR(void);        /* OR> S1 S2: parallel, true when S1 > S2 (16 bit) */
void DOR_GTR(void);       /* ORD> S1 S2: parallel, true when S1 > S2 (32 bit) */
void OR_LSS(void);        /* OR< S1 S2: parallel, true when S1 < S2 (16 bit) */
void DOR_LSS(void);       /* ORD< S1 S2: parallel, true when S1 < S2 (32 bit) */
void OR_NEQ(void);        /* OR<> S1 S2: parallel, true when S1 <> S2 (16 bit) */
void DOR_NEQ(void);       /* ORD<> S1 S2: parallel, true when S1 <> S2 (32 bit) */
void OR_LEQ(void);        /* OR<= S1 S2: parallel, true when S1 <= S2 (16 bit) */
void DOR_LEQ(void);       /* ORD<= S1 S2: parallel, true when S1 <= S2 (32 bit) */
void OR_GEQ(void);        /* OR>= S1 S2: parallel, true when S1 >= S2 (16 bit) */
void DOR_GEQ(void);       /* ORD>= S1 S2: parallel, true when S1 >= S2 (32 bit) */

/* ====================================================================== */
/* plc_exp_unsupported.c                                                  */
/* Rejected instructions (fault PLC_RUNTIME_LEGACY_UNSUPPORTED)           */
/* ====================================================================== */

void REF(void);           /* REF: input/output refresh, rejected */
void REFF(void);          /* REFF: refresh and filter adjust, rejected */
void MTR(void);           /* MTR: matrix input, rejected */
void DHSCS(void);         /* DHSCS: high-speed counter set, rejected */
void DHSCR(void);         /* DHSCR: high-speed counter reset, rejected */
void DHSZ(void);          /* DHSZ: high-speed counter zone compare, rejected */
void SPD(void);           /* SPD: speed detection, rejected */
void DSPD(void);          /* DSPD: speed detection, 32-bit form, rejected */
void PLSY(void);          /* PLSY: pulse output, rejected */
void PWM(void);           /* PWM: pulse width modulation, rejected */
void PLSR(void);          /* PLSR: pulse output with acceleration/deceleration, rejected */
void IST(void);           /* IST: initial state, rejected */
void INCD(void);          /* INCD: incremental drum sequencer, rejected */
void TTMR(void);          /* TTMR: teaching timer, rejected */
void STMR(void);          /* STMR: special timer, rejected */
void RAMP(void);          /* RAMP: ramp variable value, rejected */
void ROTS(void);          /* ROTS: rotary table control, rejected */
void TKY(void);           /* TKY: ten-key input, rejected */
void HKY(void);           /* HKY: hexadecimal key input, rejected */
void DSW(void);           /* DSW: digital switch input, rejected */
void SEGL(void);          /* SEGL: seven-segment display, time division, rejected */
void ARWS(void);          /* ARWS: arrow switch, rejected */
void ASC(void);           /* ASC: ASCII code input, rejected */
void PR(void);            /* PR: ASCII print, rejected */
void FROM(void);          /* FROM: read from a special function block, rejected */
void TO(void);            /* TO: write to a special function block, rejected */
void RS(void);            /* RS: serial communication, rejected */
void PRUN(void);          /* PRUN: parallel run (octal bit transfer), rejected */
void ASCI(void);          /* ASCI: hex to ASCII conversion, rejected */
void HEX(void);           /* HEX: ASCII to hex conversion, rejected */
void VRRD(void);          /* VRRD: volume read, rejected */
void VRSC(void);          /* VRSC: volume scale, rejected */
void RS2(void);           /* RS2: serial communication 2, rejected */
void PID(void);           /* PID: PID loop, rejected */
void DESTR(void);         /* DESTR: float to string, rejected */
void DEVAL(void);         /* DEVAL: string to float, rejected */
void EBCD(void);          /* EBCD: binary float to decimal float, rejected */
void EBIN(void);          /* EBIN: decimal float to binary float, rejected */
void RD3A(void);          /* RD3A: read from the analog block, rejected */
void WR3A(void);          /* WR3A: write to the analog block, rejected */

#endif /* PLC_EXP_HANDLERS_H */
