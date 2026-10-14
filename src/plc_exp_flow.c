/**
 * @file plc_exp_flow.c
 * @brief Program flow: CJ, CALL/SRET, FOR/NEXT, WDT and the interrupt placeholders.
 *
 * Labels are the program words 0xB0nn collected once per scan by the scan adapter; CALL
 * and FOR nest 8 levels deep. Interrupt instructions (IRET/EI/DI) deliberately fault
 * because the board has no interrupt sources.
 *
 * Jump targets are word offsets into the program. After CJ and CALL the program counter
 * is set just behind the label word, so execution continues with the first instruction
 * of the labelled section. All levels (CALL return addresses, FOR frames) are cleared at
 * the start of every scan; END reports an open level as a circuit error.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* ====================================================================== */
/* Program flow: CJ, CALL, SRET, FEND (END), WDT, FOR, NEXT                 */
/* Pointer operand 88LL 80HH carries 2*n for label Pn (the repository's     */
/* original convention); the label word itself is B0nn.                     */
/* ====================================================================== */

/**
 * Decode the pointer operand of CJ/CALL and look up its label.
 *
 * The operand is validated even when the jump is not taken, because it has to be consumed
 * either way. A malformed operand, an odd pointer value, a pointer above P127 or a label
 * that is not defined in the program stops the PLC with PLC_RUNTIME_OPERAND.
 *
 * @return the word offset of the label word 0xB0nn; 0 after a fault.
 */
static u16 label_target(void) {
  operand_t p;
  u32 n;
  opd_decode(&p, 0);
  if (plc_exp_last_error) return 0;
  n = p.value >> 1;
  if (p.kind != OPD_PTR || (p.value & 1U) || n >= 128U || plc_exp_label_addr[n] == 0xFFFFU) {
    plc_exp_fault(PLC_RUNTIME_OPERAND);
    return 0;
  }
  return plc_exp_label_addr[n];
}

/** CJ Pn: conditional jump; while the rung is ON execution continues behind label Pn. */
void CJ(void) {
  u16 t = label_target();
  if (plc_exp_last_error || !CMD_PULS_TEST) return;
  PLC_PC = PLC_START_ADDR + t + 1;               /* continue after the label word */
}

/**
 * CALL Pn: call the subroutine at label Pn while the rung is ON.
 * The return address and the accumulator are saved (8 levels), so the rung continues
 * after the call as if the subroutine had not run. A ninth level stops the PLC with
 * PLC_RUNTIME_STACK.
 */
void CALL(void) {
  u16 t = label_target();
  if (plc_exp_last_error || !CMD_PULS_TEST) return;
  if (plc_exp_call_depth >= 8U) { plc_exp_fault(PLC_RUNTIME_STACK); return; }
  plc_exp_call_pc[plc_exp_call_depth] = (u16)(PLC_PC - PLC_START_ADDR);
  plc_exp_call_acc[plc_exp_call_depth++] = PLC_ACC_BIT;
  PLC_PC = PLC_START_ADDR + t + 1;
}

/**
 * SRET: return from a subroutine to the instruction behind its CALL and restore the
 * accumulator saved by CALL. SRET without a matching CALL stops the PLC with
 * PLC_RUNTIME_STACK.
 */
void SRET(void) {
  if (!plc_exp_call_depth) { plc_exp_fault(PLC_RUNTIME_STACK); return; }
  --plc_exp_call_depth;
  PLC_PC = PLC_START_ADDR + plc_exp_call_pc[plc_exp_call_depth];
  PLC_ACC_BIT = plc_exp_call_acc[plc_exp_call_depth];
}

/* Interrupt processing needs hardware interrupt sources that this board does
 * not provide, so IRET/EI/DI stay explicit faults instead of silent no-ops. */

/** IRET: return from an interrupt routine; rejected (no interrupt sources). */
void IRET(void) {
  PLC_PROG_ERROR(M8065,02);
}

/** EI: enable interrupts; rejected (no interrupt sources). */
void EI(void) {
  PLC_PROG_ERROR(M8065,02);
}

/** DI: disable interrupts; rejected (no interrupt sources). */
void DI(void) {
  PLC_PROG_ERROR(M8065,02);
}

/** WDT: watchdog refresh. There is no scan watchdog to refresh; WDT is a defined no-op. */
void WDT(void) {
}

/**
 * FOR n: start of a loop that repeats the instructions up to the matching NEXT n times.
 * Like on the FX series, FOR is executed regardless of the rung. The count is read when
 * FOR executes; a count below 1 runs the loop once. Loops nest 8 levels deep (a ninth
 * stops the PLC with PLC_RUNTIME_STACK). A loop that is too long for the per-scan dispatch
 * budget ends with PLC_RUNTIME_LEGACY_BUDGET.
 */
void FOR(void) {
  operand_t n;
  s32 c;
  opd_decode(&n, 0);
  if (plc_exp_last_error) return;
  if (plc_exp_for_depth >= 8U) { plc_exp_fault(PLC_RUNTIME_STACK); return; }
  c = opd_gets(&n, 0, 0);
  plc_exp_for_stack[plc_exp_for_depth].start = (u16)(PLC_PC - PLC_START_ADDR);
  plc_exp_for_stack[plc_exp_for_depth].cycle = 0;
  plc_exp_for_stack[plc_exp_for_depth].count = c < 1 ? 1U : (u16)c;
  plc_exp_for_depth++;
}

/**
 * NEXT: end of the innermost FOR loop. Jumps back behind the FOR instruction until the
 * loop has run its count, then leaves the loop. NEXT without FOR stops the PLC with
 * PLC_RUNTIME_STACK.
 */
void NEXT(void) {
  if (!plc_exp_for_depth) { plc_exp_fault(PLC_RUNTIME_STACK); return; }
  if (++plc_exp_for_stack[plc_exp_for_depth - 1U].cycle < plc_exp_for_stack[plc_exp_for_depth - 1U].count)
    PLC_PC = PLC_START_ADDR + plc_exp_for_stack[plc_exp_for_depth - 1U].start;
  else
    plc_exp_for_depth--;
}

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_flow_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
