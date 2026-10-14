/**
 * @file plc_exp_internal.h
 * @brief Interfaces shared between the modules of the experimental FX1N-style interpreter.
 *
 * This header is private to the plc_exp_*.c modules (and their host tests); the firmware
 * talks to the interpreter only through plc_runtime.h. It declares
 *   - the interpreter registers (accumulator, program counter, call/FOR/MC stacks),
 *   - the operand model and the operand layer API (opd_*),
 *   - the bit-device helpers and the small logic helpers other modules reuse.
 *
 * Conventions used by all instruction handlers:
 *   1. decode every operand first (opd_decode), so a false rung consumes the same program
 *      words as a true one;
 *   2. continue only if PLC_EXP_ENABLED (no fault so far and the rung/pulse condition holds);
 *   3. read/write operands through opd_get()/opd_put() so every access is range checked.
 */
#ifndef PLC_EXP_INTERNAL_H
#define PLC_EXP_INTERNAL_H

/* Resolved through the include path (not the directory of this header) so that host tests can
 * substitute a mock platform.h for the STM32 device header. */
#include <platform.h>
#include "plc_runtime.h"
#include "plc_memory.h"
#include "plc_rtc.h"
#include "plc_memory_map.h"
#include "plc_special_devices.h"
#include "plc_exp_guard.h"

/* ---------------------------------------------------------------------------------------
 * Session state (defined in plc_exp_scan.c)
 * ------------------------------------------------------------------------------------- */

/** Pending software RUN/STOP request written by the programming link. */
enum { PLC_EXP_REQUEST_NONE, PLC_EXP_REQUEST_STOP, PLC_EXP_REQUEST_RUN };

extern volatile u8 plc_exp_pending_run_request;
extern volatile u32 plc_exp_milliseconds;        /**< free-running 1 ms tick */
extern u32 plc_exp_scan_seconds;                 /**< whole seconds elapsed since the previous scan */
extern u8 plc_exp_timer_fraction[256];           /**< sub-tick remainder per timer (always < 100 ms) */
extern plc_runtime_error plc_exp_last_error;     /**< first fault of the current run, 0 when none */

/* ---------------------------------------------------------------------------------------
 * Interpreter registers (defined in plc_exp_scan.c)
 * ------------------------------------------------------------------------------------- */

extern u8 PLC_ACC_BIT;                           /**< packed logic-block stack, bit 0 is the accumulator */
extern u8 PLC_MPS_BIT;                           /**< packed MPS/MRD/MPP stack */
extern const u16 *PLC_PC;                        /**< program counter (word pointer) */
extern const u16 *PLC_Err;                       /**< address of the instruction being executed */
extern u32 CMD_ADDR;                             /**< word offset of the current instruction */
extern u32 CMD_REG;                              /**< bit address operand of one-word instructions */
extern u32 CMD_PULS;                             /**< 1 for pulse ("P") execution */
extern u32 CMD_PULS_TEST;                        /**< execution condition of the current instruction */
extern u16 CMD_CMD;                              /**< first word of the current instruction */

extern u16 plc_exp_label_addr[128];              /**< word offset of label Pn, 0xFFFF when absent */
extern u16 plc_exp_call_pc[8];                   /**< CALL return offsets */
extern u8 plc_exp_call_acc[8];                   /**< accumulator saved by CALL */
extern u8 plc_exp_call_depth;
/** One FOR..NEXT level. */
typedef struct { u16 start, cycle, count; } plc_exp_for_frame_t;
extern plc_exp_for_frame_t plc_exp_for_stack[8];
extern u8 plc_exp_for_depth;
extern u8 plc_exp_mc_depth;                      /**< open MC levels */
extern u8 plc_exp_mc_off;                        /**< bit mask of MC levels whose coil is OFF */

extern u32 PLC_STL_Addr;                         /**< bit of the active step-ladder step */
extern u8 PLC_STL_Status;                        /**< 1 between STL and RET */
extern u8 PLC_STL_CMD;                           /**< reset together with PLC_STL_Status */

/* Dispatch tables (plc_exp_dispatch.c). */
extern void (* const FUNC_BASIC[16])(void);
extern void (* const FUNC_EXT_F[16])(void);
extern void (* const FUNC_EXT_0[512])(void);

/* ---------------------------------------------------------------------------------------
 * Operand model
 * ------------------------------------------------------------------------------------- */

/** Operand kinds produced by opd_decode(). */
enum { OPD_NONE, OPD_CONST, OPD_WORD, OPD_GROUP, OPD_BIT, OPD_PTR };
/** Word-device families addressed by an OPD_WORD operand. */
enum { DEV_D8000, DEV_T, DEV_C, DEV_D };

/** A decoded operand. @c index is a register number (OPD_WORD) or a linear bit (OPD_BIT/GROUP). */
typedef struct {
  u8 kind;      /**< OPD_* */
  u8 dev;       /**< DEV_* for OPD_WORD */
  u8 digits;    /**< K1..K8 digit count for OPD_GROUP */
  u32 value;    /**< constant value (OPD_CONST) or pointer number (OPD_PTR) */
  u32 index;    /**< register number or first bit, after V/Z indexing */
} operand_t;

/** True while the current instruction may act (no fault, rung ON or pulse edge). */
#define PLC_EXP_ENABLED (!plc_exp_last_error && CMD_PULS_TEST)

/* ---------------------------------------------------------------------------------------
 * Operand layer (plc_exp_operands.c)
 * ------------------------------------------------------------------------------------- */

/** Decode the next operand. @p width: 0 = 16-bit item, 1 = 32-bit (two items), 2 = 16-bit
 *  item that is accessed as 32-bit (MUL/WSUM results). Faults on malformed operands. */
void opd_decode(operand_t *o, u8 width);
u32 opd_get(const operand_t *o, u32 elem, u8 width);       /**< element @p elem, zero extended */
s32 opd_gets(const operand_t *o, u32 elem, u8 width);      /**< element @p elem, sign extended */
void opd_put(const operand_t *o, u32 elem, u32 value, u8 width);
u8 opd_bit_get(const operand_t *o, u32 elem);              /**< bit @p elem of a bit operand */
void opd_bit_put(const operand_t *o, u32 elem, u8 value);
u32 opd_bits(const operand_t *o, u32 count);               /**< count bits (bit device) or masked word */
u32 opd_count(const operand_t *o, u32 min, u32 max);       /**< validated element count */

u32 plc_exp_index_reg(u8 z, u8 r);                         /**< V (z=0) / Z (z=1) register r */
void plc_exp_index_reg_write(u8 z, u8 r, u16 value);

/* Bit devices. Reads are allowed everywhere in the image; writes only where programs may
 * drive a device (see plc_exp_bit_ok). */
u8 plc_exp_bit_area(u32 bit, u32 *limit, u8 *writable);
u8 plc_exp_bit_ok(u32 bit, u8 write);
u8 plc_exp_bit_read(u32 bit);
void plc_exp_bit_write(u32 bit, u8 value);
u8 plc_exp_is_step_bit(u32 bit);
void plc_exp_put_bit(u32 bit, u8 value);                   /**< unchecked image write (runtime-owned bits) */
void plc_exp_set_flag(u32 bit, u8 value);                  /**< set an M80xx flag relay */

u16 plc_exp_next_word(void);                               /**< fetch one program word */
u32 plc_exp_bit_operand(u8 write);                         /**< fetch a marked 0x8000|bit operand word */

/* ---------------------------------------------------------------------------------------
 * Helpers implemented by the logic, timer and data modules
 * ------------------------------------------------------------------------------------- */

u8 plc_exp_rung_enabled(void);                             /**< master control / STL step permits loads */
void plc_exp_acc_and(u8 value);                            /**< AND a value into the accumulator */
void plc_exp_acc_or(u8 value);                             /**< OR a value into the accumulator */
void plc_exp_timer_reset(u32 n);
void plc_exp_counter_reset(u32 n);
void plc_exp_block_copy(const operand_t *s, const operand_t *d, u32 count, u8 width);

u32 PLC_PL_BIT_TEST(void);
void PLC_PL_BIT_ON(void);
void PLC_PL_BIT_OFF(void);
u8 PLC_LDP_TEST(void);
void PLC_PROG_ERROR(u16 err, u16 err_id);
u32 DZ0_Z3(u16 n);                                         /**< (V:Z) pair of index registers 0..3 */
u32 DZ4_Z7(u16 n);                                         /**< (V:Z) pair of index registers 4..7 */


#endif /* PLC_EXP_INTERNAL_H */
