/**
 * @file plc_exp_rotate_shift.c
 * @brief ROR/ROL/RCR/RCL, bit and word shifts (SFTR/SFTL/WSFR/WSFL) and the SFWR/SFWL FIFO.
 *
 * The carry flag M8022 is used by the rotations through carry and by SFWR (full table);
 * the zero flag M8020 reports an empty table after SFWL. Table and string sizes are
 * validated with opd_count(), so the loops cannot run past the operands.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Rotation and shift. A KnM destination must span the whole word          */
/* (K4 / K8); the carry flag M8022 receives the last bit moved out.        */
/* ====================================================================== */
/**
 * ROR/ROL/RCR/RCL D n: rotate D by n bit positions.
 *
 * ROR rotates right and ROL left. RCR and RCL rotate through the carry flag M8022, which
 * takes part as an extra bit. Afterwards M8022 holds the last bit that was moved out.
 * n = 0 does nothing; n must be 0..16 (0..32 for the 32-bit forms), and a bit-group
 * destination must span the whole word (K4, or K8 for 32 bit); otherwise the PLC stops
 * with PLC_RUNTIME_OPERAND.
 *
 * @param w     0 for 16 bit, 1 for 32 bit.
 * @param kind  0 = ROR, 1 = ROL, 2 = RCR, 3 = RCL.
 */
static void f_rot(u8 w, u8 kind) {                /* 0 ROR, 1 ROL, 2 RCR, 3 RCL */
  operand_t d, n;
  u32 width = w ? 32U : 16U, v, c, out, cnt, i;
  s32 k;
  opd_decode(&d, w); opd_decode(&n, 0);
  if (!PLC_EXP_ENABLED) return;
  k = opd_gets(&n, 0, 0);
  if (k < 0 || (u32)k > width || (d.kind == OPD_GROUP && d.digits != (w ? 8U : 4U))) {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
    return;
  }
  cnt = (u32)k;
  if (!cnt) return;
  v = opd_get(&d, 0, w);
  c = PLC_BIT_TEST(M8022);
  for (i = 0; i < cnt; ++i) {
    if (kind == 0U || kind == 2U) {               /* right */
      out = v & 1U;
      v = (v >> 1) | ((kind == 0U ? out : c) << (width - 1U));
    } else {                                      /* left */
      out = (v >> (width - 1U)) & 1U;
      v = (v << 1) | (kind == 1U ? out : c);
    }
    c = out;
    if (!w) v &= 0xFFFFU;
  }
  opd_put(&d, 0, v, w);
  plc_exp_set_flag(M8022, (u8)c);
}
/** ROR D n: rotate right (16 bit). */
void ROR(void)   { f_rot(0, 0); }
/** DROR D n: rotate right (32 bit). */
void DROR(void)  { f_rot(1, 0); }
/** ROL D n: rotate left (16 bit). */
void ROL(void)   { f_rot(0, 1); }
/** DROL D n: rotate left (32 bit). */
void DROL(void)  { f_rot(1, 1); }
/** RCR D n: rotate right through the carry flag (16 bit). */
void RCR(void)   { f_rot(0, 2); }
/** DRCR D n: rotate right through the carry flag (32 bit). */
void DRCR(void)  { f_rot(1, 2); }
/** RCL D n: rotate left through the carry flag (16 bit). */
void RCL(void)   { f_rot(0, 3); }
/** DRCL D n: rotate left through the carry flag (32 bit). */
void DRCL(void)  { f_rot(1, 3); }

/**
 * SFTR/SFTL S D n1 n2: bit shift. D is a string of n1 (1..1024) bits that is shifted by
 * n2 (1..n1) positions; the n2 bits starting at S enter at the vacated end. SFTR shifts
 * toward the lower bit numbers (S enters at the top of the string), SFTL toward the higher
 * ones (S enters at the bottom). S and D must be bit devices; n2 above n1 stops the PLC
 * with PLC_RUNTIME_OPERAND.
 * @param left  0 for SFTR, 1 for SFTL.
 */
static void f_sft(u8 left) {
  operand_t s, d, a, b;
  u32 n1, n2, i, j;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&a, 0); opd_decode(&b, 0);
  if (plc_exp_last_error) return;
  n1 = opd_count(&a, 1, 1024); n2 = opd_count(&b, 1, 1024);
  if (n2 > n1) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (!PLC_EXP_ENABLED) return;
  if (left) {
    for (i = n1; i-- > n2 && !plc_exp_last_error;) opd_bit_put(&d, i, opd_bit_get(&d, i - n2));
    for (j = 0; j < n2; ++j) opd_bit_put(&d, j, opd_bit_get(&s, j));
  } else {
    for (i = 0; i + n2 < n1 && !plc_exp_last_error; ++i) opd_bit_put(&d, i, opd_bit_get(&d, i + n2));
    for (j = 0; j < n2; ++j) opd_bit_put(&d, n1 - n2 + j, opd_bit_get(&s, j));
  }
}
/** SFTR S D n1 n2: shift a bit string right. */
void SFTR(void) { f_sft(0); }
/** SFTL S D n1 n2: shift a bit string left. */
void SFTL(void) { f_sft(1); }

/**
 * WSFR/WSFL S D n1 n2: word shift. D is a table of n1 (1..512) words that is shifted by n2
 * (1..n1) words; the n2 words starting at S enter at the vacated end. WSFR shifts toward
 * lower word numbers, WSFL toward higher ones. n2 above n1 stops the PLC with
 * PLC_RUNTIME_OPERAND.
 * @param left  0 for WSFR, 1 for WSFL.
 */
static void f_wsft(u8 left) {
  operand_t s, d, a, b;
  u32 n1, n2, i, j;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&a, 0); opd_decode(&b, 0);
  if (plc_exp_last_error) return;
  n1 = opd_count(&a, 1, 512); n2 = opd_count(&b, 1, 512);
  if (n2 > n1) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (!PLC_EXP_ENABLED) return;
  if (left) {
    for (i = n1; i-- > n2 && !plc_exp_last_error;) opd_put(&d, i, opd_get(&d, i - n2, 0), 0);
    for (j = 0; j < n2; ++j) opd_put(&d, j, opd_get(&s, j, 0), 0);
  } else {
    for (i = 0; i + n2 < n1 && !plc_exp_last_error; ++i) opd_put(&d, i, opd_get(&d, i + n2, 0), 0);
    for (j = 0; j < n2; ++j) opd_put(&d, n1 - n2 + j, opd_get(&s, j, 0), 0);
  }
}
/** WSFR S D n1 n2: shift a word table right. */
void WSFR(void) { f_wsft(0); }
/** WSFL S D n1 n2: shift a word table left. */
void WSFL(void) { f_wsft(1); }

/**
 * SFWR S D n: FIFO write. D is a table of n (2..512) registers: D holds the pointer (the
 * number of stored values) and the data live in D+1..D+n-1. The value S is appended behind
 * the last stored value and the pointer is incremented. When the table is full (pointer =
 * n-1) nothing is written and the carry flag M8022 is set; otherwise M8022 is cleared. A
 * pointer outside 0..n-1 stops the PLC with PLC_RUNTIME_OPERAND.
 */
void SFWR(void) {
  operand_t s, d, n;
  u32 cnt, p;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 2, 512);
  if (!PLC_EXP_ENABLED) return;
  p = opd_get(&d, 0, 0);
  if (p >= cnt) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (p == cnt - 1U) { plc_exp_set_flag(M8022, 1); return; }
  opd_put(&d, p + 1U, opd_get(&s, 0, 0), 0);
  opd_put(&d, 0, p + 1U, 0);
  plc_exp_set_flag(M8022, 0);
}
/**
 * SFWL S D n: FIFO read (the Mitsubishi instruction SFRD, FNC39). S is a table written by
 * SFWR (pointer in S, data in S+1..S+n-1): the oldest value, S+1, is moved to D, the
 * remaining values move down by one place and the pointer is decremented. M8020 (zero) is
 * set when the table has become empty; reading a table that is already empty only sets
 * M8020. A pointer outside 0..n-1 stops the PLC with PLC_RUNTIME_OPERAND.
 */
void SFWL(void) {
  operand_t s, d, n;
  u32 cnt, p, i;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 2, 512);
  if (!PLC_EXP_ENABLED) return;
  p = opd_get(&s, 0, 0);
  if (p >= cnt) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (p == 0U) { plc_exp_set_flag(M8020, 1); return; }
  opd_put(&d, 0, opd_get(&s, 1, 0), 0);
  for (i = 1; i < p && !plc_exp_last_error; ++i) opd_put(&s, i, opd_get(&s, i + 1U, 0), 0);
  opd_put(&s, 0, p - 1U, 0);
  plc_exp_set_flag(M8020, p == 1U);
}

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_rotate_shift_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
