/**
 * @file plc_exp_compare_move.c
 * @brief CMP/ZCP, MOV/CML/SMOV/BMOV/FMOV/XCH and BCD/BIN conversions (16 and 32 bit).
 *
 * The D-prefixed instructions (DCMP, DMOV, ...) are the 32-bit forms. Operands are read
 * and written through the operand layer, so every source and destination may be a
 * constant (sources only), a D/T/C register, a KnM bit group or an indexed (V/Z) form.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Compare, move, BCD/BIN                                                  */
/* ====================================================================== */

/**
 * Convert a binary number to packed BCD.
 * @param v       value to convert; only the lowest @p digits decimal digits are kept.
 * @param digits  number of BCD digits (4 or 8).
 */
static u32 bin2bcd(u32 v, u8 digits) {
  u32 r = 0, k;
  for (k = 0; k < digits; ++k) { r |= (v % 10U) << (4U * k); v /= 10U; }
  return r;
}

/**
 * Convert packed BCD to a binary number.
 * @param v       BCD value.
 * @param digits  number of BCD digits (4 or 8).
 * @param out     receives the binary value on success.
 * @return 0 if a digit is above 9 or bits are set above the BCD digits, otherwise 1.
 */
static u8 bcd2bin(u32 v, u8 digits, u32 *out) {
  u32 r = 0, m = 1, k, d;
  for (k = 0; k < digits; ++k) {
    d = (v >> (4U * k)) & 15U;
    if (d > 9U) return 0;
    r += d * m; m *= 10U;
  }
  if (digits < 8U && (v >> (4U * digits))) return 0;   /* stray bits above the BCD digits */
  *out = r;
  return 1;
}

/**
 * CMP S1 S2 D: signed compare of two values. Three consecutive bits are written starting
 * at the destination: D = (S1 > S2), D+1 = (S1 == S2), D+2 = (S1 < S2).
 * @param w  0 for 16-bit operands, 1 for 32-bit operands.
 */
static void f_cmp(u8 w) {
  operand_t a, b, d;
  s32 x, y;
  opd_decode(&a, w); opd_decode(&b, w); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  x = opd_gets(&a, 0, w); y = opd_gets(&b, 0, w);
  opd_bit_put(&d, 0, x > y); opd_bit_put(&d, 1, x == y); opd_bit_put(&d, 2, x < y);
}

/** CMP S1 S2 D: 16-bit compare, see f_cmp(). */
void CMP(void)  { f_cmp(0); }
/** DCMP S1 S2 D: 32-bit compare, see f_cmp(). */
void DCMP(void) { f_cmp(1); }

/**
 * ZCP S1 S2 S D: zone compare of S with the range S1..S2. Three consecutive bits are
 * written starting at the destination: D = (S < S1), D+1 = (S1 <= S <= S2), D+2 = (S > S2).
 * If S1 is above S2 the range is treated as S1..S1.
 * @param w  0 for 16-bit operands, 1 for 32-bit operands.
 */
static void f_zcp(u8 w) {
  operand_t a, b, c, d;
  s32 lo, hi, x;
  opd_decode(&a, w); opd_decode(&b, w); opd_decode(&c, w); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  lo = opd_gets(&a, 0, w); hi = opd_gets(&b, 0, w); x = opd_gets(&c, 0, w);
  if (hi < lo) hi = lo;                          /* S1 > S2 is handled as S2 = S1 */
  opd_bit_put(&d, 0, x < lo); opd_bit_put(&d, 1, x >= lo && x <= hi); opd_bit_put(&d, 2, x > hi);
}

/** ZCP S1 S2 S D: 16-bit zone compare, see f_zcp(). */
void ZCP(void)  { f_zcp(0); }
/** DZCP S1 S2 S D: 32-bit zone compare, see f_zcp(). */
void DZCP(void) { f_zcp(1); }

/**
 * MOV S D / CML S D: copy a value, optionally inverted.
 * @param w    0 for 16 bit, 1 for 32 bit.
 * @param inv  non-zero to invert every bit (CML).
 */
static void f_mov(u8 w, u8 inv) {
  operand_t s, d;
  opd_decode(&s, w); opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  opd_put(&d, 0, inv ? ~opd_get(&s, 0, w) : opd_get(&s, 0, w), w);
}

/** MOV S D: D = S (16 bit). */
void MOV(void)   { f_mov(0, 0); }
/** DMOV S D: D = S (32 bit). */
void DMOV(void)  { f_mov(1, 0); }
/** CML S D: D = ~S, bitwise complement (16 bit). */
void CML(void)   { f_mov(0, 1); }
/** DCML S D: D = ~S, bitwise complement (32 bit). */
void DCML(void)  { f_mov(1, 1); }

/**
 * SMOV S m1 m2 D n: move m2 BCD digits starting at digit m1 of S to digit n of D.
 *
 * Digits are numbered 1 (lowest) to 4 (highest) and are taken from digit m1 downwards;
 * they are stored from digit n downwards. Required: 1 <= m1 <= 4, 1 <= m2 <= m1 and
 * m2 <= n <= 4, otherwise the PLC stops with PLC_RUNTIME_OPERAND. The other digits of D
 * are unchanged. With M8168 OFF the operands are binary numbers (0..9999) that are
 * converted to BCD for the move and back afterwards; with M8168 ON they already hold BCD.
 */
void SMOV(void) {
  operand_t s, m1, m2, d, n;
  s32 a, b, c, k;
  u32 sv, dv, v;
  u8 bcd_mode;
  opd_decode(&s, 0); opd_decode(&m1, 0); opd_decode(&m2, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (!PLC_EXP_ENABLED) return;
  a = opd_gets(&m1, 0, 0); b = opd_gets(&m2, 0, 0); c = opd_gets(&n, 0, 0);
  if (a < 1 || a > 4 || b < 1 || b > a || c < 1 || c > 4 || b > c) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  sv = opd_get(&s, 0, 0); dv = opd_get(&d, 0, 0);
  bcd_mode = (u8)PLC_BIT_TEST(M8168);           /* M8168 ON: data is already BCD */
  if (!bcd_mode) {
    if (sv > 9999U || dv > 9999U) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
    sv = bin2bcd(sv, 4); dv = bin2bcd(dv, 4);
  }
  for (k = 0; k < b; ++k) {
    v = (sv >> (4 * (a - 1 - k))) & 15U;
    dv = (dv & ~(15UL << (4 * (c - 1 - k)))) | (v << (4 * (c - 1 - k)));
  }
  if (!bcd_mode && !bcd2bin(dv, 4, &dv)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  opd_put(&d, 0, dv, 0);
}

/**
 * Copy @p n elements from operand @p s to operand @p d (BMOV, SORT and friends).
 * When source and destination are in the same device and the destination starts above the
 * source, the copy runs backwards, so overlapping ranges transfer correctly. The copy
 * stops at the first operand fault.
 *
 * @param w  0 for 16-bit elements, 1 for 32-bit elements.
 */
void plc_exp_block_copy(const operand_t *s, const operand_t *d, u32 n, u8 w) {
  u32 i;
  /* Walk backwards when the destination lies above the source so that
   * overlapping ranges transfer correctly. */
  if (s->kind == d->kind && s->dev == d->dev && d->index > s->index) {
    for (i = n; i-- > 0 && !plc_exp_last_error;) opd_put(d, i, opd_get(s, i, w), w);
  } else {
    for (i = 0; i < n && !plc_exp_last_error; ++i) opd_put(d, i, opd_get(s, i, w), w);
  }
}

/** BMOV S D n: copy a block of n (1..512) 16-bit elements; overlapping blocks are handled. */
void BMOV(void) {
  operand_t s, d, n;
  u32 cnt;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 512);
  if (PLC_EXP_ENABLED) plc_exp_block_copy(&s, &d, cnt, 0);
}

/**
 * FMOV S D n: fill n (1..512) consecutive destination elements with the single source value.
 * @param w  0 for 16 bit, 1 for 32 bit.
 */
static void f_fmov(u8 w) {
  operand_t s, d, n;
  u32 cnt, i, v;
  opd_decode(&s, w); opd_decode(&d, w); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 512);
  if (!PLC_EXP_ENABLED) return;
  v = opd_get(&s, 0, w);
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) opd_put(&d, i, v, w);
}

/** FMOV S D n: 16-bit fill, see f_fmov(). */
void FMOV(void)  { f_fmov(0); }
/** DFMOV S D n: 32-bit fill, see f_fmov(). */
void DFMOV(void) { f_fmov(1); }

/**
 * XCH D1 D2: exchange the contents of two operands.
 * @param w  0 for 16 bit, 1 for 32 bit.
 */
static void f_xch(u8 w) {
  operand_t a, b;
  u32 x, y;
  opd_decode(&a, w); opd_decode(&b, w);
  if (!PLC_EXP_ENABLED) return;
  x = opd_get(&a, 0, w); y = opd_get(&b, 0, w);
  opd_put(&a, 0, y, w); opd_put(&b, 0, x, w);
}

/** XCH D1 D2: exchange two 16-bit values. */
void XCH(void)  { f_xch(0); }
/** DXCH D1 D2: exchange two 32-bit values. */
void DXCH(void) { f_xch(1); }

/**
 * BCD S D: convert a binary number to packed BCD. The source must be in 0..9999 (16 bit)
 * or 0..99999999 (32 bit), otherwise the PLC stops with PLC_RUNTIME_OPERAND.
 * @param w  0 for 16 bit, 1 for 32 bit.
 */
static void f_bcd(u8 w) {
  operand_t s, d;
  s32 v;
  opd_decode(&s, w); opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  v = opd_gets(&s, 0, w);
  if (v < 0 || v > (w ? 99999999 : 9999)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  opd_put(&d, 0, bin2bcd((u32)v, w ? 8U : 4U), w);
}

/**
 * BIN S D: convert packed BCD to a binary number. A source with a digit above 9 stops the
 * PLC with PLC_RUNTIME_OPERAND.
 * @param w  0 for 16 bit (4 digits), 1 for 32 bit (8 digits).
 */
static void f_bin(u8 w) {
  operand_t s, d;
  u32 v;
  opd_decode(&s, w); opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  if (!bcd2bin(opd_get(&s, 0, w), w ? 8U : 4U, &v)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  opd_put(&d, 0, v, w);
}

/** BCD S D: binary to BCD (16 bit). */
void BCD(void)  { f_bcd(0); }
/** DBCD S D: binary to BCD (32 bit). */
void DBCD(void) { f_bcd(1); }
/** BIN S D: BCD to binary (16 bit). */
void BIN(void)  { f_bin(0); }
/** DBIN S D: BCD to binary (32 bit). */
void DBIN(void) { f_bin(1); }

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_compare_move_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
