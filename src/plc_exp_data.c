/**
 * @file plc_exp_data.c
 * @brief Data-processing instructions: ZRST, DECO/ENCO, SUM/BON/MEAN, SQR, annunciators,
 *        byte/nibble/word handling, Gray code, 7-segment, SORT/SER/ABSD, CCD and ZPUSH/ZPOP.
 *
 * All handlers follow the common instruction pattern of the experimental interpreter:
 * decode every operand first, check the count operands, then act only if the instruction
 * is enabled (PLC_EXP_ENABLED). Counts are validated with opd_count(), so a bad count
 * stops the PLC with PLC_RUNTIME_OPERAND instead of running over the end of a device.
 * Instructions that exist in a 32-bit form take the D prefix (DSUM, DBON, DMEAN, ...).
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Data processing                                                         */
/* ====================================================================== */
/**
 * ZRST D1 D2: clear the range D1..D2 while the rung is ON.
 *
 * Both operands must be of the same kind and D1 must not be above D2: either word devices
 * of the same family (D, T or C; at most 512 registers) or bit devices (at most 1024 bits,
 * subject to the write rules). Timers and counters in the range are reset like RST, which
 * also clears their contacts. Anything else stops the PLC with PLC_RUNTIME_OPERAND.
 */
void ZRST(void) {
  operand_t a, b, t;
  u32 i;
  opd_decode(&a, 0); opd_decode(&b, 0);
  if (!PLC_EXP_ENABLED) return;
  if (a.kind == OPD_WORD && b.kind == OPD_WORD && a.dev == b.dev && a.index <= b.index && b.index - a.index < 512U) {
    t = a;
    for (i = a.index; i <= b.index && !plc_exp_last_error; ++i) {
      t.index = i;
      if (a.dev == DEV_T && i < 256U) plc_exp_timer_reset(i);
      else if (a.dev == DEV_C && i < 256U) plc_exp_counter_reset(i);
      else opd_put(&t, 0, 0, 0);
    }
  } else if (a.kind == OPD_BIT && b.kind == OPD_BIT && a.index <= b.index && b.index - a.index < 1024U) {
    for (i = a.index; i <= b.index && !plc_exp_last_error; ++i) plc_exp_bit_write(i, 0);
  } else {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
  }
}

/**
 * DECO S D n: decode. The n-bit number S (bits of a bit device, or a word) selects one bit
 * of the 2^n-bit field D, which is set; all other bits of the field are cleared.
 * n = 1..8 for a bit-device field; a word destination holds at most 16 bits (n <= 4).
 */
void DECO(void) {
  operand_t s, d, n;
  u32 cnt, v, i;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 8);
  if (!PLC_EXP_ENABLED) return;
  v = opd_bits(&s, cnt);
  if (d.kind == OPD_BIT) {
    for (i = 0; i < (1UL << cnt) && !plc_exp_last_error; ++i) opd_bit_put(&d, i, i == v);
  } else if (d.kind == OPD_WORD && cnt <= 4U) {
    opd_put(&d, 0, 1UL << v, 0);
  } else {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
  }
}
/**
 * ENCO S D n: encode. Finds the highest ON bit of the 2^n-bit field S (a bit device, or a
 * word when n <= 4) and stores its position in the n low bits of D (a bit device) or in D
 * (a word). A field without any ON bit stops the PLC with PLC_RUNTIME_OPERAND.
 */
void ENCO(void) {
  operand_t s, d, n;
  u32 cnt, i, pos = 0, found = 0;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 8);
  if (!PLC_EXP_ENABLED) return;
  if (s.kind == OPD_BIT) {
    for (i = 0; i < (1UL << cnt) && !plc_exp_last_error; ++i) if (opd_bit_get(&s, i)) { pos = i; found = 1; }
  } else if (s.kind == OPD_WORD && cnt <= 4U) {
    for (i = 0; i < (1UL << cnt); ++i) if ((opd_get(&s, 0, 0) >> i) & 1U) { pos = i; found = 1; }
  } else {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
  }
  if (plc_exp_last_error) return;
  if (!found) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (d.kind == OPD_BIT) { for (i = 0; i < cnt; ++i) opd_bit_put(&d, i, (u8)((pos >> i) & 1U)); }
  else opd_put(&d, 0, pos, 0);
}

/**
 * SUM S D: count the ON bits of S and store the count in D. M8020 is set when the count is
 * zero.
 * @param w  0 for a 16-bit source, 1 for a 32-bit source.
 */
static void f_sum(u8 w) {
  operand_t s, d;
  u32 v, c = 0;
  opd_decode(&s, w); opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  for (v = opd_get(&s, 0, w); v; v &= v - 1U) ++c;
  opd_put(&d, 0, c, w);
  plc_exp_set_flag(M8020, c == 0U);
}
/** SUM S D: number of ON bits of a 16-bit value. */
void SUM(void)  { f_sum(0); }
/** DSUM S D: number of ON bits of a 32-bit value. */
void DSUM(void) { f_sum(1); }

/**
 * BON S D n: D = bit n of S. n is 0..15 (0..31 for the 32-bit form); anything else stops
 * the PLC with PLC_RUNTIME_OPERAND. D must be a bit device.
 * @param w  0 for a 16-bit source, 1 for a 32-bit source.
 */
static void f_bon(u8 w) {
  operand_t s, d, n;
  s32 k;
  opd_decode(&s, w); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  k = opd_gets(&n, 0, 0);
  if (k < 0 || k > (w ? 31 : 15)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (PLC_EXP_ENABLED) opd_bit_put(&d, 0, (u8)((opd_get(&s, 0, w) >> k) & 1U));
}
/** BON S D n: test bit n of a 16-bit value. */
void BON(void)  { f_bon(0); }
/** DBON S D n: test bit n of a 32-bit value. */
void DBON(void) { f_bon(1); }

/**
 * MEAN S D n: D = average of the n (1..64) consecutive values starting at S, computed from
 * the exact sum and truncated toward zero.
 * @param w  0 for 16-bit values, 1 for 32-bit values.
 */
static void f_mean(u8 w) {
  operand_t s, d, n;
  u32 cnt, i;
  int64_t sum = 0;
  opd_decode(&s, w); opd_decode(&d, w); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 64);
  if (!PLC_EXP_ENABLED) return;
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) sum += opd_gets(&s, i, w);
  opd_put(&d, 0, (u32)(sum / (int64_t)cnt), w);
}
/** MEAN S D n: average of 16-bit values. */
void MEAN(void)  { f_mean(0); }
/** DMEAN S D n: average of 32-bit values. */
void DMEAN(void) { f_mean(1); }

/* ANS S m D / ANR: annunciators S900..S999. Timer S (T0..T199) times m x 100 ms
 * of continuous input; M8049 enables the M8048 flag and D8049 (lowest active). */

/**
 * Refresh the annunciator status. D8049 receives the number of the lowest ON annunciator
 * among S900..S999 (0 if none); M8048 is set when any annunciator is ON and M8049 has
 * enabled the flag.
 */
static void ann_update(void) {
  u32 i, first = 0;
  u8 any = 0;
  for (i = 900; i < 1000U; ++i)
    if (PLC_BIT_TEST(Addr_S * 8U + i)) { first = i; any = 1; break; }
  plc_exp_set_flag(M8048, any && PLC_BIT_TEST(M8049));
  D8049 = (u16)first;
}
/**
 * ANS S m D: annunciator set. S must be a timer T0..T199 (100 ms base), m = 1..32767 is
 * the delay in units of 100 ms and D one of the annunciators S900..S999. When the rung has
 * been ON for m x 100 ms the annunciator D is set; it stays set when the rung drops until
 * ANR resets it. The timer is reset while the rung is OFF. Any other operand kind stops
 * the PLC with PLC_RUNTIME_OPERAND.
 */
void ANS(void) {
  operand_t s, m, d;
  s32 t;
  u32 n;
  u8 acc = PLC_ACC_BIT & 1U;
  opd_decode(&s, 0); opd_decode(&m, 0); opd_decode(&d, 0);
  if (plc_exp_last_error) return;
  n = opd_count(&m, 1, 32767);
  if (s.kind != OPD_WORD || s.dev != DEV_T || s.index >= 200U || d.kind != OPD_BIT ||
      d.index < Addr_S * 8U + 900U || d.index >= Addr_S * 8U + 1000U) {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
    return;
  }
  t = (s32)s.index;
  PLC_16BIT(Addr_TMax + (u32)t * 2U) = (u16)n;
  plc_exp_put_bit(Addr_TE * 8U + (u32)t, acc);
  if (!acc) { plc_exp_timer_reset((u32)t); return; }
  plc_exp_put_bit(Addr_TO * 8U + (u32)t, PLC_16BIT(Addr_TVal + (u32)t * 2U) >= n);
  if (PLC_16BIT(Addr_TVal + (u32)t * 2U) >= n) { plc_exp_put_bit(d.index, 1); ann_update(); }
}
/**
 * ANR: annunciator reset. Every execution (normally the pulse form ANRP) resets the lowest
 * ON annunciator of S900..S999 and refreshes D8049 and M8048.
 */
void ANR(void) {
  u32 i;
  if (!PLC_EXP_ENABLED) return;
  for (i = 900; i < 1000U; ++i)
    if (PLC_BIT_TEST(Addr_S * 8U + i)) { plc_exp_put_bit(Addr_S * 8U + i, 0); break; }
  ann_update();
}

/** Integer square root, rounded down (bit-by-bit method, no floating point). */
static u32 isqrt32(u32 v) {
  u32 r = 0, bit = 1UL << 30;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
    bit >>= 2;
  }
  return r;
}
/**
 * SQR S D: D = integer square root of S, rounded down. A negative source stops the PLC with
 * PLC_RUNTIME_OPERAND. M8020 is set when the result is zero and M8021 when the result was
 * rounded (S is not a perfect square).
 * @param w  0 for 16 bit, 1 for 32 bit.
 */
static void f_sqr(u8 w) {
  operand_t s, d;
  s32 v;
  u32 r;
  opd_decode(&s, w); opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  v = opd_gets(&s, 0, w);
  if (v < 0) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  r = isqrt32((u32)v);
  opd_put(&d, 0, r, w);
  plc_exp_set_flag(M8020, r == 0U);
  plc_exp_set_flag(M8021, (u32)v != r * r);                 /* result was truncated */
}
/** SQR S D: integer square root (16 bit). */
void SQR(void)  { f_sqr(0); }
/** DSQR S D: integer square root (32 bit). */
void DSQR(void) { f_sqr(1); }


/* ====================================================================== */
/* Word/byte/nibble handling, gray code, 7-segment, sorting, searching      */
/* ====================================================================== */
/**
 * WSUM S D n: sum of the n (1..256) consecutive values starting at S. The 16-bit form
 * stores a 32-bit sum in D and D+1; the 32-bit form stores a 64-bit sum in four registers.
 * @param w  0 for 16-bit values, 1 for 32-bit values.
 */
static void f_wsum(u8 w) {
  operand_t s, d, n;
  u32 cnt, i;
  int64_t sum = 0;
  opd_decode(&s, w); opd_decode(&d, w ? 1U : 2U); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 256);
  if (!PLC_EXP_ENABLED) return;
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) sum += opd_gets(&s, i, w);
  opd_put(&d, 0, (u32)sum, 1);
  if (w) opd_put(&d, 1, (u32)((uint64_t)sum >> 32), 1);
}
/** WSUM S D n: sum of 16-bit values (32-bit result). */
void WSUM(void)  { f_wsum(0); }
/** DWSUM S D n: sum of 32-bit values (64-bit result). */
void DWSUM(void) { f_wsum(1); }

/**
 * WTOB S D n: word to byte. The n (1..256) bytes of S, low byte of each word first, are
 * stored in the low bytes of n consecutive words of D; the high bytes are cleared.
 */
void WTOB(void) {
  operand_t s, d, n;
  u32 cnt, i, v;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 256);
  if (!PLC_EXP_ENABLED) return;
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) {
    v = opd_get(&s, i / 2U, 0);
    opd_put(&d, i, (i & 1U) ? (v >> 8) & 0xFFU : v & 0xFFU, 0);
  }
}
/**
 * BTOW S D n: byte to word. The low bytes of n (1..256) consecutive words of S are packed
 * two per word into D (the first byte in the low half). An odd n leaves the high byte of
 * the last word zero.
 */
void BTOW(void) {
  operand_t s, d, n;
  u32 cnt, i, v = 0;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 256);
  if (!PLC_EXP_ENABLED) return;
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) {
    if (i & 1U) { v |= (opd_get(&s, i, 0) & 0xFFU) << 8; opd_put(&d, i / 2U, v, 0); }
    else { v = opd_get(&s, i, 0) & 0xFFU; if (i + 1U == cnt) opd_put(&d, i / 2U, v, 0); }
  }
}
/**
 * UNI S D n: unite. The low nibbles of n (1..4) consecutive words of S are combined into
 * one word D (nibble i of D = low nibble of word i of S).
 */
void UNI(void) {
  operand_t s, d, n;
  u32 cnt, i, v = 0;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 4);
  if (!PLC_EXP_ENABLED) return;
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) v |= (opd_get(&s, i, 0) & 0xFU) << (4U * i);
  opd_put(&d, 0, v, 0);
}
/**
 * DIS S D n: disperse, the inverse of UNI. The n (1..4) nibbles of S are stored in the low
 * nibbles of n consecutive words starting at D; the other bits of those words are cleared.
 */
void DIS(void) {
  operand_t s, d, n;
  u32 cnt, i, v;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 4);
  if (!PLC_EXP_ENABLED) return;
  v = opd_get(&s, 0, 0);
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) opd_put(&d, i, (v >> (4U * i)) & 0xFU, 0);
}
/**
 * SWAP D: exchange the high and low byte of each 16-bit word of D. The 32-bit form swaps
 * the bytes inside both words and leaves the words in place.
 * @param w  0 for a 16-bit operand, 1 for a 32-bit operand.
 */
static void f_swap(u8 w) {
  operand_t d;
  u32 v;
  opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  v = opd_get(&d, 0, w);
  opd_put(&d, 0, ((v & 0x00FF00FFUL) << 8) | ((v >> 8) & 0x00FF00FFUL), w);
}
/** SWAP D: exchange the bytes of a 16-bit word. */
void SWAP(void)  { f_swap(0); }
/** DSWAP D: exchange the bytes inside each word of a 32-bit value. */
void DSWAP(void) { f_swap(1); }

/**
 * GRY S D / GBIN S D: convert between binary and Gray code.
 * @param w        0 for 16 bit, 1 for 32 bit.
 * @param inverse  0 = binary to Gray code (GRY), non-zero = Gray code to binary (GBIN).
 */
static void f_gray(u8 w, u8 inverse) {
  operand_t s, d;
  u32 v, sh;
  opd_decode(&s, w); opd_decode(&d, w);
  if (!PLC_EXP_ENABLED) return;
  v = opd_get(&s, 0, w);
  if (!inverse) v ^= v >> 1;
  else for (sh = 1; sh < 32U; sh <<= 1) v ^= v >> sh;
  opd_put(&d, 0, v, w);
}
/** GRY S D: binary to Gray code (16 bit). */
void GRY(void)   { f_gray(0, 0); }
/** DGRY S D: binary to Gray code (32 bit). */
void DGRY(void)  { f_gray(1, 0); }
/** GBIN S D: Gray code to binary (16 bit). */
void GBIN(void)  { f_gray(0, 1); }
/** DGBIN S D: Gray code to binary (32 bit). */
void DGBIN(void) { f_gray(1, 1); }

/** Seven-segment patterns of the hexadecimal digits 0..F (bit 0 = segment a ... bit 6 = segment g). */
static const u8 seg7[16] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71};
/** SEGD S D: D = seven-segment pattern of the low nibble of S (bit 0 = a ... bit 6 = g, bit 7 = 0). */
void SEGD(void) {
  operand_t s, d;
  opd_decode(&s, 0); opd_decode(&d, 0);
  if (PLC_EXP_ENABLED) opd_put(&d, 0, seg7[opd_get(&s, 0, 0) & 0xFU], 0);
}

/**
 * ALT D: alternate output. Toggles the bit D on every execution; use the pulse form ALTP,
 * otherwise the bit would toggle in every scan.
 */
void ALT(void) {
  operand_t d;
  opd_decode(&d, 0);
  if (PLC_EXP_ENABLED) opd_bit_put(&d, 0, !opd_bit_get(&d, 0));
}

/**
 * SORT/SORT2 S m1 m2 D n: sort a table by one column.
 *
 * The table has m1 rows (1..32) and m2 columns (1..6) stored row by row; n (1..m2) selects
 * the key column. The table is copied from S to D and sorted there with a stable insertion
 * sort, so S is not modified. SORT always sorts ascending; SORT2 and DSORT2 sort
 * descending while M8165 is ON. M8029 (execution complete) is set when the sort is done,
 * which happens within the scan.
 *
 * @param w          0 for 16-bit elements, 1 for 32-bit elements.
 * @param use_m8165  non-zero if M8165 selects descending order (SORT2 family).
 */
static void f_sort(u8 w, u8 use_m8165) {
  operand_t s, a, b, d, n;
  u32 rows, cols, key, i, j, k, tot;
  u8 desc;
  opd_decode(&s, w); opd_decode(&a, 0); opd_decode(&b, 0); opd_decode(&d, w); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  rows = opd_count(&a, 1, 32); cols = opd_count(&b, 1, 6); key = opd_count(&n, 1, cols);
  if (!PLC_EXP_ENABLED) return;
  tot = rows * cols;
  desc = use_m8165 && PLC_BIT_TEST(M8165);
  plc_exp_block_copy(&s, &d, tot, w);
  for (i = 1; i < rows && !plc_exp_last_error; ++i) {
    for (j = i; j > 0U && !plc_exp_last_error; --j) {
      s32 x = opd_gets(&d, (j - 1U) * cols + key - 1U, w), y = opd_gets(&d, j * cols + key - 1U, w);
      if (desc ? x >= y : x <= y) break;
      for (k = 0; k < cols; ++k) {
        u32 p = opd_get(&d, (j - 1U) * cols + k, w), q = opd_get(&d, j * cols + k, w);
        opd_put(&d, (j - 1U) * cols + k, q, w); opd_put(&d, j * cols + k, p, w);
      }
    }
  }
  plc_exp_set_flag(M8029, 1);
}
/** SORT S m1 m2 D n: ascending table sort with 16-bit elements. */
void SORT(void)   { f_sort(0, 0); }
/** SORT2 S m1 m2 D n: table sort with 16-bit elements, descending while M8165 is ON. */
void SORT2(void)  { f_sort(0, 1); }
/** DSORT2 S m1 m2 D n: table sort with 32-bit elements, descending while M8165 is ON. */
void DSORT2(void) { f_sort(1, 1); }

/**
 * SER S1 S2 D n: search n (1..256) consecutive values starting at S1 for the value S2.
 * Five consecutive registers are written from D: the number of matches, the position of the
 * first match, the position of the last match (both 0 if there is none), the position of
 * the minimum value and the position of the maximum value. Positions count from 0; when
 * the minimum or maximum occurs several times the last one is reported.
 * @param w  0 for 16-bit values, 1 for 32-bit values.
 */
static void f_ser(u8 w) {
  operand_t s, v, d, n;
  u32 cnt, i, hits = 0, first = 0, last = 0, mn = 0, mx = 0;
  s32 key, x, lo = 0, hi = 0;
  opd_decode(&s, w); opd_decode(&v, w); opd_decode(&d, w); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 256);
  if (!PLC_EXP_ENABLED) return;
  key = opd_gets(&v, 0, w);
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) {
    x = opd_gets(&s, i, w);
    if (i == 0U || x <= lo) { lo = x; mn = i; }
    if (i == 0U || x >= hi) { hi = x; mx = i; }
    if (x == key) { if (!hits) first = i; last = i; ++hits; }
  }
  opd_put(&d, 0, hits, w); opd_put(&d, 1, first, w); opd_put(&d, 2, last, w); opd_put(&d, 3, mn, w); opd_put(&d, 4, mx, w);
}
/** SER S1 S2 D n: search 16-bit values. */
void SER(void)  { f_ser(0); }
/** DSER S1 S2 D n: search 32-bit values. */
void DSER(void) { f_ser(1); }

/**
 * ABSD S1 S2 D n: absolute drum sequencer. S1 is a table of n (1..64) pairs of values (ON
 * point, OFF point), S2 the current counter value and D the first of n output bits. Output
 * k is ON while ON_k <= counter < OFF_k; when ON_k is above OFF_k the range wraps around
 * (ON while counter >= ON_k or counter < OFF_k).
 * @param w  0 for 16-bit values, 1 for 32-bit values.
 */
static void f_absd(u8 w) {
  operand_t t, c, d, n;
  u32 cnt, k;
  s32 on, off, cur;
  opd_decode(&t, w); opd_decode(&c, w); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 64);
  if (!PLC_EXP_ENABLED) return;
  cur = opd_gets(&c, 0, w);
  for (k = 0; k < cnt && !plc_exp_last_error; ++k) {
    on = opd_gets(&t, 2U * k, w); off = opd_gets(&t, 2U * k + 1U, w);
    opd_bit_put(&d, k, on < off ? (cur >= on && cur < off) : (cur >= on || cur < off));
  }
}
/** ABSD S1 S2 D n: absolute drum with 16-bit values. */
void ABSD(void)  { f_absd(0); }
/** DABSD S1 S2 D n: absolute drum with 32-bit values. */
void DABSD(void) { f_absd(1); }

/**
 * CCD S D n: check code. Computes two checks over n (1..256) bytes starting at S: D = the
 * sum of the bytes (low 16 bits) and D+1 = the exclusive OR of the bytes (horizontal
 * parity). With M8161 OFF every word holds two bytes, low byte first; with M8161 ON only
 * the low byte of each word is used.
 */
void CCD(void) {
  operand_t s, d, n;
  u32 cnt, i, v, sum = 0, par = 0;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  cnt = opd_count(&n, 1, 256);
  if (!PLC_EXP_ENABLED) return;
  for (i = 0; i < cnt && !plc_exp_last_error; ++i) {
    if (PLC_BIT_TEST(M8161)) v = opd_get(&s, i, 0) & 0xFFU;
    else { v = opd_get(&s, i / 2U, 0); v = (i & 1U) ? (v >> 8) & 0xFFU : v & 0xFFU; }
    sum += v; par ^= v;
  }
  opd_put(&d, 0, sum & 0xFFFFU, 0); opd_put(&d, 1, par, 0);
}

/**
 * ZPUSH D: save the index registers on a stack in the data registers.
 * D holds the number of saved blocks (clear it before the first push); every push appends
 * 16 words in the order Z0, V0, Z1, V1 ... Z7, V7 behind the counter and increments it.
 */
void ZPUSH(void) {
  operand_t d;
  u32 cnt, k;
  opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  cnt = opd_get(&d, 0, 0);
  for (k = 0; k < 16U && !plc_exp_last_error; ++k)
    opd_put(&d, 1U + 16U * cnt + k, plc_exp_index_reg((u8)((k & 1U) ^ 1U), (u8)(k >> 1)), 0);
  opd_put(&d, 0, cnt + 1U, 0);
}
/**
 * ZPOP D: restore the index registers saved by ZPUSH.
 * Takes the last block off the stack (Z0, V0 ... Z7, V7) and decrements the counter in D.
 * Popping an empty stack stops the PLC with PLC_RUNTIME_OPERAND.
 */
void ZPOP(void) {
  operand_t d;
  u32 cnt, k;
  opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  cnt = opd_get(&d, 0, 0);
  if (cnt == 0U) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  --cnt;
  for (k = 0; k < 16U && !plc_exp_last_error; ++k)
    plc_exp_index_reg_write((u8)((k & 1U) ^ 1U), (u8)(k >> 1), (u16)opd_get(&d, 1U + 16U * cnt + k, 0));
  opd_put(&d, 0, cnt, 0);
}

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_data_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
