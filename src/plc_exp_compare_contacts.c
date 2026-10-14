/**
 * @file plc_exp_compare_contacts.c
 * @brief Comparison contacts: LD/AND/OR with =, >, <, <>, <=, >= in 16 and 32-bit form.
 *
 * A comparison contact is a contact whose state is the result of comparing two operands
 * (signed values, 16 or 32 bit). It combines with the accumulator exactly like the plain
 * contacts of the same family: LD starts a new logic block (OFF while master control or an
 * inactive step disables the rung), AND is a series connection and OR a parallel one.
 * The 36 handlers are thin wrappers around three helpers; the D-prefixed names are the
 * 32-bit forms.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Comparison contacts (LD/AND/OR with = > < <> <= >=, 16 and 32 bit)      */
/* ====================================================================== */

/**
 * Decode the two operands of a comparison contact and compare them as signed numbers.
 * Both operands are always consumed, whatever the rung state.
 *
 * @param w   0 for 16-bit operands, 1 for 32-bit operands.
 * @param op  0 =, 1 >, 2 <, 3 <>, 4 <=, 5 >=.
 * @return the comparison result; 0 after an operand fault.
 */
static u8 cmp_test(u8 w, u8 op) {
  operand_t a, b;
  s32 x, y;
  opd_decode(&a, w); opd_decode(&b, w);
  if (plc_exp_last_error) return 0;
  x = opd_gets(&a, 0, w); y = opd_gets(&b, 0, w);
  switch (op) {
    case 0: return x == y;
    case 1: return x > y;
    case 2: return x < y;
    case 3: return x != y;
    case 4: return x <= y;
    default: return x >= y;
  }
}

/** LD form: start a logic block with the comparison result (OFF while the rung is disabled). */
static void ld_cmp(u8 w, u8 op) {
  u8 r = cmp_test(w, op);
  PLC_ACC_BIT <<= 1;
  if (r && plc_exp_rung_enabled()) PLC_ACC_BIT |= 0x01;
}

/** AND form: comparison contact in series. */
static void and_cmp(u8 w, u8 op) { plc_exp_acc_and(cmp_test(w, op)); }

/** OR form: comparison contact in parallel. */
static void or_cmp(u8 w, u8 op)  { plc_exp_acc_or(cmp_test(w, op)); }

/* LD forms: LD= LD> LD< LD<> LD<= LD>= (16 bit) and the D-prefixed 32-bit versions. */
void LD_EQU(void)   { ld_cmp(0, 0); }
void DLD_EQU(void)  { ld_cmp(1, 0); }
void LD_GTR(void)   { ld_cmp(0, 1); }
void DLD_GTR(void)  { ld_cmp(1, 1); }
void LD_LSS(void)   { ld_cmp(0, 2); }
void DLD_LSS(void)  { ld_cmp(1, 2); }
void LD_NEQ(void)   { ld_cmp(0, 3); }
void DLD_NEQ(void)  { ld_cmp(1, 3); }
void LD_LEQ(void)   { ld_cmp(0, 4); }
void DLD_LEQ(void)  { ld_cmp(1, 4); }
void LD_GEQ(void)   { ld_cmp(0, 5); }
void DLD_GEQ(void)  { ld_cmp(1, 5); }

/* AND forms: AND= AND> AND< AND<> AND<= AND>= and their 32-bit versions. */
void AND_EQU(void)  { and_cmp(0, 0); }
void DAND_EQU(void) { and_cmp(1, 0); }
void AND_GTR(void)  { and_cmp(0, 1); }
void DAND_GTR(void) { and_cmp(1, 1); }
void AND_LSS(void)  { and_cmp(0, 2); }
void DAND_LSS(void) { and_cmp(1, 2); }
void AND_NEQ(void)  { and_cmp(0, 3); }
void DAND_NEQ(void) { and_cmp(1, 3); }
void AND_LEQ(void)  { and_cmp(0, 4); }
void DAND_LEQ(void) { and_cmp(1, 4); }
void AND_GEQ(void)  { and_cmp(0, 5); }
void DAND_GEQ(void) { and_cmp(1, 5); }

/* OR forms: OR= OR> OR< OR<> OR<= OR>= and their 32-bit versions. */
void OR_EQU(void)   { or_cmp(0, 0); }
void DOR_EQU(void)  { or_cmp(1, 0); }
void OR_GTR(void)   { or_cmp(0, 1); }
void DOR_GTR(void)  { or_cmp(1, 1); }
void OR_LSS(void)   { or_cmp(0, 2); }
void DOR_LSS(void)  { or_cmp(1, 2); }
void OR_NEQ(void)   { or_cmp(0, 3); }
void DOR_NEQ(void)  { or_cmp(1, 3); }
void OR_LEQ(void)   { or_cmp(0, 4); }
void DOR_LEQ(void)  { or_cmp(1, 4); }
void OR_GEQ(void)   { or_cmp(0, 5); }
void DOR_GEQ(void)  { or_cmp(1, 5); }

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_compare_contacts_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
