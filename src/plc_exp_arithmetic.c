/**
 * @file plc_exp_arithmetic.c
 * @brief ADD/SUB/MUL/DIV, INC/DEC/NEG and WAND/WOR/WXOR (16 and 32 bit) with the M8020..M8022 flags.
 *
 * Every handler follows the common instruction pattern: decode all operands first (so a
 * false rung consumes the same program words as a true one), return unless the instruction
 * is enabled, then read the sources and write the destination through the operand layer.
 * The D-prefixed forms (DADD, DSUB, ...) are the 32-bit versions of the same operation.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Arithmetic and logic. 16-bit forms wrap like the PLC; ADD/SUB set the   */
/* zero (M8020), borrow (M8021) and carry (M8022) flags from the exact      */
/* result, MUL/DIV widen their result, division by zero is an operation     */
/* error (STOP).                                                            */
/* ====================================================================== */

/** Operation selector of f_arith(). */
enum { A_ADD, A_SUB, A_MUL, A_DIV, A_AND, A_OR, A_XOR };

/**
 * Common body of the instructions with two sources and one destination (S1 S2 D).
 *
 *   ADD, SUB  D = S1 + S2 or S1 - S2 with wrap-around. M8020 is set for a zero result,
 *             M8021 when the exact result is below the signed minimum (borrow) and M8022
 *             when it is above the signed maximum (carry).
 *   MUL       D = S1 * S2 as a signed 32-bit result for the 16-bit form (the destination
 *             occupies two registers) or a 64-bit result in four registers for DMUL.
 *   DIV       D = quotient, D+1 = remainder (D+2 and D+3 for DDIV). A zero divisor stops the
 *             PLC with PLC_RUNTIME_OPERAND and writes nothing.
 *   AND/OR/XOR  bitwise operation on the unsigned values (WAND, WOR, WXOR); no flags.
 *
 * @param w   0 for the 16-bit form, 1 for the 32-bit form.
 * @param op  A_ADD .. A_XOR.
 */
static void f_arith(u8 w, u8 op) {
  operand_t a, b, d;
  int64_t x, y, r;
  u32 ux, uy;
  opd_decode(&a, w); opd_decode(&b, w);
  opd_decode(&d, (op == A_MUL && !w) ? 2U : w);
  if (!PLC_EXP_ENABLED) return;
  switch (op) {
    case A_ADD: case A_SUB:
      x = opd_gets(&a, 0, w); y = opd_gets(&b, 0, w);
      r = op == A_ADD ? x + y : x - y;
      plc_exp_set_flag(M8020, (((u32)r) & (w ? 0xFFFFFFFFU : 0xFFFFU)) == 0U);
      plc_exp_set_flag(M8021, r < (w ? (int64_t)INT32_MIN : -32768));
      plc_exp_set_flag(M8022, r > (w ? (int64_t)INT32_MAX : 32767));
      opd_put(&d, 0, (u32)r, w);
      break;
    case A_MUL:
      r = (int64_t)opd_gets(&a, 0, w) * (int64_t)opd_gets(&b, 0, w);
      opd_put(&d, 0, (u32)r, 1);
      if (w) opd_put(&d, 1, (u32)((uint64_t)r >> 32), 1);
      break;
    case A_DIV:
      x = opd_gets(&a, 0, w); y = opd_gets(&b, 0, w);
      if (y == 0) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
      opd_put(&d, 0, (u32)(x / y), w);
      opd_put(&d, 1, (u32)(x % y), w);
      break;
    default:
      ux = opd_get(&a, 0, w); uy = opd_get(&b, 0, w);
      opd_put(&d, 0, op == A_AND ? (ux & uy) : op == A_OR ? (ux | uy) : (ux ^ uy), w);
      break;
  }
}

/** ADD S1 S2 D: D = S1 + S2 (16 bit), sets M8020..M8022. */
void ADD(void)    { f_arith(0, A_ADD); }
/** DADD S1 S2 D: D = S1 + S2 (32 bit), sets M8020..M8022. */
void DADD(void)   { f_arith(1, A_ADD); }
/** SUB S1 S2 D: D = S1 - S2 (16 bit), sets M8020..M8022. */
void SUB(void)    { f_arith(0, A_SUB); }
/** DSUB S1 S2 D: D = S1 - S2 (32 bit), sets M8020..M8022. */
void DSUB(void)   { f_arith(1, A_SUB); }
/** MUL S1 S2 D: D, D+1 = S1 * S2 (16-bit operands, 32-bit result). */
void MUL(void)    { f_arith(0, A_MUL); }
/** DMUL S1 S2 D: D..D+3 = S1 * S2 (32-bit operands, 64-bit result). */
void DMUL(void)   { f_arith(1, A_MUL); }
/** DIV S1 S2 D: D = S1 / S2, D+1 = remainder (16 bit). */
void DIV(void)    { f_arith(0, A_DIV); }
/** DDIV S1 S2 D: D, D+1 = S1 / S2, D+2, D+3 = remainder (32 bit). */
void DDIV(void)   { f_arith(1, A_DIV); }
/** WAND S1 S2 D: D = S1 & S2 (16 bit). */
void WAND(void)   { f_arith(0, A_AND); }
/** DWAND S1 S2 D: D = S1 & S2 (32 bit). */
void DWAND(void)  { f_arith(1, A_AND); }
/** WOR S1 S2 D: D = S1 | S2 (16 bit). */
void WOR(void)    { f_arith(0, A_OR); }
/** DWOR S1 S2 D: D = S1 | S2 (32 bit). */
void DWOR(void)   { f_arith(1, A_OR); }
/** WXOR S1 S2 D: D = S1 ^ S2 (16 bit). */
void WXOR(void)   { f_arith(0, A_XOR); }
/** DWXOR S1 S2 D: D = S1 ^ S2 (32 bit). */
void DWXOR(void)  { f_arith(1, A_XOR); }

/**
 * Common body of the single-operand instructions INC, DEC and NEG.
 * The value wraps around in the width of the operand and no flags are changed. With the
 * pulse form (INCP, ...) the instruction runs only on the rising edge of the rung.
 *
 * @param w     0 for the 16-bit form, 1 for the 32-bit form.
 * @param kind  0 = INC (D + 1), 1 = DEC (D - 1), 2 = NEG (two's complement, 0 - D).
 */
static void f_unary(u8 w, u8 kind) {              /* 0 INC, 1 DEC, 2 NEG */
  operand_t d;
  u32 v;
  opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  v = opd_get(&d, 0, w);
  opd_put(&d, 0, kind == 0U ? v + 1U : kind == 1U ? v - 1U : 0U - v, w);
}

/** INC D: D = D + 1 (16 bit, wraps from 65535 to 0). */
void INC(void)   { f_unary(0, 0); }
/** DINC D: D = D + 1 (32 bit). */
void DINC(void)  { f_unary(1, 0); }
/** DEC D: D = D - 1 (16 bit, wraps from 0 to 65535). */
void DEC(void)   { f_unary(0, 1); }
/** DDEC D: D = D - 1 (32 bit). */
void DDEC(void)  { f_unary(1, 1); }
/** NEG D: D = -D, the two's complement (16 bit). */
void NEG(void)   { f_unary(0, 2); }
/** DNEG D: D = -D, the two's complement (32 bit). */
void DNEG(void)  { f_unary(1, 2); }

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_arithmetic_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
