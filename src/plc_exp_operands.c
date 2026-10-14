/**
 * @file plc_exp_operands.c
 * @brief Bit/word device access and the operand decoder of the experimental interpreter.
 *
 * All PLC image accesses made by instructions go through here: bit areas and their
 * write permissions, K/H/D/T/C/KnM operands in 16 and 32-bit form, V/Z index registers
 * and range-checked reads/writes. An operand is decoded and validated completely before
 * the instruction looks at its rung condition, so a false rung still consumes exactly
 * the same program words as a true one.
 *
 * The module has four layers, from the bottom up:
 *   1. raw bit access by linear bit address (PLC_BIT_TEST/ON/OFF),
 *   2. the bit device map with its read/write rules (plc_exp_bit_area, plc_exp_bit_ok),
 *   3. the program-word fetchers (plc_exp_next_word, plc_exp_bit_operand),
 *   4. the operand model (opd_decode and the opd_get/opd_put accessors).
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include <string.h>
#include "plc_exp_internal.h"


/* ====================================================================== */
/* Raw bit access                                                          */
/* ====================================================================== */

/**
 * Read one bit of the PLC image.
 * @param addr  linear bit address (byte offset * 8 + bit index).
 * @return 0 or 1. An address beyond the image stops the PLC with PLC_RUNTIME_BOUNDS
 *         and reads as 0.
 */
u32 PLC_BIT_TEST(u32 addr) {
  if (addr >= PLC_RAM_SIZE * 8U) { plc_exp_fault(PLC_RUNTIME_BOUNDS); return 0; }
  return (PLC_8BIT(addr / 8U) >> (addr & 7U)) & 1U;
}

/** Set one bit of the PLC image (no write-permission check, see plc_exp_bit_write()). */
void PLC_BIT_ON(u32 addr) { plc_exp_bit_put(addr, 1); }

/** Clear one bit of the PLC image (no write-permission check, see plc_exp_bit_write()). */
void PLC_BIT_OFF(u32 addr) { plc_exp_bit_put(addr, 0); }

/**
 * Drive a bit to @p value without a permission check.
 * Used for bits the runtime owns (step bits, timer and counter contacts, pulse results).
 */
void plc_exp_put_bit(u32 bit, u8 value) {
  if (value) PLC_BIT_ON(bit); else PLC_BIT_OFF(bit);
}

/** Drive a status/flag relay of the M80xx family (zero, borrow and carry flags, M8029, ...). */
void plc_exp_set_flag(u32 bit, u8 value) { plc_exp_put_bit(bit, value); }


/* ====================================================================== */
/* Bit device map (same linear map and write rules as the minimal profile) */
/* ====================================================================== */

/**
 * Locate the bit area that contains linear bit address @p b.
 *
 * The areas are, in image order: M0..M3071 (rw), Y0..Y511 (rw), M8000..M8255 (see
 * m8_user()), C0..C255 contacts (ro), T0..T255 contacts (ro), X0..X511 (ro) and
 * S0..S4095 (rw). Everything else in the image (word devices, gaps) is not a bit device.
 *
 * @param b   linear bit address.
 * @param hi  receives the first bit address after the area.
 * @param rw  receives 1 if programs may write the area (the M8000 area is refined further
 *            by plc_exp_bit_ok()).
 * @return 1 if @p b is a bit device; 0 otherwise, in which case @p hi and @p rw are left untouched.
 */
u8 plc_exp_bit_area(u32 b, u32 *hi, u8 *rw) {
  u32 h; u8 w = 0;
  if (b < Addr_Y * 8U) { h = Addr_Y * 8U; w = 1; }                          /* M0..3071 */
  else if (b < Addr_M8 * 8U) { h = Addr_M8 * 8U; w = 1; }                   /* Y0..511 */
  else if (b < Addr_CO * 8U) h = Addr_CO * 8U;                              /* M8000..8255, see m8_user() */
  else if (b < Addr_CO * 8U + 256U) h = Addr_CO * 8U + 256U;                /* C0..255 contact */
  else if (b >= Addr_TO * 8U && b < Addr_TO * 8U + 256U) h = Addr_TO * 8U + 256U; /* T0..255 contact */
  else if (b >= Addr_X * 8U && b < Addr_X * 8U + 512U) h = Addr_X * 8U + 512U;   /* X0..511 */
  else if (b >= Addr_S * 8U && b < Addr_S * 8U + 4096U) { h = Addr_S * 8U + 4096U; w = 1; } /* S */
  else return 0;
  *hi = h; *rw = w;
  return 1;
}

/**
 * Special relays a program may drive: mode and user flags M8030..M8059 and
 * M8100..M8255 (M8049 annunciators, M8161/M8165/M8168 instruction modes, M8200.. counter
 * direction). M8000..M8029 and M8060..M8099 are status, clock, arithmetic-flag and error
 * relays owned by the runtime; programs can read but not drive them.
 *
 * @param b  linear bit address inside the M8000..M8255 area.
 */
static u8 m8_user(u32 b) {
  u32 n = b - Addr_M8 * 8U;
  return (n >= 30U && n < 60U) || n >= 100U;
}

/**
 * May a program access bit @p b?
 * @param b      linear bit address.
 * @param write  0 to test a read, non-zero to test a write.
 * @return 1 if the access is allowed. Reads are allowed in every bit area; writes only in
 *         the writable areas and, inside M8000..M8255, only for the user relays.
 */
u8 plc_exp_bit_ok(u32 b, u8 write) {
  u32 h; u8 w;
  if (!plc_exp_bit_area(b, &h, &w)) return 0;
  if (!write) return 1;
  return (b >= Addr_M8 * 8U && b < Addr_CO * 8U) ? m8_user(b) : w;
}

/** Are all @p count bits starting at @p b accessible (see plc_exp_bit_ok())? */
static u8 span_ok(u32 b, u32 count, u8 write) {
  u32 k;
  for (k = 0; k < count; ++k) if (!plc_exp_bit_ok(b + k, write)) return 0;
  return 1;
}

/**
 * Checked bit read for program operands.
 * An address that is not a bit device stops the PLC with PLC_RUNTIME_OPERAND and reads as 0.
 */
u8 plc_exp_bit_read(u32 b) {
  if (!plc_exp_bit_ok(b, 0)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return 0; }
  return (u8)PLC_BIT_TEST(b);
}

/**
 * Checked bit write for program operands.
 * A read-only or non-existent bit stops the PLC with PLC_RUNTIME_OPERAND; nothing is written.
 */
void plc_exp_bit_write(u32 b, u8 v) {
  if (!plc_exp_bit_ok(b, 1)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  plc_exp_put_bit(b, v);
}

/** Is bit @p b a step relay (S0..S4095)? */
u8 plc_exp_is_step_bit(u32 b) { return b >= Addr_S * 8U && b < Addr_S * 8U + 4096U; }


/* ====================================================================== */
/* Program word fetch                                                      */
/* ====================================================================== */

/**
 * Fetch the program word at the program counter and advance it.
 * A fetch outside the program latches a fault (see plc_exp_peek_word()) and returns 0.
 */
u16 plc_exp_next_word(void) {
  u16 w = plc_exp_peek_word();
  PLC_PC++;
  return w;
}

/**
 * Fetch a marked bit operand: 0x8000 | linear bit address.
 * Bits 15..14 must be 10; the 14 address bits (including bit 12) are all kept.
 *
 * @param write  non-zero if the instruction writes the bit (the write rules apply).
 * @return the linear bit address; 0 after a PLC_RUNTIME_OPERAND fault when the word is not
 *         a marked operand or the bit does not exist / may not be written.
 */
u32 plc_exp_bit_operand(u8 write) {
  u16 w = plc_exp_next_word();
  if ((w & 0xC000U) != 0x8000U || !plc_exp_bit_ok(w & 0x3FFFU, write)) {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
    return 0;
  }
  return w & 0x3FFFU;
}


/* ====================================================================== */
/* Operand model                                                           */
/*                                                                         */
/* An operand ("item") is two program words:                               */
/*                                                                         */
/*     word 1 = (TYPE << 8)   | NUM_LOW        e.g. 0x80LL                 */
/*     word 2 = (DEVICE << 8) | NUM_HIGH       e.g. 0x86HH                 */
/*                                                                         */
/* NUM = (NUM_HIGH << 8) | NUM_LOW is the device number or the constant.   */
/*                                                                         */
/*   TYPE bit 7       always 1; bits 6 and 0 must be 0                     */
/*   TYPE bits 5..4   index mode: 0 none, 1 V-indexed, 2 Z-indexed         */
/*   TYPE bits 3..1   0/1 K or H constant, 2 bit device or KnX group,      */
/*                    3 word device, 4 P/I pointer                         */
/*                                                                         */
/*   DEVICE byte      constant: 0x80, or 0x80..0x87 = index register no.   */
/*                    bit/group: 0x80 = single bit, 0x82..0x90 (even) =    */
/*                      K1..K8; indexed 0x82..0x91 = K1..K8 (see below)    */
/*                    word: 0x80 D8000.., 0x82 T, 0x84 C, 0x86 D0..,       */
/*                      0x88 D1000..; indexed 0x82..0x89 = T, C, D, D1000  */
/*                    pointer: 0x80                                        */
/*                                                                         */
/* For indexed bit/group/word operands the index register number is split: */
/* bit 2 is bit 0 of (DEVICE - 0x82), bits 1..0 are NUM bits 15..14, so    */
/* only NUM bits 13..0 address the device.                                 */
/*                                                                         */
/* In a 32-bit ("D") instruction every operand occupies two items. For a  */
/* constant the second item (words 3 and 4, bit 15 set in both) carries    */
/* bits 31..16: the low byte of word 3 is bits 23..16 and the low byte of  */
/* word 4 is bits 31..24. For a device or group the second item is         */
/* consumed and ignored. Every operand is decoded, range checked and       */
/* consumed before an instruction acts, so a false rung still skips the    */
/* right number of words.                                                  */
/* ====================================================================== */


/** D-numbers (minus 8000) of the index registers: row 0 = V0..V7, row 1 = Z0..Z7. */
static const u8 VZREG[2][8] = {
  {29, 183, 185, 187, 189, 191, 193, 195},
  {28, 182, 184, 186, 188, 190, 192, 194}
};

/**
 * Read an index register.
 * @param z  0 selects V, 1 selects Z.
 * @param r  register number 0..7 (only the low three bits are used).
 * @return the raw 16-bit value, zero extended.
 */
u32 plc_exp_index_reg(u8 z, u8 r) { return PLC_16BIT(Addr_D8000 + (u32)VZREG[z][r & 7U] * 2U); }

/** Write an index register (@p z and @p r as for plc_exp_index_reg()). */
void plc_exp_index_reg_write(u8 z, u8 r, u16 value) { PLC_16BIT(Addr_D8000 + (u32)VZREG[z][r & 7U] * 2U) = value; }

/**
 * Index offset added to an indexed operand.
 * @param z  0 = V register, 1 = Z register.
 * @param r  register number.
 * @param w  non-zero for a 32-bit operand.
 * @return the sign-extended 16-bit register value; for a 32-bit Z-indexed operand the
 *         32-bit pair V:Z (V in the high half, Z in the low half).
 */
static u32 idxval(u8 z, u8 r, u8 w) {
  if (w && z) return (plc_exp_index_reg(0, r) << 16) | plc_exp_index_reg(1, r);
  return (u32)(s32)(s16)plc_exp_index_reg(z, r);
}

/** The 32-bit pair V:Z of index register @p n (0..3), V in the high half; 0 if @p n >= 4. */
u32 DZ0_Z3(u16 n) { return n < 4U ? (plc_exp_index_reg(0, (u8)n) << 16) | plc_exp_index_reg(1, (u8)n) : 0U; }

/** The 32-bit pair V:Z of index register @p n + 4 (@p n = 0..3), V in the high half; 0 if @p n >= 4. */
u32 DZ4_Z7(u16 n) { return n < 4U ? (plc_exp_index_reg(0, (u8)(n + 4U)) << 16) | plc_exp_index_reg(1, (u8)(n + 4U)) : 0U; }

/**
 * Decode the next operand from the program and validate its encoding.
 *
 * Always consumes the whole operand (two words, or four words when @p w is 1) so that the
 * program counter stays correct on a false rung. A malformed operand stops the PLC with
 * PLC_RUNTIME_OPERAND and leaves @p o cleared (kind OPD_NONE).
 *
 * @param o  receives the decoded operand.
 * @param w  0: 16-bit operand;
 *           1: 32-bit operand (two items, bit/word groups up to K8);
 *           2: 16-bit item whose device is accessed as 32-bit (MUL/DIV results).
 */
void opd_decode(operand_t *o, u8 w) {
  u16 w1 = plc_exp_next_word(), w2 = plc_exp_next_word(), w3 = 0, w4 = 0;
  u8 hi = (u8)(w1 >> 8), lo = (u8)(w2 >> 8), z, r = 0, dig, kd, two = (w == 1U), wide = (w != 0U);
  u32 num = ((u32)(w2 & 0xFFU) << 8) | (w1 & 0xFFU), add = 0;
  memset(o, 0, sizeof *o);
  if (plc_exp_last_error) return;
  if (!(hi & 0x80U) || (hi & 0x41U) || ((hi >> 4) & 3U) == 3U) goto bad;
  z = (u8)((hi >> 4) & 3U);                     /* 0 plain, 1 V indexed, 2 Z indexed */
  if (two) { w3 = plc_exp_next_word(); w4 = plc_exp_next_word(); if (plc_exp_last_error) return; }
  switch (hi & 0x0EU) {
    case 0x00: case 0x02:                       /* K / H constant */
      if (z ? (lo < 0x80U || lo > 0x87U) : lo != 0x80U) goto bad;
      if (two && (!(w3 & 0x8000U) || !(w4 & 0x8000U))) goto bad;
      o->kind = OPD_CONST;
      o->value = num | (two ? (((w3 & 0xFFU) | ((u32)(w4 & 0xFFU) << 8)) << 16) : 0U);
      if (z) o->value += idxval((u8)(z - 1U), lo & 7U, wide);
      if (!two) o->value &= 0xFFFFU;
      return;
    case 0x04:                                  /* bit device or K1..K8 group */
      if (!z && lo == 0x80U) { o->kind = OPD_BIT; o->index = num; return; }
      if (!z) {
        if (lo < 0x82U || lo > 0x90U || (lo & 1U)) goto bad;
        dig = (u8)((lo - 0x80U) >> 1);
      } else {
        if (lo < 0x82U || lo > 0x91U) goto bad;
        dig = (u8)(((lo - 0x82U) >> 1) + 1U);
        r = (u8)((((lo - 0x82U) & 1U) << 2) | (num >> 14));
        num &= 0x3FFFU;
        add = idxval((u8)(z - 1U), r, wide);
      }
      if (dig > (wide ? 8U : 4U)) goto bad;
      o->kind = OPD_GROUP; o->digits = dig; o->index = num + add;
      break;
    case 0x06:                                  /* word device */
      if (!z) {
        switch (lo) {
          case 0x80: o->dev = DEV_D8000; break;
          case 0x82: o->dev = DEV_T; break;
          case 0x84: o->dev = DEV_C; break;
          case 0x86: o->dev = DEV_D; break;
          case 0x88: o->dev = DEV_D; add = 1000U; break;
          default: goto bad;
        }
      } else {
        if (lo < 0x82U || lo > 0x89U) goto bad;
        kd = (u8)((lo - 0x82U) >> 1);
        r = (u8)((((lo - 0x82U) & 1U) << 2) | (num >> 14));
        num &= 0x3FFFU;
        o->dev = kd == 0U ? DEV_T : kd == 1U ? DEV_C : DEV_D;
        add = (kd == 3U ? 1000U : 0U) + idxval((u8)(z - 1U), r, wide);
      }
      o->kind = OPD_WORD; o->index = num + add;
      break;
    case 0x08:                                  /* P/I pointer */
      if (z || lo != 0x80U) goto bad;
      o->kind = OPD_PTR; o->value = num;
      return;
    default: goto bad;
  }
  return;
bad:
  plc_exp_fault(PLC_RUNTIME_OPERAND);
  memset(o, 0, sizeof *o);
}

/**
 * Locate a word-device element in the PLC image.
 *
 * Counters are special: C0..C199 are 16-bit registers and C200..C255 are 32-bit registers,
 * so a counter element is one counter whatever the access width, and the width must match
 * the counter's kind. For the other devices a 32-bit element spans two consecutive registers.
 *
 * @param o    word operand.
 * @param e    element number counted from the operand's first register.
 * @param w    0 = 16-bit element, 1 = 32-bit element.
 * @param off  receives the byte offset inside the PLC image.
 * @return 1 if the element exists, 0 if it lies outside the device.
 */
static u8 word_off(const operand_t *o, u32 e, u8 w, u32 *off) {
  u32 n = o->index + e * (w ? 2U : 1U);
  switch (o->dev) {
    case DEV_D8000: if (n + w < 256U) { *off = Addr_D8000 + n * 2U; return 1; } break;
    case DEV_T: if (n + w < 256U) { *off = Addr_TVal + n * 2U; return 1; } break;
    case DEV_D: if (n + w < MAXDXXX) { *off = Addr_D0000 + n * 2U; return 1; } break;
    default:                                    /* C0..199 are 16-bit, C200..255 are 32-bit */
      n = o->index + e;
      if (!w) { if (n < 200U) { *off = Addr_CVal + n * 2U; return 1; } }
      else if (n >= 200U && n < 256U) { *off = Addr_CHVal + (n - 200U) * 4U; return 1; }
      break;
  }
  return 0;
}

/** Gather @p n hexadecimal digits (4 * n bits) starting at bit @p b into an integer, LSB first. */
static u32 grp_get(u32 b, u8 n) {
  u32 v = 0, k;
  for (k = 0; k < (u32)n * 4U; ++k) v |= (u32)PLC_BIT_TEST(b + k) << k;
  return v;
}

/** Scatter the low 4 * @p n bits of @p v over the bits starting at @p b (no permission check). */
static void grp_put(u32 b, u8 n, u32 v) {
  u32 k;
  for (k = 0; k < (u32)n * 4U; ++k) plc_exp_put_bit(b + k, (u8)((v >> k) & 1U));
}

/**
 * Read element @p e of an operand, zero extended.
 *
 * Constants repeat for every @p e. Word devices are read as 16 bits (@p w = 0) or 32 bits
 * (@p w = 1, low register first). Bit groups yield 4 * digits bits; element @p e starts
 * 4 * digits bits after element @p e - 1 and must lie inside one bit area.
 *
 * An operand that cannot be read this way (wrong kind, outside the device) stops the PLC
 * with PLC_RUNTIME_OPERAND and yields 0.
 */
u32 opd_get(const operand_t *o, u32 e, u8 w) {
  u32 off, b, hi; u8 rw;
  switch (o->kind) {
    case OPD_CONST: return w ? o->value : (o->value & 0xFFFFU);
    case OPD_WORD:
      if (!word_off(o, e, w, &off)) break;
      if (o->dev == DEV_C && w) return PLC_32BIT(off);
      return w ? ((u32)PLC_16BIT(off) | ((u32)PLC_16BIT(off + 2U) << 16)) : PLC_16BIT(off);
    case OPD_GROUP:
      b = o->index + e * o->digits * 4U;
      if (plc_exp_bit_area(b, &hi, &rw) && b + (u32)o->digits * 4U <= hi) return grp_get(b, o->digits);
      break;
    default: break;
  }
  plc_exp_fault(PLC_RUNTIME_OPERAND);
  return 0;
}

/** Like opd_get() but sign extended (from 16 bits when @p w = 0). */
s32 opd_gets(const operand_t *o, u32 e, u8 w) {
  u32 v = opd_get(o, e, w);
  return w ? (s32)v : (s32)(s16)(u16)v;
}

/**
 * Write element @p e of an operand.
 *
 * Only word devices and bit groups can be written; a group must lie inside one bit area
 * and every bit must be writable. Anything else (a constant, a read-only bit, an element
 * outside the device) stops the PLC with PLC_RUNTIME_OPERAND and writes nothing.
 *
 * @param v  value to store (the low 16 bits when @p w = 0).
 * @param w  0 = 16-bit element, 1 = 32-bit element (low register first).
 */
void opd_put(const operand_t *o, u32 e, u32 v, u8 w) {
  u32 off, b, hi; u8 rw;
  switch (o->kind) {
    case OPD_WORD:
      if (!word_off(o, e, w, &off)) break;
      if (o->dev == DEV_C && w) { PLC_32BIT(off) = v; return; }
      PLC_16BIT(off) = (u16)v;
      if (w) PLC_16BIT(off + 2U) = (u16)(v >> 16);
      return;
    case OPD_GROUP:
      b = o->index + e * o->digits * 4U;
      if (plc_exp_bit_area(b, &hi, &rw) && b + (u32)o->digits * 4U <= hi && span_ok(b, (u32)o->digits * 4U, 1)) { grp_put(b, o->digits, v); return; }
      break;
    default: break;
  }
  plc_exp_fault(PLC_RUNTIME_OPERAND);
}

/** Read bit @p e of a bit operand (device bit index + e); a non-bit operand is a fault. */
u8 opd_bit_get(const operand_t *o, u32 e) {
  if (o->kind != OPD_BIT) { plc_exp_fault(PLC_RUNTIME_OPERAND); return 0; }
  return plc_exp_bit_read(o->index + e);
}

/** Write bit @p e of a bit operand (device bit index + e); a non-bit operand is a fault. */
void opd_bit_put(const operand_t *o, u32 e, u8 v) {
  if (o->kind != OPD_BIT) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  plc_exp_bit_write(o->index + e, v);
}

/**
 * Read an @p n-bit number (the source of DECO).
 * A bit device is collected bit by bit (at most 32 bits, bit 0 = first device bit);
 * any other operand is read as a 16-bit word and masked to @p n bits.
 */
u32 opd_bits(const operand_t *o, u32 n) {
  u32 v = 0, k;
  if (o->kind != OPD_BIT) return opd_get(o, 0, 0) & ((n >= 32U) ? 0xFFFFFFFFU : ((1UL << n) - 1U));
  for (k = 0; k < n && k < 32U; ++k) v |= (u32)opd_bit_get(o, k) << k;
  return v;
}

/**
 * Count operand: a constant or a word value, validated to the range lo..max.
 * @return the count, or @p lo after a PLC_RUNTIME_OPERAND fault when it is out of range.
 */
u32 opd_count(const operand_t *o, u32 lo, u32 max) {
  s32 v = opd_gets(o, 0, 0);
  if (v < (s32)lo || v > (s32)max) { plc_exp_fault(PLC_RUNTIME_OPERAND); return lo; }
  return (u32)v;
}

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_operands_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
