/**
 * @file plc_exp_timers.c
 * @brief OUT Tn / OUT Cn, RST Tn/Cn and the timer/counter reset helpers.
 *
 * Timers T0..T255 tick in the scan adapter (plc_exp_scan.c); counters C0..C199 are 16-bit
 * up counters, C200..C234 32-bit up/down counters. Presets are K constants or D registers
 * addressed by an even byte offset (the convention shared with the minimal profile).
 *
 * Per-device state in the PLC image (offsets from plc_memory_map.h):
 *   - timer:   current value (Addr_TVal), preset (Addr_TMax), enable bit TE (rung state,
 *              Addr_TE) and contact TO (Addr_TO, ON when value >= preset);
 *   - counter: current value (Addr_CVal for C0..C199, Addr_CHVal for C200..), edge memory
 *              CE (previous rung state, Addr_CE) and contact CO (Addr_CO).
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/**
 * Reset timer @p n: value, sub-tick remainder, contact (TO) and enable (TE) are cleared.
 * Used by RST Tn and by OUT Tn when its rung drops (non-retentive timers).
 */
void plc_exp_timer_reset(u32 n) {
  PLC_16BIT(Addr_TVal + n * 2U) = 0; plc_exp_timer_fraction[n] = 0;
  plc_exp_put_bit(Addr_TO * 8U + n, 0); plc_exp_put_bit(Addr_TE * 8U + n, 0);
}

/**
 * Reset counter @p n: value (16-bit for C0..C199, 32-bit above), contact (CO) and edge
 * memory (CE) are cleared.
 */
void plc_exp_counter_reset(u32 n) {
  if (n < 200U) PLC_16BIT(Addr_CVal + n * 2U) = 0;
  else PLC_32BIT(Addr_CHVal + (n - 200U) * 4U) = 0;
  plc_exp_put_bit(Addr_CO * 8U + n, 0); plc_exp_put_bit(Addr_CE * 8U + n, 0);
}

/**
 * RST Tn / RST Cn: reset a timer or counter while the rung is ON.
 * The opcode word 0x000C is followed by one operand word, 0x86nn for timer Tn or 0x8Enn
 * for counter Cn. Any other operand word stops the PLC with PLC_RUNTIME_OPERAND.
 */
void RST_TC(void) {
  u16 w = plc_exp_next_word();
  u32 n = w & 0xFFU;
  if (plc_exp_last_error) return;
  if ((w >> 8) != 0x86U && (w >> 8) != 0x8EU) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (!(PLC_ACC_BIT & 1U)) return;
  if ((w >> 8) == 0x86U) plc_exp_timer_reset(n); else plc_exp_counter_reset(n);
}


/* ====================================================================== */
/* OUT Tn / OUT Cn. Presets: K (80LL 80HH) or D as an even BYTE offset      */
/* (86LL 86HH / 86LL 88HH), the convention shared with the minimal profile. */
/* ====================================================================== */

/**
 * Fetch the preset operand that follows OUT Tn / OUT Cn.
 *
 * Two forms are accepted: a K constant (80LL 80HH, a second item for a 32-bit preset) or a
 * D register given as an even byte offset (86LL 86HH for D0..D999, 86LL 88HH for D1000..).
 * A 32-bit preset reads the register pair Dn, Dn+1 and skips the second item. The words
 * are always consumed, so a false rung skips the same number of words as a true one.
 *
 * @param wide  non-zero for a 32-bit preset (32-bit counters C200..C234).
 * @return the preset; 0 after a PLC_RUNTIME_OPERAND fault for a malformed operand.
 */
static u32 preset_fetch(u8 wide) {
  u16 a = plc_exp_next_word(), b = plc_exp_next_word(), c, d;
  u32 n, v;
  if (plc_exp_last_error) return 0;
  if ((a >> 8) == 0x80U && (b >> 8) == 0x80U) {
    v = (a & 0xFFU) | ((u32)(b & 0xFFU) << 8);
    if (wide) {
      c = plc_exp_next_word(); d = plc_exp_next_word();
      if ((c >> 8) != 0x80U || (d >> 8) != 0x80U) goto bad;
      v |= ((c & 0xFFU) | ((u32)(d & 0xFFU) << 8)) << 16;
    }
    return v;
  }
  if ((a >> 8) == 0x86U && ((b >> 8) == 0x86U || (b >> 8) == 0x88U)) {
    n = (a & 0xFFU) | ((u32)(b & 0xFFU) << 8);
    if ((n & 1U) || n / 2U >= 1000U) goto bad;
    n = n / 2U + ((b >> 8) == 0x88U ? 1000U : 0U);
    if (!wide) return PLC_16BIT(Addr_D0000 + n * 2U);
    if (n + 1U >= MAXDXXX) goto bad;
    v = (u32)PLC_16BIT(Addr_D0000 + n * 2U) | ((u32)PLC_16BIT(Addr_D0000 + n * 2U + 2U) << 16);
    plc_exp_next_word(); plc_exp_next_word();                            /* second item of the 32-bit operand */
    return v;
  }
bad:
  plc_exp_fault(PLC_RUNTIME_OPERAND);
  return 0;
}

/**
 * OUT Tn: timer coil (word 0x06nn, then the preset operand).
 *
 * The preset (0..32767) is stored for the scan adapter, which advances the value of every
 * enabled timer. The enable bit TE follows the rung. When the rung is OFF a non-retentive
 * timer (T0..T245) is reset; the retentive timers T246..T255 keep value and contact. While
 * the rung is ON the contact TO is ON once the value has reached the preset. A preset above
 * 32767 stops the PLC with PLC_RUNTIME_OPERAND.
 */
void FUNC_TIMER(void) {
  u32 n = CMD_CMD & 0xFFU, preset;
  u8 acc = PLC_ACC_BIT & 1U;
  preset = preset_fetch(0);
  if (plc_exp_last_error) return;
  if (preset > 32767U) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  PLC_16BIT(Addr_TMax + n * 2U) = (u16)preset;
  plc_exp_put_bit(Addr_TE * 8U + n, acc);
  if (!acc) { if (n < 246U) plc_exp_timer_reset(n); }    /* T246..255 are retentive */
  else plc_exp_put_bit(Addr_TO * 8U + n, PLC_16BIT(Addr_TVal + n * 2U) >= preset);
}

/**
 * OUT Cn: counter coil (word 0x0Enn, then the preset operand).
 *
 * C0..199: 16-bit up counters. C200..234: 32-bit up/down counters, direction
 * relay M8200+(n-200) (ON = down). C235..255 need the hardware counter unit and stop the
 * PLC with PLC_RUNTIME_LEGACY_UNSUPPORTED.
 *
 * The counter counts once per OFF-to-ON transition of the rung; the previous rung state is
 * kept in the CE bit. A 16-bit counter stops at its preset (0..32767). The contact CO is ON
 * while the value is at or above the preset (a signed comparison for 32-bit counters).
 * Counters are reset with RST Cn (RST_TC), not by the rung going OFF.
 */
void FUNC_COUNTER(void) {
  u32 n = CMD_CMD & 0xFFU, preset;
  u8 acc = PLC_ACC_BIT & 1U;
  if (n >= 235U) { plc_exp_fault(PLC_RUNTIME_LEGACY_UNSUPPORTED); return; }
  preset = preset_fetch(n >= 200U);
  if (plc_exp_last_error) return;
  if (n < 200U) {
    u16 v;
    if (preset > 32767U) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
    v = PLC_16BIT(Addr_CVal + n * 2U);
    if (acc && !PLC_BIT_TEST(Addr_CE * 8U + n) && v < preset) PLC_16BIT(Addr_CVal + n * 2U) = ++v;
    plc_exp_put_bit(Addr_CE * 8U + n, acc);
    plc_exp_put_bit(Addr_CO * 8U + n, v >= preset);
  } else {
    u32 raw = PLC_32BIT(Addr_CHVal + (n - 200U) * 4U);
    if (acc && !PLC_BIT_TEST(Addr_CE * 8U + n)) {
      raw += PLC_BIT_TEST(Addr_M8 * 8U + n) ? 0xFFFFFFFFU : 1U;
      PLC_32BIT(Addr_CHVal + (n - 200U) * 4U) = raw;
    }
    plc_exp_put_bit(Addr_CE * 8U + n, acc);
    plc_exp_put_bit(Addr_CO * 8U + n, (s32)raw >= (s32)preset);
  }
}

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_timers_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
