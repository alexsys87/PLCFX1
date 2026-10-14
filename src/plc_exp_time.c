/**
 * @file plc_exp_time.c
 * @brief Time instructions: TCMP/TZCP, TADD/TSUB, HTOS/STOH, TRD/TWR and the HOUR meter.
 *
 * A time operand is three consecutive 16-bit registers: hours (0..23), minutes (0..59) and
 * seconds (0..59). Values outside these ranges stop the PLC with PLC_RUNTIME_OPERAND.
 * The real-time clock is read from D8013..D8019 and written through RTC_Set()
 * (plc_rtc.c).
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Time                                                                    */
/* ====================================================================== */
/**
 * Read a time operand and convert it to seconds since midnight.
 * @param o   time operand: three consecutive words h, m, s.
 * @param ok  set to 0 if any field is out of range; left unchanged otherwise.
 */
static u32 time_secs(const operand_t *o, u8 *ok) {      /* three consecutive words: h, m, s */
  u32 h = opd_get(o, 0, 0), m = opd_get(o, 1, 0), s = opd_get(o, 2, 0);
  if (h > 23U || m > 59U || s > 59U) *ok = 0;
  return h * 3600U + m * 60U + s;
}
/** Store @p t (seconds since midnight, below 86400) in a time operand as h, m, s. */
static void put_time(const operand_t *o, u32 t) {
  opd_put(o, 0, t / 3600U, 0); opd_put(o, 1, (t / 60U) % 60U, 0); opd_put(o, 2, t % 60U, 0);
}
/**
 * TCMP S1 S2 S3 S D: compare a reference time with the time S. S1, S2 and S3 are the hours,
 * minutes and seconds of the reference (constants or words); S is a time operand. Three
 * consecutive bits are written from D: D = (reference > S), D+1 = (reference == S),
 * D+2 = (reference < S).
 */
void TCMP(void) {
  operand_t a, b, c, s, d;
  u32 ref, tm;
  u8 ok = 1;
  opd_decode(&a, 0); opd_decode(&b, 0); opd_decode(&c, 0); opd_decode(&s, 0); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  if (opd_get(&a, 0, 0) > 23U || opd_get(&b, 0, 0) > 59U || opd_get(&c, 0, 0) > 59U) ok = 0;
  ref = opd_get(&a, 0, 0) * 3600U + opd_get(&b, 0, 0) * 60U + opd_get(&c, 0, 0);
  tm = time_secs(&s, &ok);
  if (!ok) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  opd_bit_put(&d, 0, ref > tm); opd_bit_put(&d, 1, ref == tm); opd_bit_put(&d, 2, ref < tm);
}
/**
 * TZCP S1 S2 S D: zone compare of the time S with the range S1..S2 (all time operands).
 * D = (S < S1), D+1 = (S1 <= S <= S2), D+2 = (S > S2). If S1 is above S2 the range is S1..S1.
 */
void TZCP(void) {
  operand_t a, b, s, d;
  u32 lo, hi, tm;
  u8 ok = 1;
  opd_decode(&a, 0); opd_decode(&b, 0); opd_decode(&s, 0); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  lo = time_secs(&a, &ok); hi = time_secs(&b, &ok); tm = time_secs(&s, &ok);
  if (!ok) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (hi < lo) hi = lo;
  opd_bit_put(&d, 0, tm < lo); opd_bit_put(&d, 1, tm >= lo && tm <= hi); opd_bit_put(&d, 2, tm > hi);
}
/**
 * TADD/TSUB S1 S2 D: add or subtract two times, modulo 24 hours. M8022 (carry) is set when
 * an addition reaches 24 hours, M8021 (borrow) when a subtraction goes below zero, and
 * M8020 (zero) when the stored result is 00:00:00.
 * @param sub  0 = TADD, 1 = TSUB.
 */
static void f_tadd(u8 sub) {
  operand_t a, b, d;
  u32 x, y;
  s32 r;
  u8 ok = 1;
  opd_decode(&a, 0); opd_decode(&b, 0); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  x = time_secs(&a, &ok); y = time_secs(&b, &ok);
  if (!ok) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  r = sub ? (s32)x - (s32)y : (s32)(x + y);
  plc_exp_set_flag(M8022, !sub && r >= 86400);
  plc_exp_set_flag(M8021, sub && r < 0);
  if (r >= 86400) r -= 86400;
  if (r < 0) r += 86400;
  plc_exp_set_flag(M8020, r == 0);
  put_time(&d, (u32)r);
}
/** TADD S1 S2 D: D = S1 + S2 (time). */
void TADD(void) { f_tadd(0); }
/** TSUB S1 S2 D: D = S1 - S2 (time). */
void TSUB(void) { f_tadd(1); }

/**
 * HTOS S D: convert a time (h, m, s) to seconds. The result must fit the signed 16-bit
 * destination (at most 32767 s, about 9 h 6 min), otherwise the PLC stops with
 * PLC_RUNTIME_OPERAND.
 */
void HTOS(void) {
  operand_t s, d;
  u32 t;
  u8 ok = 1;
  opd_decode(&s, 0); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  t = time_secs(&s, &ok);
  if (!ok || t > 32767U) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  opd_put(&d, 0, t, 0);
}
/** STOH S D: convert seconds (S >= 0) to a time (h, m, s) in three registers from D; the time wraps at 24 h. */
void STOH(void) {
  operand_t s, d;
  s32 t;
  opd_decode(&s, 0); opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  t = opd_gets(&s, 0, 0);
  if (t < 0) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  put_time(&d, (u32)t % 86400U);
}
/**
 * TRD D: read the real-time clock into seven registers from D: year, month, day, hour,
 * minute, second and weekday, taken from D8018..D8013 and D8019.
 */
void TRD(void) {
  operand_t d;
  opd_decode(&d, 0);
  if (!PLC_EXP_ENABLED) return;
  opd_put(&d, 0, D8018, 0); opd_put(&d, 1, D8017, 0); opd_put(&d, 2, D8016, 0); opd_put(&d, 3, D8015, 0);
  opd_put(&d, 4, D8014, 0); opd_put(&d, 5, D8013, 0); opd_put(&d, 6, D8019, 0);
}
/**
 * TWR S: set the real-time clock from six registers: S..S+5 = year, month, day, hour,
 * minute, second (a two-digit year means 20xx). An invalid date or time stops the PLC with
 * PLC_RUNTIME_OPERAND; a clock that is unavailable or does not accept the write stops it
 * with PLC_RUNTIME_LEGACY_UNSUPPORTED.
 */
void TWR(void) {
  operand_t s;
  u32 y, mo, d, h, mi, se;
  opd_decode(&s, 0);
  if (!PLC_EXP_ENABLED) return;
  y = opd_get(&s, 0, 0); mo = opd_get(&s, 1, 0); d = opd_get(&s, 2, 0);
  h = opd_get(&s, 3, 0); mi = opd_get(&s, 4, 0); se = opd_get(&s, 5, 0);
  if (y < 100U) y += 2000U;
  if (plc_exp_last_error) return;
  if (y < 2000U || y > 2099U || mo < 1U || mo > 12U || d < 1U || d > 31U || h > 23U || mi > 59U || se > 59U) {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
    return;
  }
  switch (RTC_Set((u16)y, (u8)mo, (u8)d, (u8)h, (u8)mi, (u8)se)) {
    case 0: break;                                /* written */
    case 1: plc_exp_fault(PLC_RUNTIME_OPERAND); break;           /* date outside the RTC range */
    default: plc_exp_fault(PLC_RUNTIME_LEGACY_UNSUPPORTED); break; /* RTC unavailable or write timed out */
  }
}
/**
 * HOUR S D1 D2: hour meter while the rung is ON. D1 counts hours (saturating at 32767),
 * D1+1 counts seconds (0..3599) and the bit D2 is ON once the hours reach S. The meter
 * integrates the whole seconds that elapsed between scans; if the seconds counter plus the
 * elapsed time reaches two hours, the seconds are clamped to 3599 instead of looping.
 */
void HOUR(void) {
  operand_t s, d, o;
  u32 limit, hrs, sec;
  opd_decode(&s, 0); opd_decode(&d, 0); opd_decode(&o, 0);
  if (!PLC_EXP_ENABLED) return;
  limit = opd_get(&s, 0, 0); hrs = opd_get(&d, 0, 0); sec = opd_get(&d, 1, 0) + plc_exp_scan_seconds;
  if (sec >= 3600U * 2U) sec = 3599U;            /* very long scan gap: clamp instead of looping */
  while (sec >= 3600U) { sec -= 3600U; if (hrs < 32767U) ++hrs; }
  opd_put(&d, 0, hrs, 0); opd_put(&d, 1, sec, 0);
  opd_bit_put(&o, 0, hrs >= limit);
}

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_time_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
