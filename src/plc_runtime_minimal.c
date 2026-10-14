#include "plc_runtime_profile.h"
#if !PLC_RUNTIME_EXPERIMENTAL
/**
 * @file plc_runtime_minimal.c
 * @brief Minimal checked interpreter for the repository's machine code (the default profile).
 *
 * Executes a deliberately small instruction subset (contacts, coils, SET/RST, PLS/PLF,
 * OUT T/C, RST T/C, MOV, INC, the logic-block instructions and END). Before the first
 * instruction of a scan has any side effect, the *whole* program is decoded and validated:
 * operand ranges, logic-stack depth and the presence of an END are all checked. Any problem
 * latches a fault, clears the outputs and stops the PLC.
 *
 * See docs/runtime-subset.md for the exact instruction list and encodings. This is not a
 * general FX1N implementation; the broad instruction set lives in the experimental profile
 * (plc_exp_*.c). Both profiles implement the API of plc_runtime.h.
 */
#include "plc_memory_map.h"
#include "plc_special_devices.h"
#include "plc_memory.h"
#include "plc_runtime.h"
#include <string.h>

/** Masks to set (OR) or clear (AND) bit n of a byte. */
const unsigned char PLC_BIT_OR[] = {1,2,4,8,16,32,64,128};
const unsigned char PLC_BIT_AND[] = {254,253,251,247,239,223,191,127};
volatile u8 PLC_NEW_PROG;               /* set by the programming link after a new program was stored */
u8 PLC_PRG_STOP = 1;                    /* 1 outside a running scan (read by the main loop) */
u8 PLC_PULS;                            /* 1 during the first scan after RUN (initial pulse M8002) */

/* Depth of the logic-block (ANB/ORB) stack and of the branch (MPS/MRD/MPP) stack. */
#define STACK_LIMIT 8U
/* Size of the PLS/PLF edge memory: one bit per program word. */
#define PULSE_BYTES ((MAXSTEP + 7U) / 8U)
/* ---- Session state ---------------------------------------------------------------- */
static volatile u32 milliseconds;       /* 1 ms tick counter */
static u32 previous_ms;                 /* tick at the previous scan (timer base) */
static u16 timer_fraction[256];         /* sub-tick remainder per timer */
static u8 initialized, running;        /* runtime initialised / scanning since the last RUN */
static plc_runtime_error last_error;   /* first latched fault, PLC_RUNTIME_OK when none */
static u16 error_step;                   /* program word of the latched fault */
/* Values of the RUN/STOP mailbox written by the programming link. */
enum { REQUEST_NONE, REQUEST_STOP, REQUEST_RUN };
static volatile u8 pending_run_request;

/* Only main owns the M8000..8007 image. Interrupts publish a byte-sized
 * mailbox; the bounded critical section prevents a read/clear lost update. */
/** Post a RUN or STOP request; safe from interrupt context. Applied at the next scan. */
void PLC_Runtime_RequestRun(u8 run) {
  pending_run_request = run ? REQUEST_RUN : REQUEST_STOP;
}
/** Atomically fetch and clear the pending request. */
static u8 take_request(void) {
  u32 saved = __get_PRIMASK();
  u8 request;
  __disable_irq();
  request = pending_run_request;
  pending_run_request = REQUEST_NONE;
  __set_PRIMASK(saved);
  return request;
}
/** True when the PLC runs or is about to run: a pending request wins over the M8000/M8001 image. */
u8 PLC_Runtime_RunRequested(void) {
  u8 request = pending_run_request;
  if (request != REQUEST_NONE) return request == REQUEST_RUN;
  return PLC_BIT_TEST(M8000) && !PLC_BIT_TEST(M8001);
}

/* These exported helpers take communication LINEAR bit offsets, never an
 * instruction opcode or an AddrB_* historical alias. */
/** Read one bit of the PLC image (0 for an address outside the image). */
u32 PLC_BIT_TEST(u32 bit) {
  if (bit >= PLC_RAM_SIZE * 8U) return 0;
  return (PLC_8BIT(bit / 8U) >> (bit & 7U)) & 1U;
}
/** Set one bit; special relays (M8xxx) are updated with interrupts masked. */
void PLC_BIT_ON(u32 bit) {
  if (bit < PLC_RAM_SIZE * 8U) {
    u32 saved = 0;
    if (bit / 8U == Addr_M8) { saved = __get_PRIMASK(); __disable_irq(); }
    PLC_8BIT(bit / 8U) |= PLC_BIT_OR[bit & 7U];
    if (bit / 8U == Addr_M8) __set_PRIMASK(saved);
  }
}
/** Clear one bit; special relays (M8xxx) are updated with interrupts masked. */
void PLC_BIT_OFF(u32 bit) {
  if (bit < PLC_RAM_SIZE * 8U) {
    u32 saved = 0;
    if (bit / 8U == Addr_M8) { saved = __get_PRIMASK(); __disable_irq(); }
    PLC_8BIT(bit / 8U) &= PLC_BIT_AND[bit & 7U];
    if (bit / 8U == Addr_M8) __set_PRIMASK(saved);
  }
}
/** Set or clear a bit. */
static void put_bit(u32 bit, u8 value) {
  if (value) PLC_BIT_ON(bit); else PLC_BIT_OFF(bit);
}
/** Zero @p count bytes of the PLC image starting at @p offset. */
static void clear_bytes(u32 offset, u32 count) {
  u32 i;
  for (i = 0; i < count; ++i) PLC_8BIT(offset + i) = 0;
}
/** Forget the PLS/PLF edge memory. */
static void clear_pulses(void) {
  u32 i;
  for (i = 0; i < PULSE_BYTES; ++i) PLC_Bu(RAM_STEP_ADDR + i) = 0;
}
/**
 * Edge detector for PLS/PLF: remembers @p current in the bit belonging to program word
 * @p step and returns 1 on a rising (or, with @p falling, falling) edge.
 */
static u8 edge(u16 step, u8 current, u8 falling) {
  u32 addr = RAM_STEP_ADDR + step / 8U;
  u8 mask = PLC_BIT_OR[step & 7U];
  u8 old = (PLC_Bu(addr) & mask) != 0;
  if (current) PLC_Bu(addr) |= mask; else PLC_Bu(addr) &= (u8)~mask;
  return falling ? (old && !current) : (!old && current);
}
/**
 * Enter STOP: clear the outputs, timer enables and non-retentive timers (T0..T245) and
 * counter edge memory; keep M/S/D and counter values; publish M8001/M8003.
 */
static void stop(void) {
  u16 n;
  clear_bytes(Addr_Y, 64);
  clear_bytes(Addr_TE, 32);
  clear_bytes(Addr_CE, 25);
  for (n = 0; n < 246U; ++n) {
    PLC_16BIT(Addr_TVal+n*2U) = 0;
    timer_fraction[n] = 0;
    PLC_BIT_OFF(Addr_TO*8U+n);
  }
  PLC_BIT_OFF(M8000); PLC_BIT_ON(M8001);
  PLC_BIT_OFF(M8002); PLC_BIT_ON(M8003);
  PLC_PRG_STOP = 1; PLC_PULS = 1;
  if (running) clear_pulses();
  running = 0;
}
/** Latch the first fault (code in D8067, step in D8068/D8069, M8065 on) and stop the PLC. */
static void fault(plc_runtime_error code, u16 step) {
  if (last_error == PLC_RUNTIME_OK) {
    last_error = code; error_step = step;
    D8067 = (u16)code; D8068 = step; D8069 = step;
  }
  PLC_BIT_ON(M8065);
  stop();
}
/** Acknowledge a latched fault and clear its diagnostics. */
void PLC_Runtime_ClearFault(void) {
  last_error = PLC_RUNTIME_OK; error_step = 0;
  D8067 = D8068 = D8069 = 0; PLC_BIT_OFF(M8065);
}
/** First fault since the last acknowledgement, PLC_RUNTIME_OK when none. */
plc_runtime_error PLC_Runtime_LastError(void) { return last_error; }
/** Program word at which the latched fault occurred. */
u16 PLC_Runtime_ErrorStep(void) { return error_step; }
/** Reset the session: boot state is STOP, no fault, no pulse history. */
void PLC_Runtime_Init(void) {
  milliseconds = previous_ms = 0;
  memset(timer_fraction, 0, sizeof timer_fraction);
  running = 0; PLC_PULS = 1; PLC_PRG_STOP = 1;
  clear_pulses(); PLC_Runtime_ClearFault(); initialized = 1;
}
/** 1 ms tick; call from the timer interrupt. */
void PLC_Runtime_Tick1ms(void) { ++milliseconds; }

/* Contact operands use the original parser's linear bit image. Reject gaps,
 * T256+, C200+, and writes to X/T/C/special status bits. */
/** Is @p bit an addressable device, and (for @p write) one that a program may drive? */
static u8 bit_valid(u16 bit, u8 write) {
  if (bit < Addr_Y * 8U) return 1; /* M0..3071 */
  if (bit < Addr_M8 * 8U) return 1; /* Y0..511 */
  if (bit < Addr_CO * 8U) return !write; /* Special M8000..8255 */
  if (bit < Addr_CO * 8U + 200U) return !write;
  if (bit >= Addr_TO * 8U && bit < Addr_TO * 8U + 256U) return !write;
  if (bit >= Addr_X * 8U && bit < Addr_X * 8U + 512U) return !write;
  if (bit >= Addr_S * 8U && bit < Addr_S * 8U + 4096U) return 1;
  return 0;
}
/** Decode a marked 0x8000|bit operand word of the extended forms; returns 0 when invalid. */
static u8 extended_bit(u16 word, u16 *bit, u8 write) {
  /* Strip the operand marker, preserving address bit 12 (the old parser's
   * &0x2FFF incorrectly aliased X/S/T operands to unrelated M devices). */
  if ((word & 0xC000U) != 0x8000U) return 0;
  *bit = word & 0x3FFFU;
  return bit_valid(*bit, write);
}

/** One decoded instruction. @c length is the number of program words it occupies. */
typedef struct {
  u16 op;                 /* first program word */
  u16 bit;                /* bit device operand */
  u16 number;             /* timer/counter number */
  u16 source;             /* constant, or byte offset of the source register when source_register */
  u16 destination;        /* byte offset of the destination register */
  u16 length;             /* words occupied by the instruction */
  u8 kind;                /* NOP_KIND..END_KIND */
  u8 source_register;     /* 1 when @c source addresses a register, 0 for a constant */
} instruction;
/* Instruction classes handled by the interpreter. */
enum { NOP_KIND, CONTACT, COIL, TIMER, COUNTER, RESET_TC, STACK,
       MOVE, INCREMENT, END_KIND };

/** Decode a two-word D8000/T/C/D/D1000 register operand into its byte offset in the PLC image. */
static u8 register_address(u16 first, u16 second, u16 *offset) {
  u32 n = (first & 255U) | ((second & 255U) << 8);
  u32 base, limit;
  if ((first >> 8) != 0x86U) return 0;
  switch (second >> 8) {
    case 0x80: base = Addr_D8000; limit = 256; break;
    case 0x82: base = Addr_TVal; limit = 256; break;
    case 0x84: base = Addr_CVal; limit = 200; break;
    case 0x86: base = Addr_D0000; limit = 1000; break;
    case 0x88: base = Addr_D1000; limit = 1000; break;
    default: return 0;
  }
  if (n >= limit || base + n * 2U + 2U > PLC_RAM_SIZE) return 0;
  *offset = (u16)(base + n * 2U); return 1;
}
/**
 * Decode a source operand: a K/H constant or a register. For timer/counter presets
 * (@p preset) only K constants up to 32767 or an even D byte offset are valid.
 */
static u8 source_operand(u16 first, u16 second, instruction *d, u8 preset) {
  if (((first >> 8) == 0x80U || (!preset && (first >> 8) == 0x82U)) &&
      (second >> 8) == 0x80U) {
    d->source = (first & 255U) | ((second & 255U) << 8);
    d->source_register = 0;
    return !preset || d->source <= 32767U;
  }
  /* Timer/counter D presets in the legacy FUNC_TIMER/COUNTER are BYTE
   * offsets; MOV/INC operands instead contain a register number. */
  if (preset) {
    u16 n = (first & 255U) | ((second & 255U) << 8);
    if ((first >> 8) != 0x86U || (n & 1U) ||
        ((second >> 8) != 0x86U && (second >> 8) != 0x88U)) return 0;
    if (n / 2U >= 1000U) return 0;
    d->source = (u16)(((second >> 8) == 0x88U ? Addr_D1000 : Addr_D0000) + n);
    if (d->source + 2U > PLC_RAM_SIZE) return 0;
  } else if (!register_address(first, second, &d->source)) return 0;
  d->source_register = 1; return 1;
}

/**
 * Decode and validate the instruction at program word @p pc into @p d.
 * @return PLC_RUNTIME_OK, or BOUNDS (truncated instruction), OPERAND (bad device/range) or
 *         OPCODE (not part of the checked subset).
 */
static plc_runtime_error decode(u16 pc, instruction *d) {
  u16 w, a, b;
  u16 remaining = (u16)(MAXSTEP - pc);
  memset(d, 0, sizeof *d);
  if (!remaining) return PLC_RUNTIME_BOUNDS;
  w = (PLC_START_ADDR)[pc]; d->op = w; d->length = 1;
  if (w == 0x000FU || w == 0x001CU) { d->kind = END_KIND; return PLC_RUNTIME_OK; }
  if (w == 0x0000U) { d->kind = NOP_KIND; return PLC_RUNTIME_OK; }
  if (w >= 0xFFF8U && w <= 0xFFFDU) { d->kind = STACK; return PLC_RUNTIME_OK; }
  if ((w >> 12) >= 2U && (w >> 12) <= 7U) {
    d->kind = CONTACT; d->bit = w & 0xFFFU;
    return bit_valid(d->bit, 0) ? PLC_RUNTIME_OK : PLC_RUNTIME_OPERAND;
  }
  if ((w >> 12) >= 12U && (w >> 12) <= 14U) {
    d->kind = COIL; d->bit = w & 0xFFFU;
    return bit_valid(d->bit, 1) ? PLC_RUNTIME_OK : PLC_RUNTIME_OPERAND;
  }
  if (w >= 0x01C2U && w <= 0x01C7U) {
    if (remaining < 2U) return PLC_RUNTIME_BOUNDS;
    d->kind = CONTACT; d->length = 2;
    return extended_bit((PLC_START_ADDR)[pc+1], &d->bit, 0) ? PLC_RUNTIME_OK : PLC_RUNTIME_OPERAND;
  }
  if (w == 2U || w == 3U || w == 4U || w == 5U || w == 6U || w == 7U || w == 8U || w == 9U) {
    if (remaining < 2U) return PLC_RUNTIME_BOUNDS;
    d->kind = COIL; d->length = 2;
    return extended_bit((PLC_START_ADDR)[pc+1], &d->bit, 1) ? PLC_RUNTIME_OK : PLC_RUNTIME_OPERAND;
  }
  if ((w & 0xFF00U) == 0x0600U || (w & 0xFF00U) == 0x0E00U) {
    if (remaining < 3U) return PLC_RUNTIME_BOUNDS;
    d->kind = (w >> 8) == 6U ? TIMER : COUNTER;
    d->number = w & 255U; d->length = 3;
    if (d->kind == COUNTER && d->number >= 200U) return PLC_RUNTIME_OPERAND;
    return source_operand((PLC_START_ADDR)[pc+1], (PLC_START_ADDR)[pc+2], d, 1) ? PLC_RUNTIME_OK : PLC_RUNTIME_OPERAND;
  }
  if (w == 0x000CU) {
    if (remaining < 2U) return PLC_RUNTIME_BOUNDS;
    a = (PLC_START_ADDR)[pc+1]; d->kind = RESET_TC; d->length = 2; d->number = a & 255U;
    if ((a >> 8) == 0x86U) d->bit = Addr_TO * 8U;
    else if ((a >> 8) == 0x8EU && d->number < 200U) d->bit = Addr_CO * 8U;
    else return PLC_RUNTIME_OPERAND;
    return PLC_RUNTIME_OK;
  }
  if (w == 0x0028U || w == 0x0040U) {
    d->kind = w == 0x28U ? MOVE : INCREMENT;
    d->length = w == 0x28U ? 5U : 3U;
    if (remaining < d->length) return PLC_RUNTIME_BOUNDS;
    a = (PLC_START_ADDR)[pc+d->length-2]; b = (PLC_START_ADDR)[pc+d->length-1];
    if (!register_address(a,b,&d->destination) ||
        d->destination < Addr_D0000 || d->destination >= PLC_RAM_SIZE) return PLC_RUNTIME_OPERAND;
    if (d->kind == MOVE && !source_operand((PLC_START_ADDR)[pc+1], (PLC_START_ADDR)[pc+2],d,0)) return PLC_RUNTIME_OPERAND;
    return PLC_RUNTIME_OK;
  }
  return PLC_RUNTIME_OPCODE;
}

/** Model of the ladder logic stacks, used both for validation and for execution. */
typedef struct {
  u8 acc, valid, after_output, depth, branch_depth;
  u8 logic[STACK_LIMIT], branch[STACK_LIMIT];
} logic_state;
/**
 * Advance the logic-stack model by one instruction. With @p execute = 0 (validation pass) the
 * contact values are not read, so only the stack discipline (balanced ANB/ORB/MPS/MPP, no
 * output without a preceding load) is checked. Returns PLC_RUNTIME_STACK on a violation.
 */
static plc_runtime_error logic(instruction *d, logic_state *s, u8 execute) {
  u16 op = d->op;
  u8 v = execute ? (u8)PLC_BIT_TEST(d->bit) : 0;
  if (d->kind == CONTACT) {
    u8 family = d->length == 1 ? (u8)(op >> 12) : (u8)(op - 0x01C0U);
    if (family & 1U) v = !v;
    if (family <= 3U) {
      if (s->after_output) { s->depth = 0; s->valid = 0; }
      if (s->valid) {
        if (s->depth >= STACK_LIMIT) return PLC_RUNTIME_STACK;
        s->logic[s->depth++] = s->acc;
      }
      s->acc = v; s->valid = 1; s->after_output = 0;
    } else {
      if (!s->valid) return PLC_RUNTIME_STACK;
      s->acc = family <= 5U ? (s->acc && v) : (s->acc || v);
      s->after_output = 0;
    }
  } else if (d->kind == STACK) {
    if (!s->valid) return PLC_RUNTIME_STACK;
    switch (op) {
      case 0xFFF8: case 0xFFF9:
        if (!s->depth) return PLC_RUNTIME_STACK;
        v = s->logic[--s->depth];
        s->acc = op == 0xFFF8U ? (s->acc && v) : (s->acc || v); break;
      case 0xFFFA:
        if (s->branch_depth >= STACK_LIMIT) return PLC_RUNTIME_STACK;
        s->branch[s->branch_depth++] = s->acc; break;
      case 0xFFFB: case 0xFFFC:
        if (!s->branch_depth) return PLC_RUNTIME_STACK;
        s->acc = s->branch[s->branch_depth-1];
        if (op == 0xFFFCU) --s->branch_depth;
        break;
      case 0xFFFD: s->acc = !s->acc; break;
      default: return PLC_RUNTIME_OPCODE;
    }
    s->after_output = 0;
  } else if (d->kind == END_KIND) {
    if (s->branch_depth || s->depth) return PLC_RUNTIME_STACK;
  } else if (d->kind != NOP_KIND) {
    if (!s->valid || s->depth) return PLC_RUNTIME_STACK;
    s->after_output = 1;
  }
  return PLC_RUNTIME_OK;
}
/** Zero timer @p n: value, sub-tick remainder, contact and enable. */
static void timer_reset(u16 n) {
  PLC_16BIT(Addr_TVal + n*2U) = 0; timer_fraction[n] = 0;
  PLC_BIT_OFF(Addr_TO*8U+n); PLC_BIT_OFF(Addr_TE*8U+n);
}
/**
 * Advance every enabled timer by the @p elapsed milliseconds since the previous scan.
 * Bases: T0..T199 100 ms, T200..T245 10 ms, T246..T249 1 ms (retentive), T250..T255 100 ms
 * (retentive). The contact bit is refreshed with the value.
 */
static void advance_timers(u32 elapsed) {
  u16 n;
  for (n = 0; n < 256U; ++n) {
    u16 base = n < 200U || n >= 250U ? 100U : n < 246U ? 10U : 1U;
    if (PLC_BIT_TEST(Addr_TE*8U+n)) {
      u32 units = elapsed/base;
      u16 remainder = (u16)(elapsed%base + timer_fraction[n]);
      u16 value = PLC_16BIT(Addr_TVal+n*2U), max = PLC_16BIT(Addr_TMax+n*2U);
      units += remainder/base; timer_fraction[n] = remainder%base;
      if (value < max) {
        u32 gap = (u32)max-value;
        value = units >= gap ? max : (u16)(value+units);
        PLC_16BIT(Addr_TVal+n*2U) = value;
      }
      put_bit(Addr_TO*8U+n, value >= max);
    }
  }
}
/** Perform the side effect of instruction @p d (coils, timers, counters, RST, MOV, INC) with the current accumulator. */
static plc_runtime_error execute(instruction *d, u16 pc, logic_state *s) {
  u16 n = d->number;
  u16 preset = d->source_register ? PLC_16BIT(d->source) : d->source;
  u8 acc = s->acc;
  switch (d->kind) {
    case COIL:
      if (d->op == 8U || d->op == 9U) put_bit(d->bit, edge(pc, acc, d->op == 9U));
      else if ((d->op >> 12) == 13U || d->op == 3U || d->op == 6U) { if (acc) PLC_BIT_ON(d->bit); }
      else if ((d->op >> 12) == 14U || d->op == 4U || d->op == 7U) { if (acc) PLC_BIT_OFF(d->bit); }
      else put_bit(d->bit, acc);
      break;
    case TIMER:
      if (preset > 32767U) return PLC_RUNTIME_OPERAND;
      PLC_16BIT(Addr_TMax+n*2U) = preset;
      put_bit(Addr_TE*8U+n, acc);
      if (!acc && n < 246U) timer_reset(n);
      else if (acc) put_bit(Addr_TO*8U+n, PLC_16BIT(Addr_TVal+n*2U) >= preset);
      break;
    case COUNTER:
      if (preset > 32767U) return PLC_RUNTIME_OPERAND;
      if (acc && !PLC_BIT_TEST(Addr_CE*8U+n) && PLC_16BIT(Addr_CVal+n*2U) < preset)
        ++PLC_16BIT(Addr_CVal+n*2U);
      put_bit(Addr_CE*8U+n, acc);
      put_bit(Addr_CO*8U+n, PLC_16BIT(Addr_CVal+n*2U) >= preset);
      break;
    case RESET_TC:
      if (acc) {
        if (d->bit == Addr_TO*8U) timer_reset(n);
        else { PLC_16BIT(Addr_CVal+n*2U) = 0; PLC_BIT_OFF(Addr_CO*8U+n); PLC_BIT_OFF(Addr_CE*8U+n); }
      }
      break;
    case MOVE: if (acc) PLC_16BIT(d->destination) = preset; break;
    case INCREMENT: if (acc) ++PLC_16BIT(d->destination); break;
    default: break;
  }
  return PLC_RUNTIME_OK;
}
/**
 * Run one scan.
 *
 * 1. Apply a pending RUN/STOP request and refresh the clock relays M8011..M8014.
 * 2. Stay in STOP while a fault is latched, programming is in progress or RUN is off.
 * 3. Validate the whole program (decode every instruction, check stacks, require an END).
 * 4. Advance the timers, publish the initial pulse and execute the program up to END.
 * Any validation or execution error latches a fault and stops the PLC.
 */
void PLC_Main_Parser(void) {
  u16 pc, end = 0;
  u8 request;
  u32 now;
  instruction d;
  logic_state s;
  plc_runtime_error err;
  if (!initialized) PLC_Runtime_Init();
  now = milliseconds;
  request = take_request();
  if (request == REQUEST_STOP) { stop(); previous_ms = now; return; }
  if (request == REQUEST_RUN) {
    PLC_BIT_ON(M8000); PLC_BIT_OFF(M8001);
  }
  put_bit(M8011,(now % 10U) >= 5U);
  put_bit(M8012,(now % 100U) >= 50U);
  put_bit(M8013,(now % 1000U) >= 500U);
  put_bit(M8014,(now % 60000U) >= 30000U);
  if (PLC_NEW_PROG) {
    PLC_NEW_PROG = 0; PLC_Runtime_ClearFault(); clear_pulses();
    running = 0; memset(timer_fraction,0,sizeof timer_fraction);
    clear_bytes(Addr_TE,32); clear_bytes(Addr_CE,25);
  }
  /* A fault itself sets STOP. A subsequent software RUN request is the
   * explicit acknowledgement; always perform the complete validation again. */
  if (last_error && PLC_BIT_TEST(M8000) && !PLC_BIT_TEST(M8001))
    PLC_Runtime_ClearFault();
  if (last_error || PLC_UPDATE_PROG || !PLC_BIT_TEST(M8000) || PLC_BIT_TEST(M8001)) {
    stop(); previous_ms = now; return;
  }
  /* Validate the entire scan before any user instruction has side effects.
   * Operand words are consumed, never dispatched as independent instructions. */
  memset(&s,0,sizeof s);
  for (pc = 0; pc < MAXSTEP; pc = (u16)(pc+d.length)) {
    if (pending_run_request == REQUEST_STOP) { stop(); previous_ms = now; return; }
    err = decode(pc,&d);
    if (!err) err = logic(&d,&s,0);
    if (err) { fault(err,pc); previous_ms = now; return; }
    if (d.kind == END_KIND) { end = (u16)(pc+1U); break; }
  }
  if (!end) { fault(PLC_RUNTIME_NO_END,(u16)(MAXSTEP-1)); previous_ms = now; return; }
  if (!running) {
    clear_pulses(); PLC_PULS = 1; previous_ms = now;
  }
  advance_timers(now-previous_ms); previous_ms = now;
  running = 1; PLC_PRG_STOP = 0;
  put_bit(M8002,PLC_PULS); put_bit(M8003,!PLC_PULS);
  memset(&s,0,sizeof s);
  for (pc = 0; pc < end; pc = (u16)(pc+d.length)) {
    if (pending_run_request == REQUEST_STOP || !PLC_BIT_TEST(M8000) || PLC_BIT_TEST(M8001) || PLC_UPDATE_PROG) { stop(); return; }
    err = decode(pc,&d);
    if (!err) err = logic(&d,&s,1);
    if (pending_run_request == REQUEST_STOP) { stop(); return; }
    if (!err) err = execute(&d,pc,&s);
    if (err) { fault(err,pc); return; }
  }
  if (pending_run_request == REQUEST_STOP) { stop(); return; }
  PLC_PULS = 0; PLC_BIT_OFF(M8002); PLC_BIT_ON(M8003);
  PLC_PRG_STOP = 1;
}
/** Historical entry point used by main(): one scan. */
void PLC_ProInstructParse(void) { PLC_Main_Parser(); }

#endif /* !PLC_RUNTIME_EXPERIMENTAL */

typedef int plc_runtime_minimal_unit_marker;  /* keeps the translation unit non-empty when this profile is off */
