/**
 * @file plc_exp_unsupported.c
 * @brief Instructions that deliberately stop the PLC with the "unsupported" fault.
 *
 * Every handler here calls PLC_PROG_ERROR(), which latches PLC_RUNTIME_LEGACY_UNSUPPORTED
 * (D8067 = 8), clears the outputs and stops the PLC. A rejected instruction therefore
 * can never run "silently" as a no-op. The handlers keep the slots of the dispatch table
 * (plc_exp_dispatch.c) named, so the table still documents which instruction a slot
 * belongs to. IRET, EI and DI are rejected in plc_exp_flow.c.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Deliberately rejected instructions: each stops the PLC with the         */
/* "unsupported" fault instead of silently doing nothing.                   */
/*  - need hardware this board does not have: I/O refresh/scan (REF REFF    */
/*    MTR TKY HKY DSW ARWS SEGL), pulse and high-speed counter units (DHSCS */
/*    DHSCR DHSZ SPD DSPD PLSY PWM PLSR), special function blocks, serial   */
/*    and analog (FROM TO RS RS2 PR VRRD VRSC RD3A WR3A)                    */
/*  - no verified reference semantics in this repository (IST INCD TTMR     */
/*    STMR RAMP ROTS ASC PRUN ASCI HEX PID DESTR DEVAL EBCD EBIN)           */
/* ====================================================================== */
void REF(void)    { PLC_PROG_ERROR(M8065,02); }   /* input/output refresh */
void REFF(void)   { PLC_PROG_ERROR(M8065,02); }   /* refresh and filter adjust */
void MTR(void)    { PLC_PROG_ERROR(M8065,02); }   /* matrix input */
void DHSCS(void)  { PLC_PROG_ERROR(M8065,02); }   /* high-speed counter set */
void DHSCR(void)  { PLC_PROG_ERROR(M8065,02); }   /* high-speed counter reset */
void DHSZ(void)   { PLC_PROG_ERROR(M8065,02); }   /* high-speed counter zone compare */
void SPD(void)    { PLC_PROG_ERROR(M8065,02); }   /* speed detection */
void DSPD(void)   { PLC_PROG_ERROR(M8065,02); }   /* speed detection, 32-bit form */
void PLSY(void)   { PLC_PROG_ERROR(M8065,02); }   /* pulse output */
void PWM(void)    { PLC_PROG_ERROR(M8065,02); }   /* pulse width modulation */
void PLSR(void)   { PLC_PROG_ERROR(M8065,02); }   /* pulse output with acceleration/deceleration */
void IST(void)    { PLC_PROG_ERROR(M8065,02); }   /* initial state */
void INCD(void)   { PLC_PROG_ERROR(M8065,02); }   /* incremental drum sequencer */
void TTMR(void)   { PLC_PROG_ERROR(M8065,02); }   /* teaching timer */
void STMR(void)   { PLC_PROG_ERROR(M8065,02); }   /* special timer */
void RAMP(void)   { PLC_PROG_ERROR(M8065,02); }   /* ramp variable value */
void ROTS(void)   { PLC_PROG_ERROR(M8065,02); }   /* rotary table control */
void TKY(void)    { PLC_PROG_ERROR(M8065,02); }   /* ten-key input */
void HKY(void)    { PLC_PROG_ERROR(M8065,02); }   /* hexadecimal key input */
void DSW(void)    { PLC_PROG_ERROR(M8065,02); }   /* digital switch input */
void SEGL(void)   { PLC_PROG_ERROR(M8065,02); }   /* seven-segment display, time division */
void ARWS(void)   { PLC_PROG_ERROR(M8065,02); }   /* arrow switch */
void ASC(void)    { PLC_PROG_ERROR(M8065,02); }   /* ASCII code input */
void PR(void)     { PLC_PROG_ERROR(M8065,02); }   /* ASCII print */
void FROM(void)   { PLC_PROG_ERROR(M8065,02); }   /* read from a special function block */
void TO(void)     { PLC_PROG_ERROR(M8065,02); }   /* write to a special function block */
void RS(void)     { PLC_PROG_ERROR(M8065,02); }   /* serial communication */
void PRUN(void)   { PLC_PROG_ERROR(M8065,02); }   /* parallel run (octal bit transfer) */
void ASCI(void)   { PLC_PROG_ERROR(M8065,02); }   /* hex to ASCII conversion */
void HEX(void)    { PLC_PROG_ERROR(M8065,02); }   /* ASCII to hex conversion */
void VRRD(void)   { PLC_PROG_ERROR(M8065,02); }   /* volume read */
void VRSC(void)   { PLC_PROG_ERROR(M8065,02); }   /* volume scale */
void RS2(void)    { PLC_PROG_ERROR(M8065,02); }   /* serial communication 2 */
void PID(void)    { PLC_PROG_ERROR(M8065,02); }   /* PID loop */
void DESTR(void)  { PLC_PROG_ERROR(M8065,02); }   /* float to string */
void DEVAL(void)  { PLC_PROG_ERROR(M8065,02); }   /* string to float */
void EBCD(void)   { PLC_PROG_ERROR(M8065,02); }   /* binary float to decimal float */
void EBIN(void)   { PLC_PROG_ERROR(M8065,02); }   /* decimal float to binary float */
void RD3A(void)   { PLC_PROG_ERROR(M8065,02); }   /* read from the analog block */
void WR3A(void)   { PLC_PROG_ERROR(M8065,02); }   /* write to the analog block */

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_unsupported_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
