/**
 * @file plc_exp_dispatch.c
 * @brief Opcode dispatch tables of the experimental FX1N-style interpreter.
 *
 * The tables and the opcode numbering are the original ones (provenance: PR2 commit
 * 4023823a1b9c0edc083288664dda0b04f28a961c, file src/ladder.c, since renamed) and must not be reordered: the
 * 16-bit program words are indexed directly.
 *
 *  - FUNC_BASIC[word >> 12]   one-word contacts, coils and the router entries
 *  - FUNC_EXT_F[(word >> 8) & 0xF]  step-ladder (STL/RET) and the FF logic-block group
 *  - FUNC_EXT_0[word & 0x1FF] application instructions; opcode = 0x10 + 2 * FNC number,
 *    +1 for the 32-bit ("D") form, bit 12 of the word selects pulse ("P") execution
 *
 * ERR slots are unassigned opcodes and stop the PLC with the "unsupported" fault.
 *
 * Opcode numbering. For the classic FX instructions the slot is 0x10 + 2 * n (+1 for the
 * 32-bit form) with n = the Mitsubishi FNC number, for example CMP (FNC10) at 0x24/0x25 and
 * ADD (FNC20) at 0x38/0x39. A few later additions sit in slots of their own: ZPUSH, ZPOP,
 * DESTR, DEVAL, EBCD, EBIN, the data block WSUM..SORT2, HOUR, RD3A and WR3A. This table, not
 * a list of FNC numbers, is the reference for the program encoding; it is checked slot by
 * slot against docs/legacy-instruction-inventory.json by tools/instruction_inventory.py.
 *
 * A slot may name an instruction that the firmware deliberately rejects (REF, MTR, PLSY,
 * FROM, ...). Its handler lives in plc_exp_unsupported.c (IRET/EI/DI in plc_exp_flow.c) and
 * stops the PLC with the "unsupported" fault, so it behaves exactly like an ERR slot at run
 * time; the name only documents which instruction the slot belongs to.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/*
 * First level: the high nibble of the program word.
 *
 *   0x0, 0x1   FUNC_0  extended group: application instructions, timer/counter coils,
 *                      extended contacts and coils (bit 12 = pulse execution)
 *   0x2..0x7   LD LDI AND ANI OR ORI   one-word contacts with a 12-bit bit address
 *   0x8..0xA   unassigned
 *   0xB        NOP     label words 0xB0nn (labels are collected by the scan, see plc_exp_scan.c)
 *   0xC..0xE   OUT SET RST             one-word coils with a 12-bit bit address
 *   0xF        FUNC_F  step ladder and logic-block group
 */
void (* const FUNC_BASIC[])(void) = {
  FUNC_0,   FUNC_0,   LD,       LDI,      AND,      ANI,      OR,       ORI,
  ERR,      ERR,      ERR,      NOP,      OUT,      BSET,     RST,      FUNC_F
};

/*
 * Second level for 0xFnnn words: the second nibble selects STL (0xF0nn..0xF3nn, step number
 * S0..S1023), RET (0xF7nn) or the logic-block group FFnn (ANB, ORB, MPS, MRD, MPP, INV).
 */
void (* const FUNC_EXT_F[])(void) = {
  STL,      STL,      STL,      STL,      ERR,      ERR,      ERR,      RET,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      FUNC_FF
};

/*
 * Extended group: the 9-bit opcode (word & 0x1FF). Rows are aligned to eight slots; the
 * comments give the first opcode of each group and what it contains.
 */
void (* const FUNC_EXT_0[])(void) = {
/* 0x0000  NOP; extended OUT, SET, RST (0x02..0x04, alias set 0x05..0x07); PLS, PLF, MC, MCR;
 *         RST Tn/Cn (0x0C) and RST Dn (0x0D); END (0x0F) */
  NOP,      ERR,      OUT,      BSET,     RST,      OUT,      BSET,     RST,
  PLS,      PLF,      MC,       MCR,      RST_TC,   RST_D,    ERR,      END,
/* 0x0010  program flow: CJ CALL SRET IRET EI DI FEND(END) WDT FOR NEXT */
  CJ,       ERR,      CALL,     ERR,      SRET,     ERR,      IRET,     ERR,
  EI,       ERR,      DI,       ERR,      END,      ERR,      WDT,      ERR,
  FOR,      ERR,      NEXT,     ERR,
/* 0x0024  compare and move: CMP ZCP MOV SMOV CML BMOV FMOV XCH BCD BIN */
                                          CMP,      DCMP,     ZCP,      DZCP,
  MOV,      DMOV,     SMOV,     ERR,      CML,      DCML,     BMOV,     ERR,
  FMOV,     DFMOV,    XCH,      DXCH,     BCD,      DBCD,     BIN,      DBIN,
/* 0x0038  arithmetic and logic: ADD SUB MUL DIV INC DEC WAND WOR WXOR NEG */
  ADD,      DADD,     SUB,      DSUB,     MUL,      DMUL,     DIV,      DDIV,
  INC,      DINC,     DEC,      DDEC,     WAND,     DWAND,    WOR,      DWOR,
  WXOR,     DWXOR,    NEG,      DNEG,
/* 0x004C  rotation and shift: ROR ROL RCR RCL, SFTR SFTL WSFR WSFL SFWR SFWL */
                                          ROR,      DROR,     ROL,      DROL,
  RCR,      DRCR,     RCL,      DRCL,     SFTR,     ERR,      SFTL,     ERR,
  WSFR,     ERR,      WSFL,     ERR,      SFWR,     ERR,      SFWL,     ERR,
/* 0x0060  data processing: ZRST DECO ENCO SUM BON MEAN ANS ANR SQR FLT */
  ZRST,     ERR,      DECO,     ERR,      ENCO,     ERR,      SUM,      DSUM,
  BON,      DBON,     MEAN,     DMEAN,    ANS,      ERR,      ANR,      ERR,
  SQR,      DSQR,     FLT,      DFLT,
/* 0x0074  I/O refresh and high-speed units, all rejected (no such hardware):
 *         REF REFF MTR DHSCS DHSCR DHSZ SPD PLSY PWM PLSR */
                                          REF,      ERR,      REFF,     ERR,
  MTR,      ERR,      DHSCS,    ERR,      DHSCR,    ERR,      DHSZ,     ERR,
  SPD,      DSPD,     PLSY,     ERR,      PWM,      ERR,      PLSR,     ERR,
/* 0x0088  handy instructions: SER ABSD ALT SORT are implemented; IST INCD TTMR STMR RAMP
 *         ROTS are rejected */
  IST,      ERR,      SER,      DSER,     ABSD,     DABSD,    INCD,     ERR,
  TTMR,     ERR,      STMR,     ERR,      ALT,      ERR,      RAMP,     ERR,
  ROTS,     ERR,      SORT,     ERR,
/* 0x009C  external devices: SEGD is implemented; TKY HKY DSW SEGL ARWS ASC PR FROM TO are
 *         rejected */
                                          TKY,      ERR,      HKY,      ERR,
  DSW,      ERR,      SEGD,     ERR,      SEGL,     ERR,      ARWS,     ERR,
  ASC,      ERR,      PR,       ERR,      FROM,     ERR,      TO,       ERR,
/* 0x00B0  serial and special blocks: CCD, ZPUSH and ZPOP are implemented; RS PRUN ASCI HEX
 *         VRRD VRSC RS2 PID are rejected */
  RS,       ERR,      PRUN,     ERR,      ASCI,     ERR,      HEX,      ERR,
  CCD,      ERR,      VRRD,     ERR,      VRSC,     ERR,      RS2,      ERR,
  PID,      ERR,      ZPUSH,    ERR,
/* 0x00C4  ZPOP, then unassigned slots up to 0x00EB */
                                          ZPOP,     ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
/* 0x00D8  unassigned */
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
/* 0x00EC  binary floating point (32-bit forms only): DECMP DEZCP DEMOV are implemented;
 *         DESTR DEVAL EBCD EBIN are rejected */
                                          ERR,      DECMP,    ERR,      DEZCP,
  ERR,      DEMOV,    ERR,      DESTR,    ERR,      DEVAL,    ERR,      EBCD,
  ERR,      EBIN,     ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
/* 0x0100  floating-point arithmetic: DEADD DESUB DEMUL DEDIV DEXP DLOGE DLOG10 DESQR DENEG;
 *         INT/DINT convert float to integer */
  ERR,      DEADD,    ERR,      DESUB,    ERR,      DEMUL,    ERR,      DEDIV,
  ERR,      DEXP,     ERR,      DLOGE,    ERR,      DLOG10,   ERR,      DESQR,
  ERR,      DENEG,    INT,      DINT,
/* 0x0114  trigonometry: DSIN DCOS DTAN DASIN DACOS DATAN DRAD DDEG */
                                          ERR,      DSIN,     ERR,      DCOS,
  ERR,      DTAN,     ERR,      DASIN,    ERR,      DACOS,    ERR,      DATAN,
  ERR,      DRAD,     ERR,      DDEG,     ERR,      ERR,      ERR,      ERR,
/* 0x0128  word/byte data: WSUM WTOB BTOW UNI DIS SWAP SORT2 (slots 0x0128..0x012B unassigned) */
  ERR,      ERR,      ERR,      ERR,      WSUM,     DWSUM,    WTOB,     BTOW,
  UNI,      DIS,      SWAP,     DSWAP,    SORT2,    DSORT2,   ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
/* 0x013C  unassigned */
                                          ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
/* 0x0150  clock: TCMP TZCP TADD TSUB HTOS STOH TRD TWR HOUR */
  TCMP,     ERR,      TZCP,     ERR,      TADD,     ERR,      TSUB,     ERR,
  HTOS,     ERR,      STOH,     ERR,      TRD,      ERR,      TWR,      ERR,
  HOUR,     ERR,      ERR,      ERR,
/* 0x0164  Gray code: GRY GBIN; analog modules RD3A and WR3A are rejected */
                                          GRY,      DGRY,     GBIN,     DGBIN,
  RD3A,     ERR,      WR3A,     ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
/* 0x0178  unassigned */
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
/* 0x018C  unassigned */
                                          ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
/* 0x01A0  unassigned */
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
/* 0x01B4  unassigned up to 0x01BF */
                                          ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
/* 0x01C0  extended contacts: LD LDI AND ANI OR ORI (0x01C2..0x01C7) and the edge contacts
 *         LDP LDF ANDP ANDF ORP ORF (0x01CA..0x01CF); each is followed by a marked bit word */
  ERR,      ERR,      LD,       LDI,      AND,      ANI,      OR,       ORI,
  ERR,      ERR,      LDP,      LDF,      ANDP,     ANDF,     ORP,      ORF,
/* 0x01D0  comparison contacts in LD form, 16-bit and 32-bit: = > < (0x01D0..0x01D5), <> <= >= (0x01D8..0x01DD) */
  LD_EQU,   DLD_EQU,  LD_GTR,   DLD_GTR,  LD_LSS,   DLD_LSS,  ERR,      ERR,
  LD_NEQ,   DLD_NEQ,  LD_LEQ,   DLD_LEQ,  LD_GEQ,   DLD_GEQ,  ERR,      ERR,
/* 0x01E0  comparison contacts in AND form */
  AND_EQU,  DAND_EQU, AND_GTR,  DAND_GTR, AND_LSS,  DAND_LSS, ERR,      ERR,
  AND_NEQ,  DAND_NEQ, AND_LEQ,  DAND_LEQ, AND_GEQ,  DAND_GEQ, ERR,      ERR,
/* 0x01F0  comparison contacts in OR form */
  OR_EQU,   DOR_EQU,  OR_GTR,   DOR_GTR,  OR_LSS,   DOR_LSS,  ERR,      ERR,
  OR_NEQ,   DOR_NEQ,  OR_LEQ,   DOR_LEQ,  OR_GEQ,   DOR_GEQ,  ERR,      ERR
};

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_dispatch_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
