/**
 * @file    plc_runtime.h
 * @brief   Interface between the PLC scan engine and the rest of the firmware.
 *
 * Two interchangeable implementations provide this interface. The build selects
 * one with PLC_RUNTIME_EXPERIMENTAL (see plc_runtime_profile.h):
 *   - src/plc_runtime_minimal.c: the checked 21-mnemonic subset (default);
 *   - src/plc_exp_*.c:           the broad FX1N-style instruction layer (experimental).
 *
 * Calling context: PLC_Main_Parser() runs only in the main loop.
 * PLC_Runtime_Tick1ms() belongs to the 1 ms timer interrupt, while
 * PLC_Runtime_RequestRun() and the PLC_BIT_* helpers may also be called from the
 * serial-port interrupt.
 */
#ifndef PLC_RUNTIME_H
#define PLC_RUNTIME_H
#include "platform.h"

/**
 * Fault codes of the scan engine. The first fault is latched: its code goes to
 * D8067, the program step to D8068/D8069, M8065 is set, the outputs are cleared and
 * the PLC stops until the fault is acknowledged.
 */
typedef enum {
  PLC_RUNTIME_OK = 0,                 /**< No fault. */
  PLC_RUNTIME_OPCODE = 1,             /**< Instruction word is not part of the supported set. */
  PLC_RUNTIME_OPERAND = 2,            /**< Invalid operand: wrong type, out of range or read-only target. */
  PLC_RUNTIME_BOUNDS = 3,             /**< Access outside the PLC image or the program area. */
  PLC_RUNTIME_STACK = 4,              /**< Logic-block, branch or nesting (CALL/FOR/MC/STL) stack violation. */
  PLC_RUNTIME_NO_END = 5,             /**< The program has no END instruction. */
  PLC_RUNTIME_LEGACY_MISSING = 6,     /**< Experimental: dispatch slot without a handler (no longer raised). */
  PLC_RUNTIME_LEGACY_INCOMPLETE = 7,  /**< Experimental: handler known to be incomplete (no longer raised). */
  PLC_RUNTIME_LEGACY_UNSUPPORTED = 8, /**< Experimental: instruction deliberately not supported, or hardware missing. */
  PLC_RUNTIME_LEGACY_BUDGET = 9       /**< Experimental: per-scan instruction budget exhausted. */
} plc_runtime_error;

/**
 * Reset the runtime-private state: tick counter, timer remainders, edge memory
 * and the latched fault (D8067..D8069 and M8065 are cleared). The rest of the PLC
 * register image and a pending RUN/STOP request are kept.
 */
void PLC_Runtime_Init(void);

/** Advance the 1 ms time base; call from the periodic timer interrupt. */
void PLC_Runtime_Tick1ms(void);

/**
 * Post a RUN (non-zero) or STOP (zero) request. This is an ISR-safe mailbox: it
 * writes no part of the PLC image and the newest pending request wins. The scan
 * applies it when it next starts.
 */
void PLC_Runtime_RequestRun(u8 run);

/** Non-zero when the PLC runs or a RUN request is pending (a pending request overrides M8000/M8001). */
u8 PLC_Runtime_RunRequested(void);

/** Acknowledge the latched fault: clear its code, D8067..D8069 and M8065. */
void PLC_Runtime_ClearFault(void);

/** First fault since the last acknowledgement; PLC_RUNTIME_OK when there is none. */
plc_runtime_error PLC_Runtime_LastError(void);

/** Program step at which that fault was detected. */
u16 PLC_Runtime_ErrorStep(void);

/**
 * Run one PLC scan: apply pending RUN/STOP requests, update the clock relays and
 * timers, then validate and execute the program. Call it repeatedly from the main loop.
 */
void PLC_Main_Parser(void);

/** Historical name of PLC_Main_Parser(), still used by the main loop. */
void PLC_ProInstructParse(void); /* Legacy entry point. */

/**
 * Test, set or clear one bit of the PLC image by linear bit address
 * (byte offset * 8 + bit index). Addresses outside the image read as 0 and are
 * ignored on write; the experimental runtime also latches PLC_RUNTIME_BOUNDS.
 */
u32 PLC_BIT_TEST(u32 linear_bit);
void PLC_BIT_ON(u32 linear_bit);
void PLC_BIT_OFF(u32 linear_bit);

/** Set by the programming link when a new program was stored; the next scan starts from a clean state. */
extern volatile u8 PLC_NEW_PROG;
/** Non-zero while a program download is in progress; the PLC stays in STOP meanwhile. */
extern volatile u8 PLC_UPDATE_PROG;
/** 1 outside a running scan; the scan clears it while it executes the program. */
extern u8 PLC_PRG_STOP;
/** 1 during the first scan after RUN (initial pulse, M8002). */
extern u8 PLC_PULS;
#endif
