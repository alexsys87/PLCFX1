/**
 * @file plc_exp_scan.c
 * @brief Scan cycle, RUN/STOP handling and fault publication of the experimental interpreter.
 *
 * This module owns the interpreter registers and implements the firmware-facing runtime
 * API of plc_runtime.h for the experimental profile (the minimal profile implements the
 * same API in plc_runtime_minimal.c). One call of PLC_Main_Parser() performs one scan:
 *
 *   1. take a pending RUN/STOP request and refresh the clock relays M8011..M8014,
 *   2. advance the timers by the time elapsed since the previous scan,
 *   3. collect the program labels (words 0xB0nn),
 *   4. execute instructions until END, a fault, the dispatch budget or a STOP request.
 *
 * Any fault latches in D8067..D8069/M8065, clears the outputs and stops the PLC.
 */
#include "plc_runtime_profile.h"

#if PLC_RUNTIME_EXPERIMENTAL

#include <string.h>
#include "plc_exp_internal.h"

/* ---------------------------------------------------------------------------------------
 * Interpreter registers and session state (declared in plc_exp_internal.h)
 * ------------------------------------------------------------------------------------- */

volatile u8 plc_exp_pending_run_request;
volatile u32 plc_exp_milliseconds;
u32 plc_exp_scan_seconds;
u8 plc_exp_timer_fraction[256];
plc_runtime_error plc_exp_last_error;

u8 PLC_ACC_BIT;
u8 PLC_MPS_BIT;
const u16 *PLC_PC;
const u16 *PLC_Err;
u32 CMD_ADDR, CMD_REG, CMD_PULS, CMD_PULS_TEST;
u16 CMD_CMD;

u16 plc_exp_label_addr[128];
u16 plc_exp_call_pc[8];
u8 plc_exp_call_acc[8];
u8 plc_exp_call_depth;
plc_exp_for_frame_t plc_exp_for_stack[8];
u8 plc_exp_for_depth;
u8 plc_exp_mc_depth;
u8 plc_exp_mc_off;

u32 PLC_STL_Addr;
u8 PLC_STL_Status;
u8 PLC_STL_CMD;

u8 PLC_PULS;                                     /* 1 during the first scan after RUN (M8002) */
u8 PLC_PRG_STOP = 0;                             /* set by END or a fault to leave the scan loop */
volatile u8 PLC_NEW_PROG;                        /* reprogramming request from the programming link */

static u32 previous_ms;                          /* tick at the previous scan */
static u16 error_step;                           /* word offset of the faulting instruction */
static u8 initialized;                           /* PLC_Runtime_Init() has run */
static u8 running;                               /* the program has been scanned since the last STOP */

/** Masks that set or clear bit n (0..7) of a byte. */
static const u8 BIT_SET_MASK[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
static const u8 BIT_CLEAR_MASK[8] = {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F};

/** Scratch object returned for rejected accesses so that the caller never touches real memory. */
static union { uint64_t alignment; u8 bytes[8]; } rejected_access;

/* ---------------------------------------------------------------------------------------
 * Guarded access (see plc_exp_guard.h)
 * ------------------------------------------------------------------------------------- */

/**
 * Latch a runtime fault and stop the scan.
 * Only the first fault of a run is kept, together with the word offset of the faulting
 * instruction; later faults cannot overwrite it. PLC_PRG_STOP ends the dispatch loop, and
 * plc_exp_publish_fault() then reports the fault in D8067..D8069 and M8065.
 */
void plc_exp_fault(plc_runtime_error error)
{
  if (!plc_exp_last_error) {
    plc_exp_last_error = error;
    error_step = CMD_ADDR < MAXSTEP ? (u16)CMD_ADDR : (u16)(MAXSTEP - 1U);
  }
  PLC_PRG_STOP = 1;
}

/**
 * Validate a memory access made through the PLC_Bu/Hu/Wu/... accessors.
 * @return @p address itself when it is a multiple of @p alignment and the whole object of
 *         @p size bytes lies inside the PLC image or the pulse bitmap. Otherwise a
 *         PLC_RUNTIME_BOUNDS fault is latched and a private scratch object is returned, so
 *         the caller's read or write cannot touch any real memory.
 */
volatile void *plc_exp_memory(uintptr_t address, u32 size, u32 alignment)
{
  if (!(address & (alignment - 1U)) &&
      ((address >= PLC_RAM_BASE && address <= PLC_RAM_BASE + PLC_RAM_SIZE - size) ||
       (address >= PLC_STEPS_BASE && address <= PLC_STEPS_BASE + PLC_STEPS_SIZE - size))) {
    return (volatile void *)address;
  }
  plc_exp_fault(PLC_RUNTIME_BOUNDS);
  return rejected_access.bytes;
}

/**
 * Return the program word at the program counter without advancing it.
 * A counter outside the program area (or at an odd address) latches PLC_RUNTIME_BOUNDS and
 * the function returns 0.
 */
u16 plc_exp_peek_word(void)
{
  uintptr_t pc = (uintptr_t)PLC_PC, start = (uintptr_t)PLC_START_ADDR;

  if (pc < start || pc >= start + MAXSTEP * 2U || (pc & 1U)) {
    plc_exp_fault(PLC_RUNTIME_BOUNDS);
    return 0;
  }
  return *PLC_PC;
}

/**
 * Set or clear one bit of the PLC image by linear bit address.
 * A bit beyond the image latches PLC_RUNTIME_BOUNDS. Writes to the special-relay bytes
 * (M8000..M8255) run with interrupts masked, because the relays may also be updated from
 * interrupt context and the byte update is a read-modify-write.
 */
void plc_exp_bit_put(u32 bit, u8 on)
{
  u32 saved = 0;

  if (bit >= PLC_RAM_SIZE * 8U) {
    plc_exp_fault(PLC_RUNTIME_BOUNDS);
    return;
  }
  /* Special relays may also be updated from interrupt context: make the read-modify-write atomic. */
  if (bit / 8U == Addr_M8) {
    saved = __get_PRIMASK();
    __disable_irq();
  }
  if (on) PLC_8BIT(bit / 8U) |= BIT_SET_MASK[bit & 7U];
  else PLC_8BIT(bit / 8U) &= BIT_CLEAR_MASK[bit & 7U];
  if (bit / 8U == Addr_M8) __set_PRIMASK(saved);
}

/**
 * Read the edge-memory bit of the instruction at word offset @p step.
 * An offset beyond the program latches PLC_RUNTIME_BOUNDS and reads as 0.
 */
u32 plc_exp_pulse_test(u32 step)
{
  if (step >= MAXSTEP) {
    plc_exp_fault(PLC_RUNTIME_BOUNDS);
    return 0;
  }
  return (PLC_Bu(RAM_STEP_ADDR + step / 8U) >> (step & 7U)) & 1U;
}

/** Set or clear the edge-memory bit of the instruction at word offset @p step. */
void plc_exp_pulse_put(u32 step, u8 on)
{
  u32 addr;

  if (step >= MAXSTEP) {
    plc_exp_fault(PLC_RUNTIME_BOUNDS);
    return;
  }
  addr = RAM_STEP_ADDR + step / 8U;
  if (on) PLC_Bu(addr) |= BIT_SET_MASK[step & 7U];
  else PLC_Bu(addr) &= BIT_CLEAR_MASK[step & 7U];
}

/* ---------------------------------------------------------------------------------------
 * RUN/STOP control
 * ------------------------------------------------------------------------------------- */

/** Forget every per-instruction edge bit (pulse bitmap). */
static void plc_exp_clear_pulses(void)
{
  memset((void *)(uintptr_t)PLC_STEPS_BASE, 0, PLC_STEPS_SIZE);
}

/** Enter STOP: clear outputs and non-retentive timer state, keep M/S/D and counters. */
static void plc_exp_stop(void)
{
  u16 n;

  memset((void *)(uintptr_t)(RAM_ADDR + Addr_Y), 0, 64);     /* whole Y image */
  memset((void *)(uintptr_t)(RAM_ADDR + Addr_TE), 0, 32);    /* timer enables */
  memset((void *)(uintptr_t)(RAM_ADDR + Addr_CE), 0, 32);    /* counter edge memory */
  for (n = 0; n < 246U; ++n) {                               /* T246..T255 are retentive */
    PLC_16BIT(Addr_TVal + n * 2U) = 0;
    plc_exp_timer_fraction[n] = 0;
    PLC_BIT_OFF(Addr_TO * 8U + n);
  }
  PLC_BIT_OFF(M8000);
  PLC_BIT_ON(M8001);
  PLC_BIT_OFF(M8002);
  PLC_BIT_ON(M8003);
  PLC_PRG_STOP = 1;
  PLC_PULS = 1;
  PLC_STL_CMD = PLC_STL_Status = 0;
  if (running) plc_exp_clear_pulses();
  running = 0;
}

/** Post a RUN or STOP request; the next call of PLC_Main_Parser() applies it (see plc_runtime.h). */
void PLC_Runtime_RequestRun(u8 run)
{
  plc_exp_pending_run_request = run ? PLC_EXP_REQUEST_RUN : PLC_EXP_REQUEST_STOP;
}

/**
 * Is the PLC running or about to run?
 * A pending request decides; without one the run relays M8000/M8001 are consulted.
 */
u8 PLC_Runtime_RunRequested(void)
{
  u8 request = plc_exp_pending_run_request;

  if (request != PLC_EXP_REQUEST_NONE) return request == PLC_EXP_REQUEST_RUN;
  return PLC_BIT_TEST(M8000) && !PLC_BIT_TEST(M8001);
}

/** Atomically fetch and clear the pending RUN/STOP request. */
static u8 take_request(void)
{
  u32 saved = __get_PRIMASK();
  u8 request;

  __disable_irq();
  request = plc_exp_pending_run_request;
  plc_exp_pending_run_request = PLC_EXP_REQUEST_NONE;
  __set_PRIMASK(saved);
  return request;
}

/** Acknowledge the latched fault: clear its code and step, D8067..D8069 and M8065. */
void PLC_Runtime_ClearFault(void)
{
  plc_exp_last_error = PLC_RUNTIME_OK;
  error_step = 0;
  D8067 = D8068 = D8069 = 0;
  PLC_BIT_OFF(M8065);
}

/** The first fault since the last acknowledgement, or PLC_RUNTIME_OK. */
plc_runtime_error PLC_Runtime_LastError(void) { return plc_exp_last_error; }

/** Word offset of the instruction that caused the latched fault. */
u16 PLC_Runtime_ErrorStep(void) { return error_step; }

/**
 * Reset the runtime-private state: time base, timer remainders, call/loop/master-control
 * levels, edge memory and the latched fault. The PLC image and a pending RUN/STOP request
 * are kept. The PLC starts in the stopped state until a RUN request arrives.
 */
void PLC_Runtime_Init(void)
{
  plc_exp_milliseconds = previous_ms = 0;
  running = 0;
  PLC_PULS = 1;
  PLC_PRG_STOP = 1;
  PLC_ACC_BIT = PLC_MPS_BIT = PLC_STL_CMD = PLC_STL_Status = 0;
  plc_exp_call_depth = plc_exp_for_depth = plc_exp_mc_depth = plc_exp_mc_off = 0;
  memset(plc_exp_timer_fraction, 0, sizeof plc_exp_timer_fraction);
  memset(plc_exp_label_addr, 0xFF, sizeof plc_exp_label_addr);
  plc_exp_clear_pulses();
  PLC_Runtime_ClearFault();
  initialized = 1;
}

/** Advance the 1 ms time base (called from the timer interrupt). */
void PLC_Runtime_Tick1ms(void) { ++plc_exp_milliseconds; }

/* ---------------------------------------------------------------------------------------
 * Scan helpers
 * ------------------------------------------------------------------------------------- */

/** Set or clear an image bit without area checks (runtime-owned relays and contacts). */
static void plc_exp_image_put_bit(u32 bit, u8 value)
{
  if (value) PLC_BIT_ON(bit);
  else PLC_BIT_OFF(bit);
}

/**
 * Advance every enabled timer by the time elapsed since the previous scan.
 *
 * Bases: T0..T199 100 ms, T200..T245 10 ms, T246..T249 1 ms (retentive), T250..T255
 * 100 ms (retentive). The sub-tick remainder is kept per timer, and the contact bit is
 * refreshed together with the current value.
 */
static void plc_exp_advance_timers(u32 elapsed)
{
  u16 n;

  for (n = 0; n < 256U; ++n) {
    u16 base, remainder, current, maximum;
    u32 units;

    if (!PLC_BIT_TEST(Addr_TE * 8U + n)) continue;
    base = (n < 200U || n >= 250U) ? 100U : (n < 246U ? 10U : 1U);
    units = elapsed / base;
    remainder = (u16)(elapsed % base + plc_exp_timer_fraction[n]);
    current = PLC_16BIT(Addr_TVal + n * 2U);
    maximum = PLC_16BIT(Addr_TMax + n * 2U);
    units += remainder / base;
    plc_exp_timer_fraction[n] = (u8)(remainder % base);
    if (current < maximum) {
      u32 gap = (u32)maximum - current;
      current = units >= gap ? maximum : (u16)(current + units);
      PLC_16BIT(Addr_TVal + n * 2U) = current;
    }
    plc_exp_image_put_bit(Addr_TO * 8U + n, current >= maximum);
  }
}

/** After a fault: publish the code and step in D8067..D8069/M8065 and stop the PLC. */
static void plc_exp_publish_fault(void)
{
  if (plc_exp_last_error) {
    D8067 = (u16)plc_exp_last_error;
    D8068 = D8069 = error_step;
    PLC_BIT_ON(M8065);
    plc_exp_stop();
  }
}

/** True while the scan has to be abandoned (STOP request, programming in progress, M8000 off). */
static u8 plc_exp_must_stop(void)
{
  return plc_exp_pending_run_request == PLC_EXP_REQUEST_STOP || PLC_UPDATE_PROG ||
         !PLC_BIT_TEST(M8000) || PLC_BIT_TEST(M8001);
}

/* ---------------------------------------------------------------------------------------
 * One scan
 * ------------------------------------------------------------------------------------- */

/**
 * Run one PLC scan (see the file header for the sequence of steps).
 *
 * The scan ends without executing the program when the PLC is stopped, a download is in
 * progress or a fault is latched. Otherwise instructions are executed until END, a fault, a
 * STOP request, or the dispatch budget of MAXSTEP * 4 instructions ends the scan. A fault
 * is published and stops the PLC.
 */
void PLC_Main_Parser(void)
{
  u16 i;
  u32 now, budget = MAXSTEP * 4U;                /* per-scan dispatch budget (bounds loops) */
  u8 request;

  if (!initialized) PLC_Runtime_Init();
  now = plc_exp_milliseconds;                    /* one time stamp for the whole scan */
  request = take_request();
  if (request == PLC_EXP_REQUEST_STOP) {
    plc_exp_stop();
    previous_ms = now;
    return;
  }
  if (request == PLC_EXP_REQUEST_RUN) {          /* RUN request: switch the run monitors */
    PLC_BIT_ON(M8000);
    PLC_BIT_OFF(M8001);
  }

  /* Clock relays: 10 ms, 100 ms, 1 s and 1 min square waves. */
  plc_exp_image_put_bit(M8011, (now % 10U) >= 5U);
  plc_exp_image_put_bit(M8012, (now % 100U) >= 50U);
  plc_exp_image_put_bit(M8013, (now % 1000U) >= 500U);
  plc_exp_image_put_bit(M8014, (now % 60000U) >= 30000U);

  if (PLC_NEW_PROG) {                            /* a new program was downloaded: start clean */
    PLC_NEW_PROG = 0;
    running = 0;
    PLC_Runtime_ClearFault();
    plc_exp_clear_pulses();
    memset(plc_exp_timer_fraction, 0, sizeof plc_exp_timer_fraction);
    plc_exp_call_depth = plc_exp_for_depth = plc_exp_mc_depth = plc_exp_mc_off = 0;
  }
  if (plc_exp_last_error && PLC_BIT_TEST(M8000) && !PLC_BIT_TEST(M8001)) {
    PLC_Runtime_ClearFault();                    /* an explicit RUN acknowledges the latched fault */
  }
  if (plc_exp_last_error || PLC_UPDATE_PROG || !PLC_BIT_TEST(M8000) || PLC_BIT_TEST(M8001)) {
    plc_exp_stop();                              /* stopped, faulted or being programmed: no scan */
    previous_ms = now;
    return;
  }

  /* Labels (B0nn) are found by raw words, not by a validated instruction map: a data word that
   * looks like a label is not distinguished from a real one. The index is limited to the
   * 128-entry table and the first definition wins. */
  memset(plc_exp_label_addr, 0xFF, sizeof plc_exp_label_addr);
  for (i = 0; i < MAXSTEP; ++i) {
    u16 w = (PLC_START_ADDR)[i];

    if ((w & 0xFF00U) == 0xB000U && (w & 0xFFU) < 128U && plc_exp_label_addr[w & 0xFFU] == 0xFFFFU) {
      plc_exp_label_addr[w & 0xFFU] = i;
    }
  }

  if (!running) {                                /* first scan after RUN */
    plc_exp_clear_pulses();
    PLC_PULS = 1;
    previous_ms = now;
  }
  plc_exp_scan_seconds = (previous_ms % 1000U + (now - previous_ms)) / 1000U;
  plc_exp_advance_timers(now - previous_ms);
  previous_ms = now;
  running = 1;
  PLC_PRG_STOP = 0;
  plc_exp_image_put_bit(M8002, PLC_PULS);        /* initial pulse */
  plc_exp_image_put_bit(M8003, !PLC_PULS);

  PLC_ACC_BIT = PLC_MPS_BIT = 0;                 /* every scan starts with empty logic stacks */
  PLC_STL_CMD = PLC_STL_Status = 0;
  plc_exp_call_depth = plc_exp_for_depth = plc_exp_mc_depth = plc_exp_mc_off = 0;
  PLC_PC = PLC_START_ADDR;

  /* Fetch/dispatch loop: the first word selects the router through FUNC_BASIC[word >> 12]. */
  while (!PLC_PRG_STOP && !plc_exp_last_error) {
    if (plc_exp_must_stop()) {
      plc_exp_stop();
      return;
    }
    if (!budget--) {
      plc_exp_fault(PLC_RUNTIME_LEGACY_BUDGET);
      break;
    }
    CMD_CMD = plc_exp_peek_word();
    if (plc_exp_last_error) break;
    CMD_REG = CMD_CMD & 0x0FFFU;
    CMD_ADDR = (u32)(PLC_PC - PLC_START_ADDR);
    PLC_Err = PLC_PC++;
    FUNC_BASIC[CMD_CMD >> 12]();
  }

  plc_exp_publish_fault();                       /* a fault stops the PLC and clears the outputs */
  if (plc_exp_last_error) return;
  if (plc_exp_pending_run_request == PLC_EXP_REQUEST_STOP) {
    plc_exp_stop();
    return;
  }
  PLC_PULS = 0;                                  /* the first scan after RUN is over */
  PLC_BIT_OFF(M8002);
  PLC_BIT_ON(M8003);
}

/** Historical name of PLC_Main_Parser(), still used by the main loop. */
void PLC_ProInstructParse(void) { PLC_Main_Parser(); }

#endif /* PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_exp_scan_unit_marker;            /* keeps the translation unit non-empty when this profile is off */
