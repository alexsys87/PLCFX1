/**
 * @file plc_exp_logic.c
 * @brief Contacts, coils, SET/RST, PLS/PLF, MC/MCR, STL/RET and the opcode routers.
 *
 * Implements the ladder-logic core: the accumulator/MPS stacks (PLC_ACC_BIT, PLC_MPS_BIT),
 * edge contacts, master control, step-ladder steps and the routers FUNC_0/FUNC_F/FUNC_FF
 * that decode the first program word and dispatch to the instruction handlers.
 *
 * Logic model
 *   - PLC_ACC_BIT is a stack of logic blocks packed into one byte. Bit 0 is the block
 *     being built (the accumulator); every LD/LDI/LDP/LDF/STL pushes a new block by
 *     shifting the byte left, ANB and ORB merge the two top blocks. At most 8 blocks
 *     can be pending: a deeper nesting loses the oldest block.
 *   - PLC_MPS_BIT is the MPS/MRD/MPP stack of saved results (bit 0 = most recent push),
 *     also limited to 8 levels.
 *   - Loading instructions (LD, LDI, LDP, LDF, STL) and parallel contacts (OR family) only
 *     produce an ON result while the rung is enabled, i.e. while no master-control level
 *     is OFF and, inside a step, the active step is ON (plc_exp_rung_enabled()). A series
 *     contact (AND family) can only turn the accumulator OFF, so it needs no gating.
 *     Coils therefore see a false accumulator in a disabled region and are driven OFF.
 *   - One-word contacts and coils carry a 12-bit bit address in the instruction word
 *     (CMD_REG). The extended forms (opcodes 0x01C2..0x01CF for contacts, 0x0002..0x000A
 *     for OUT/SET/RST, PLS/PLF and MC) are followed by a marked word
 *     (0x8000 | linear bit address) and reach the whole bit map.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Edge memory                                                             */
/* ====================================================================== */

/*
 * Every instruction that needs the previous state of its input (pulse "P"
 * execution, PLS/PLF, edge contacts) owns one bit in the pulse bitmap. The bit is
 * addressed by the word offset of the instruction (CMD_ADDR), so it is stable from
 * scan to scan and is cleared whenever the program starts or is replaced.
 */

/** Read the edge-memory bit of the current instruction. */
u32 PLC_PL_BIT_TEST(void) {
  return plc_exp_pulse_test(CMD_ADDR);
}

/** Set the edge-memory bit of the current instruction. */
void PLC_PL_BIT_ON(void) {
  plc_exp_pulse_put(CMD_ADDR, 1);
}

/** Clear the edge-memory bit of the current instruction. */
void PLC_PL_BIT_OFF(void) {
  plc_exp_pulse_put(CMD_ADDR, 0);
}

/**
 * Rising-edge detector of the accumulator for pulse ("P") execution.
 *
 * @return 1 in exactly one scan: the one in which bit 0 of PLC_ACC_BIT turns ON after it
 *         was OFF the previous time the instruction ran. The edge bit is cleared again
 *         once the accumulator is OFF, which re-arms the detector.
 */
u8 PLC_LDP_TEST(void) {
  if ( PLC_PL_BIT_TEST() == 0 ) {
    if ( PLC_ACC_BIT & 0x01 ) {
      PLC_PL_BIT_ON();
      return 1;
    }
    return 0;
  }
  if ( !( PLC_ACC_BIT & 0x01 ) )
    PLC_PL_BIT_OFF();
  return 0;
}

/**
 * Error entry of the original instruction code (codes M8064/M8065 with a detail id).
 * Both arguments are ignored: every error is latched as the "unsupported" fault and
 * stops the PLC, nothing is silently ignored.
 */
void PLC_PROG_ERROR(u16 err,u16 err_id) {
  (void)err; (void)err_id;
  plc_exp_fault(PLC_RUNTIME_LEGACY_UNSUPPORTED);
}


/**
 * Rung gating by master control (MC) and the active step (STL).
 * @return 1 if the current rung may produce an ON result: no master-control level is
 *         OFF and, between STL and RET, the active step bit is ON.
 */
u8 plc_exp_rung_enabled(void) {
  return !plc_exp_mc_off && (!PLC_STL_Status || PLC_BIT_TEST(PLC_STL_Addr));
}

/* ====================================================================== */
/* Basic instructions                                                      */
/* ====================================================================== */

/** Series connection: AND a contact value into the accumulator. */
void plc_exp_acc_and(u8 v) { if (!v) PLC_ACC_BIT &= 0xFE; }

/** Parallel connection: OR a contact value into the accumulator (only while the rung is enabled). */
void plc_exp_acc_or(u8 v) { if (v && plc_exp_rung_enabled()) PLC_ACC_BIT |= 0x01; }

/** Unassigned opcode (the ERR slots of the dispatch tables): stops the PLC. */
void ERR(void) {
  PLC_PROG_ERROR(M8065,02);
}

/** NOP, also the handler of the label words 0xB0nn and of the program word 0x0000. */
void NOP(void) {
}

/**
 * END / FEND: end of the main program, finishes the scan.
 * Open MC, FOR or CALL levels are a circuit error and also stop the PLC.
 */
void END(void) {
  /* END/FEND with open MC, FOR or CALL levels is a circuit error. */
  if (plc_exp_mc_depth || plc_exp_for_depth || plc_exp_call_depth) plc_exp_fault(PLC_RUNTIME_STACK);
  PLC_PRG_STOP = 1;
}

/** LD bit: start a logic block with a normally open contact. */
void LD(void)  { PLC_ACC_BIT <<= 1; if (plc_exp_bit_read(CMD_REG) && plc_exp_rung_enabled()) PLC_ACC_BIT |= 0x01; }

/** LDI bit: start a logic block with a normally closed contact. */
void LDI(void) { PLC_ACC_BIT <<= 1; if (!plc_exp_bit_read(CMD_REG) && plc_exp_rung_enabled()) PLC_ACC_BIT |= 0x01; }

/** AND bit: normally open contact in series. */
void AND(void) { plc_exp_acc_and(plc_exp_bit_read(CMD_REG)); }

/** ANI bit: normally closed contact in series. */
void ANI(void) { plc_exp_acc_and(!plc_exp_bit_read(CMD_REG)); }

/** OR bit: normally open contact in parallel. */
void OR(void)  { plc_exp_acc_or(plc_exp_bit_read(CMD_REG)); }

/** ORI bit: normally closed contact in parallel. */
void ORI(void) { plc_exp_acc_or(!plc_exp_bit_read(CMD_REG)); }

/** Step transition: the active step is reset and the target step is set. */
static void step_to(u32 b) {
  plc_exp_put_bit(PLC_STL_Addr, 0);
  plc_exp_put_bit(b, 1);
}

/**
 * OUT bit: drive a coil with the accumulator.
 * Between STL and RET, OUT on a step relay (S) is a step transition instead: when the
 * rung is ON the active step is reset and the target step is set; when it is OFF nothing
 * changes. A bit that programs may not drive stops the PLC with PLC_RUNTIME_OPERAND.
 */
void OUT(void) {
  u8 acc = PLC_ACC_BIT & 0x01;
  if (!plc_exp_bit_ok(CMD_REG, 1)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (PLC_STL_Status && plc_exp_is_step_bit(CMD_REG)) { if (acc) step_to(CMD_REG); }
  else plc_exp_put_bit(CMD_REG, acc);
}

/**
 * SET bit: latch a coil ON while the rung is ON; it stays ON until RST.
 * Inside a step block, SET on a step relay is a step transition, like OUT.
 */
void BSET(void) {
  if (!(PLC_ACC_BIT & 0x01)) return;
  if (!plc_exp_bit_ok(CMD_REG, 1)) { plc_exp_fault(PLC_RUNTIME_OPERAND); return; }
  if (PLC_STL_Status && plc_exp_is_step_bit(CMD_REG)) step_to(CMD_REG);
  else plc_exp_put_bit(CMD_REG, 1);
}

/** RST bit: clear a coil while the rung is ON; it stays OFF until SET or OUT. */
void RST(void) {
  if (PLC_ACC_BIT & 0x01) plc_exp_bit_write(CMD_REG, 0);
}

/**
 * PLS bit: switch the coil ON for one scan on the rising edge of the rung.
 * The previous rung state is kept in the instruction's edge bit.
 */
void PLS(void) {
  u8 cur = PLC_ACC_BIT & 1U, old = (u8)PLC_PL_BIT_TEST();
  if (cur) PLC_PL_BIT_ON(); else PLC_PL_BIT_OFF();
  plc_exp_bit_write(CMD_REG, cur && !old);
}

/**
 * PLF bit: switch the coil ON for one scan on the falling edge of the rung.
 * The previous rung state is kept in the instruction's edge bit.
 */
void PLF(void) {
  u8 cur = PLC_ACC_BIT & 1U, old = (u8)PLC_PL_BIT_TEST();
  if (cur) PLC_PL_BIT_ON(); else PLC_PL_BIT_OFF();
  plc_exp_bit_write(CMD_REG, old && !cur);
}

/**
 * MC bit: master control coil. Opens a master-control level (nesting depth 8).
 * While the rung is OFF the level is marked in plc_exp_mc_off, so the loading
 * instructions up to the matching MCR yield OFF and their coils are driven OFF; the coil
 * bit itself follows the rung. The N operand of the FX instruction is implied by the
 * nesting order. A ninth level stops the PLC with PLC_RUNTIME_STACK.
 */
void MC(void) {
  u8 on = PLC_ACC_BIT & 1U;
  if (plc_exp_last_error) return;
  if (plc_exp_mc_depth >= 8U) { plc_exp_fault(PLC_RUNTIME_STACK); return; }
  if (!on) plc_exp_mc_off |= (u8)(1U << plc_exp_mc_depth);
  plc_exp_mc_depth++;
  plc_exp_bit_write(CMD_REG, on);
}

/**
 * MCR: close the innermost master-control level.
 * The operand word (0x8000 | N) must carry the marker bit; it is otherwise ignored. An MCR
 * without an open MC, or without the marker, stops the PLC with PLC_RUNTIME_STACK.
 */
void MCR(void) {
  u16 w = plc_exp_next_word();                               /* 0x8000 | N, kept for the programmer's benefit */
  if (plc_exp_last_error) return;
  if ((w & 0x8000U) == 0U || !plc_exp_mc_depth) { plc_exp_fault(PLC_RUNTIME_STACK); return; }
  plc_exp_mc_depth--;
  plc_exp_mc_off &= (u8)~(1U << plc_exp_mc_depth);
}


/**
 * RST Dn (word form): clear a word device to zero while the rung is ON.
 * The operand is a regular two-word item, so indexed forms are allowed. RST of a timer
 * or counter contact and its state is RST_TC instead.
 */
void RST_D(void) {
  operand_t d;
  opd_decode(&d, 0);
  if (plc_exp_last_error) return;
  if (PLC_ACC_BIT & 1U) opd_put(&d, 0, 0, 0);
}

/**
 * STL Sn: step-ladder contact (S0..S1023 in the instruction word, nibble 0xF0nn..0xF3nn).
 * Opens a step block that lasts until RET: the accumulator is loaded with the step state
 * (OFF while master control is OFF), so the coils of the block are driven only while the
 * step is active.
 */
void STL(void) {
  PLC_STL_CMD = 1;
  PLC_STL_Status = 1;
  PLC_STL_Addr = Addr_S * 8U + (CMD_REG & 0x3FFU);
  PLC_ACC_BIT <<= 1;
  if (!plc_exp_mc_off && PLC_BIT_TEST(PLC_STL_Addr)) PLC_ACC_BIT |= 0x01;
}

/** RET: leave the step-ladder block opened by STL. */
void RET(void) {
  PLC_STL_Status = 0;
}

/**
 * Edge detection of a raw contact, shared by LDP/LDF/ANDP/ANDF/ORP/ORF.
 * The contact is sampled whenever the instruction executes, even when the rung is
 * disabled, and its previous value is kept in the instruction's edge bit.
 *
 * @param falling  0 to detect a rising edge, non-zero a falling edge.
 * @return 1 in the scan in which the edge occurred.
 */
static u8 edge_contact(u8 falling) {
  u8 cur = plc_exp_bit_read(CMD_REG), old = (u8)PLC_PL_BIT_TEST();
  if (cur) PLC_PL_BIT_ON(); else PLC_PL_BIT_OFF();
  return falling ? (old && !cur) : (!old && cur);
}

/** LDP bit: start a logic block with a rising-edge contact. */
void LDP(void) { u8 e = edge_contact(0); PLC_ACC_BIT <<= 1; if (e && plc_exp_rung_enabled()) PLC_ACC_BIT |= 0x01; }

/** LDF bit: start a logic block with a falling-edge contact. */
void LDF(void) { u8 e = edge_contact(1); PLC_ACC_BIT <<= 1; if (e && plc_exp_rung_enabled()) PLC_ACC_BIT |= 0x01; }

/** ANDP bit: rising-edge contact in series. */
void ANDP(void) { plc_exp_acc_and(edge_contact(0)); }

/** ANDF bit: falling-edge contact in series. */
void ANDF(void) { plc_exp_acc_and(edge_contact(1)); }

/** ORP bit: rising-edge contact in parallel. */
void ORP(void) { plc_exp_acc_or(edge_contact(0)); }

/** ORF bit: falling-edge contact in parallel. */
void ORF(void) { plc_exp_acc_or(edge_contact(1)); }


/* ====================================================================== */
/* Opcode routers                                                          */
/* ====================================================================== */

/**
 * Router for program words with a high nibble of 0 or 1.
 *
 * Handles the timer/counter coils (0x06nn / 0x0Enn), the extended contacts and coils,
 * and the application instructions. The 9-bit opcode (word & 0x1FF) indexes FUNC_EXT_0;
 * bit 12 of the word selects pulse ("P") execution, which makes the instruction run only
 * on the rising edge of the rung. Any other word with bits 11..9 set is not a valid
 * opcode and stops the PLC with PLC_RUNTIME_OPCODE.
 *
 * Instructions with a single marked bit operand have it fetched here, before the
 * dispatch, so that the operand words are consumed even when the rung is OFF.
 */
void FUNC_0(void) {
  u16 op = CMD_CMD & 0x01FFU;
  if ((CMD_CMD & 0xFF00U) == 0x0600U) { FUNC_TIMER(); return; }
  if ((CMD_CMD & 0xFF00U) == 0x0E00U) { FUNC_COUNTER(); return; }
  if (CMD_CMD & 0x0E00U) { plc_exp_fault(PLC_RUNTIME_OPCODE); return; }
  CMD_PULS = (CMD_CMD & 0x1000U) != 0U;
  /* Instructions carrying one marked bit operand read it before dispatch. */
  if (op >= 0x1C2U && op <= 0x1CFU) CMD_REG = plc_exp_bit_operand(0);
  else if ((op >= 2U && op <= 9U) || op == 0x0AU) CMD_REG = plc_exp_bit_operand(1);
  if (plc_exp_last_error) return;
  CMD_PULS_TEST = CMD_PULS ? PLC_LDP_TEST() : (PLC_ACC_BIT & 1U);
  FUNC_EXT_0[op]();
}

/**
 * Router for program words with a high nibble of 0xF: the second nibble selects STL
 * (0xF0nn..0xF3nn), RET (0xF7nn) or the logic-block group (0xFFnn, see FUNC_FF()).
 */
void FUNC_F(void) {
  FUNC_EXT_F[(CMD_CMD & 0x0F00) >> 8]();
}

/**
 * Logic-block instructions 0xFFnn, selected by the low byte.
 *
 *   0xF8 ANB  AND the two top logic blocks
 *   0xF9 ORB  OR the two top logic blocks
 *   0xFA MPS  push the accumulator on the MPS stack
 *   0xFB MRD  read the top of the MPS stack into the accumulator
 *   0xFC MPP  pop the MPS stack into the accumulator
 *   0xFD INV  invert the accumulator
 *   0xFF POP  not defined for the FX1N: stops the PLC
 *
 * Any other low byte is an unknown instruction and stops the PLC.
 */
void FUNC_FF(void) {
  switch( CMD_CMD & 0x00FF ) {
    case 0xF8: /* ANB */
      PLC_ACC_BIT = (PLC_ACC_BIT >> 1) & ( ( PLC_ACC_BIT & 0x01 ) | 0xFE );
      break;
    case 0xF9: /* ORB */
      PLC_ACC_BIT = (PLC_ACC_BIT >> 1) | (PLC_ACC_BIT & 0x01);
      break;
    case 0xFA: /* MPS */
      PLC_MPS_BIT = (PLC_MPS_BIT << 1) | (PLC_ACC_BIT & 0x01);
      break;
    case 0xFB: /* MRD */
      PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (PLC_MPS_BIT & 0x01);
      break;
    case 0xFC: /* MPP */
      PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (PLC_MPS_BIT & 0x01);
      PLC_MPS_BIT >>= 1;
      break;
    case 0xFD: /* INV */
      PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (~PLC_ACC_BIT & 0x01);
      break;
    case 0xFF: /* POP has no defined FX1N semantics: explicit fault. */
      PLC_PROG_ERROR(M8065,02);
      break;
    default:
      PLC_PROG_ERROR(M8064,02);
      break;
  }
}

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_logic_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
