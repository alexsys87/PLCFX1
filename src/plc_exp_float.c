/**
 * @file plc_exp_float.c
 * @brief Single-precision floating point: FLT/INT, arithmetic, comparisons and math functions.
 *
 * A NaN/infinity result or an invalid domain is an operation error (fault), never a silent value.
 *
 * Floating-point operands are 32-bit IEEE 754 values stored in two consecutive registers
 * (low register first). The instruction names follow the FX convention: the binary
 * floating-point instructions are 32-bit only and carry the D prefix (DEADD, DSIN, ...).
 * Trigonometric functions work in radians.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"
#include <math.h>

/* ====================================================================== */
/* Floating point (single precision, 32-bit operands; a non-finite result   */
/* or invalid domain is an operation error)                                 */
/* ====================================================================== */
/** Read a 32-bit operand as an IEEE 754 single-precision value. */
static float float_get(const operand_t *o) {
  union { u32 u; float f; } c;
  c.u = opd_get(o, 0, 1);
  return c.f;
}
/**
 * Store a single-precision value in a 32-bit operand.
 * A NaN or infinite value is never stored: it stops the PLC with PLC_RUNTIME_OPERAND.
 */
static void float_put(const operand_t *o, float f) {
  union { u32 u; float f; } c;
  if ((f - f) != 0.0f) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }   /* NaN or infinity */
  c.f = f;
  opd_put(o, 0, c.u, 1);
}
/**
 * FLT S D: convert an integer to floating point.
 * @param w  0 for a 16-bit integer source, 1 for a 32-bit source.
 */
static void f_flt(u8 w) {
  operand_t s, d;
  opd_decode(&s, w); opd_decode(&d, 1);
  if (PLC_EXP_ENABLED) float_put(&d, (float)opd_gets(&s, 0, w));
}
/** FLT S D: 16-bit integer to float. */
void FLT(void)  { f_flt(0); }
/** DFLT S D: 32-bit integer to float. */
void DFLT(void) { f_flt(1); }

/**
 * INT S D: convert a floating-point value to an integer; the fraction is discarded
 * (truncation toward zero). M8020 is set when the result is zero. A value outside the range
 * of the destination stops the PLC with PLC_RUNTIME_OPERAND.
 * @param w  0 for a 16-bit result, 1 for a 32-bit result.
 */
static void f_int(u8 w) {
  operand_t s, d;
  float f;
  s32 r;
  opd_decode(&s, 1); opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  f = float_get(&s);
  if (!(w ? (f >= -2147483648.0f && f < 2147483648.0f) : (f > -32769.0f && f < 32768.0f))) {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
    return;
  }
  r = (s32)f;                                   /* fraction is discarded */
  opd_put(&d, 0, (u32)r, w);
  plc_exp_set_flag(M8020, r == 0);
}
/** INT S D: float to 16-bit integer. */
void INT(void)  { f_int(0); }
/** DINT S D: float to 32-bit integer. */
void DINT(void) { f_int(1); }

/** DEMOV S D: move a floating-point value; a float move is a plain 32-bit move. */
void DEMOV(void) { DMOV(); }

/**
 * DECMP S1 S2 D: compare two floating-point values. Three consecutive bits are written from
 * D: D = (S1 > S2), D+1 = (S1 == S2), D+2 = (S1 < S2).
 */
void DECMP(void) {
  operand_t a, b, d;
  float x, y;
  opd_decode(&a, 1); opd_decode(&b, 1); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  x = float_get(&a); y = float_get(&b);
  opd_bit_put(&d, 0, x > y); opd_bit_put(&d, 1, x == y); opd_bit_put(&d, 2, x < y);
}
/**
 * DEZCP S1 S2 S D: zone compare of the float S with the range S1..S2. D = (S < S1),
 * D+1 = (S1 <= S <= S2), D+2 = (S > S2). If S1 is above S2 the range is S1..S1.
 */
void DEZCP(void) {
  operand_t a, b, c, d;
  float lo, hi, x;
  opd_decode(&a, 1); opd_decode(&b, 1); opd_decode(&c, 1); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  lo = float_get(&a); hi = float_get(&b); x = float_get(&c);
  if (hi < lo) hi = lo;
  opd_bit_put(&d, 0, x < lo); opd_bit_put(&d, 1, x >= lo && x <= hi); opd_bit_put(&d, 2, x > hi);
}
/**
 * Two-source floating-point arithmetic S1 S2 D. A division by zero, and any result that is
 * NaN or infinite, stops the PLC with PLC_RUNTIME_OPERAND.
 * @param op  0 = addition, 1 = subtraction, 2 = multiplication, 3 = division.
 */
static void f_fop(u8 op) {                      /* 0 + 1 - 2 * 3 / */
  operand_t a, b, d;
  float x, y;
  opd_decode(&a, 1); opd_decode(&b, 1); opd_decode(&d, 1);
  if (!PLC_EXP_ENABLED) return;
  x = float_get(&a); y = float_get(&b);
  if (op == 3U && y == 0.0f) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  float_put(&d, op == 0U ? x + y : op == 1U ? x - y : op == 2U ? x * y : x / y);
}
/** DEADD S1 S2 D: D = S1 + S2 (float). */
void DEADD(void) { f_fop(0); }
/** DESUB S1 S2 D: D = S1 - S2 (float). */
void DESUB(void) { f_fop(1); }
/** DEMUL S1 S2 D: D = S1 * S2 (float). */
void DEMUL(void) { f_fop(2); }
/** DEDIV S1 S2 D: D = S1 / S2 (float); S2 = 0 is an operation error. */
void DEDIV(void) { f_fop(3); }
/** DENEG D: change the sign of a float in place. */
void DENEG(void) {
  operand_t d;
  opd_decode(&d, 1);
  if (PLC_EXP_ENABLED) float_put(&d, -float_get(&d));
}
/**
 * One-argument functions S D (float to float). Arguments outside the domain of the function
 * (negative square root, |x| > 1 for ASIN/ACOS, x <= 0 for the logarithms, a tangent at an
 * odd multiple of pi/2) and results that overflow stop the PLC with PLC_RUNTIME_OPERAND.
 * @param fn  0 sqrt, 1 sin, 2 cos, 3 tan, 4 asin, 5 acos, 6 atan, 7 rad, 8 deg, 9 exp,
 *            10 natural logarithm, 11 logarithm to base 10.
 */
static void f_fn(u8 fn) {
  operand_t s, d;
  float x, r = 0.0f, c;
  opd_decode(&s, 1); opd_decode(&d, 1);
  if (!PLC_EXP_ENABLED) return;
  x = float_get(&s);
  switch (fn) {
    case 0: if (x < 0.0f) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; } r = sqrtf(x); break;
    case 1: r = sinf(x); break;
    case 2: r = cosf(x); break;
    case 3: c = cosf(x); if (c == 0.0f) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; } r = sinf(x) / c; break;
    case 4: case 5:
      if (x < -1.0f || x > 1.0f) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
      c = sqrtf((1.0f - x) * (1.0f + x));
      r = fn == 4U ? atan2f(x, c) : atan2f(c, x);
      break;
    case 6: r = atan2f(x, 1.0f); break;
    case 7: r = x * 0.017453292519943295f; break;
    case 8: r = x * 57.29577951308232f; break;
    case 9: r = expf(x); break;
    default:
      if (x <= 0.0f) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
      r = fn == 10U ? logf(x) : logf(x) * 0.4342944819032518f;
      break;
  }
  float_put(&d, r);
}
/** DESQR S D: square root. */
void DESQR(void)  { f_fn(0); }
/** DSIN S D: sine (radians). */
void DSIN(void)   { f_fn(1); }
/** DCOS S D: cosine (radians). */
void DCOS(void)   { f_fn(2); }
/** DTAN S D: tangent (radians). */
void DTAN(void)   { f_fn(3); }
/** DASIN S D: arc sine, result in radians. */
void DASIN(void)  { f_fn(4); }
/** DACOS S D: arc cosine, result in radians. */
void DACOS(void)  { f_fn(5); }
/** DATAN S D: arc tangent, result in radians. */
void DATAN(void)  { f_fn(6); }
/** DRAD S D: degrees to radians. */
void DRAD(void)   { f_fn(7); }
/** DDEG S D: radians to degrees. */
void DDEG(void)   { f_fn(8); }
/** DEXP S D: e raised to the power S. */
void DEXP(void)   { f_fn(9); }
/** DLOGE S D: natural logarithm. */
void DLOGE(void)  { f_fn(10); }
/** DLOG10 S D: logarithm to base 10. */
void DLOG10(void) { f_fn(11); }

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_float_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
