/**
 * @file test_runtime.c
 * @brief Host test of the minimal PLC runtime (src/plc_runtime_minimal.c).
 *
 * The test runs complete scans of hand-assembled bytecode programs through the real
 * interpreter. The runtime source is #included, so its static state is visible here and
 * nothing is copied; only the Cortex-M interrupt-mask intrinsics are replaced by a mock
 * (see platform.h). The PLC image, the pulse bitmap and the program flash are mapped at
 * their fixed target addresses with mmap(MAP_FIXED), so every access made by the runtime
 * is a real memory access that AddressSanitizer and UBSan can check.
 * tools/test_runtime.py compiles and runs it.
 *
 * Programs are written as raw program words (docs/runtime-subset.md lists the encodings).
 * Quick reference for the comments in this file:
 *
 *   2bbb 3bbb 4bbb 5bbb 6bbb 7bbb  LD, LDI, AND, ANI, OR, ORI of the linear bit bbb
 *   Cbbb Dbbb Ebbb                 OUT, SET, RST of the linear bit bbb
 *   01C2..01C7, 8000|bit           LD, LDI, AND, ANI, OR, ORI of any bit (extended form)
 *   0002..0004, 8000|bit           OUT, SET, RST of any bit (extended form)
 *   0008 / 0009, 8000|bit          PLS / PLF
 *   FFF8..FFFD                     ANB, ORB, MPS, MRD, MPP, INV
 *   0600|n, preset                 OUT Tn       (preset K: 80LL 80HH, D: byte offset)
 *   0E00|n, preset                 OUT Cn
 *   000C, 8600|n / 8E00|n          RST Tn / RST Cn
 *   0028 src dst / 0040 dst        MOV / INC
 *   000F                           END
 *
 * Linear bit bases: M 0000, Y 0C00, M8000 0E00, C contact 0F00, T contact 1000,
 * X 1200, S 1400. Y0 is therefore OUT CC00 in the basic form (Cbbb with bbb = C00).
 */
#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "../../../src/plc_runtime_minimal.c"

/* Set by the programming link while a program is being downloaded; the runtime only reads it. */
volatile u8 PLC_UPDATE_PROG;

/* ---- Mock of the interrupt-mask intrinsics --------------------------------------------- */
/* The runtime brackets its read-modify-write sequences with __disable_irq() and
 * __set_PRIMASK(saved). The mock models the mask bit, checks that __set_PRIMASK() is only
 * called inside such a section, and can inject a simulated USART interrupt: when the
 * inject_at-th critical section ends, PLC_Runtime_RequestRun(injected_run) is called as an
 * ISR would. */
static u32 primask;                              /* modelled PRIMASK: 1 = interrupts masked */
static unsigned critical_exits, inject_at;       /* sections left so far / section that fires the ISR */
static u8 injected_run;                          /* request posted by the simulated ISR */
u32 __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask=1; }
void __set_PRIMASK(u32 value) {
  assert(primask==1);                            /* must be called inside a critical section */
  primask=value;
  ++critical_exits;
  if (!value && inject_at && critical_exits==inject_at) {
    inject_at=0;
    PLC_Runtime_RequestRun(injected_run); /* Simulated USART ISR. */
  }
}

/* ---- Fixture helpers ------------------------------------------------------------------- */
static unsigned scans;                           /* complete scans executed by the whole test */
#define WORDS(a) (sizeof(a)/sizeof((a)[0]))      /* number of program words in an array */

/** Map a zero-filled region at the fixed address @p base; abort if the kernel placed it elsewhere. */
static void map(u32 base, u32 size) {
  assert(mmap((void *)(uintptr_t)base,size,PROT_READ|PROT_WRITE,
              MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) == (void *)(uintptr_t)base);
}

/**
 * Start a test case: clear the PLC image and the pulse bitmap, erase the program flash
 * (0xFF), copy @p count program words to the program start, forget any pending request and
 * initialise the runtime. The special-relay byte is then set to 0x09 (M8000 on, M8001 off,
 * M8003 on), which is the "RUN" state, so the next scan starts executing the program.
 */
static void load(const u16 *words, size_t count) {
  assert(count <= MAXSTEP);
  memset((void *)(uintptr_t)PLC_RAM_BASE,0,PLC_RAM_SIZE);
  memset((void *)(uintptr_t)PLC_STEPS_BASE,0,PLC_STEPS_SIZE);
  memset((void *)(uintptr_t)ROM_ADDR,0xFF,14U*1024U);
  memcpy((void *)PLC_START_ADDR,words,count*2U);
  PLC_NEW_PROG = 0; PLC_UPDATE_PROG = 0;
  pending_run_request=REQUEST_NONE; primask=0; inject_at=0; critical_exits=0;
  PLC_Runtime_Init(); PLC_8BIT(Addr_M8) = 0x09;
}
/** Run one complete scan and count it. */
static void scan(void) { PLC_Main_Parser(); ++scans; }
/** Advance the runtime time base by @p n milliseconds (the 1 ms timer interrupt on the target). */
static void tick(unsigned n) { while (n--) PLC_Runtime_Tick1ms(); }
/** Assert the STOP state: M8000 off, M8001 on and the whole 64-byte Y image cleared. */
static void stopped(void) {
  assert(!PLC_BIT_TEST(M8000) && PLC_BIT_TEST(M8001));
  for (unsigned i=0;i<64;i++) assert(PLC_8BIT(Addr_Y+i)==0);
}
/**
 * RUN/STOP control through the special relays.
 * Program: LD M8000, OUT M0, LD M8002, OUT M1, END. M0 mirrors the run monitor and M1 the
 * initial pulse, so M1 is on during the first scan after RUN only.
 */
static void test_start_stop(void) {
  const u16 p[] = {0x01C2,0x8E00,0xC000,    /* LD M8000, OUT M0 */
                   0x01C2,0x8E02,0xC001,    /* LD M8002, OUT M1 */
                   0x000F};                 /* END */
  load(p,WORDS(p)); scan(); assert(PLC_BIT_TEST(0) && PLC_BIT_TEST(1));   /* first scan: initial pulse */
  scan(); assert(PLC_BIT_TEST(0) && !PLC_BIT_TEST(1));                    /* later scans: no pulse */
  /* STOP through the image (M8001 on, M8000 off) clears the outputs, even Y511. */
  PLC_8BIT(Addr_M8) = 0x0A; PLC_BIT_ON(Addr_Y*8U+511); scan(); stopped();
  /* RUN again: the restart produces the initial pulse once more. */
  PLC_8BIT(Addr_M8) = 0x09; scan(); assert(PLC_BIT_TEST(1));
  /* A program download in progress inhibits execution and clears Y. */
  PLC_UPDATE_PROG=1; scan(); stopped();
  /* Extra bits in the M8 byte must not spuriously cancel RUN. */
  PLC_UPDATE_PROG=0; PLC_8BIT(Addr_M8)=0x39; scan(); assert(PLC_BIT_TEST(0));
}
/**
 * Clock relays M8011..M8014 as seen by the program.
 * Program: LD M8013, OUT Y0, END. Every clock is a square wave with a 50 % duty cycle whose
 * first half is low: M8011 10 ms, M8012 100 ms, M8013 1 s, M8014 60 s. The tick counts land
 * on the first millisecond of each high or low half.
 */
static void test_clock_sample(void) {
  const u16 p[]={0x01C2,0x8E0D,0xCC00,      /* LD M8013, OUT Y0 */
                 0x000F};                   /* END */
  load(p,WORDS(p)); scan(); assert(!PLC_BIT_TEST(Addr_Y*8));
  tick(500); scan(); assert(PLC_BIT_TEST(Addr_Y*8));
  tick(500); scan(); assert(!PLC_BIT_TEST(Addr_Y*8));
  tick(5); scan(); assert(PLC_BIT_TEST(M8011));
  tick(45); scan(); assert(PLC_BIT_TEST(M8012));
  tick(28950); scan(); assert(PLC_BIT_TEST(M8014));
  tick(30000); scan(); assert(!PLC_BIT_TEST(M8014));
}
/**
 * Device mapping and the six contact families.
 * First program: (X0 AND S0) OR T0 drives Y0, LDI C199 drives Y1 and LD M8255 (the last
 * special relay, a basic-form word) drives Y2 - extended X/S/T/C contacts all resolve to
 * their own devices.
 */
static void test_mapping_logic(void) {
  const u16 p[]={0x01C2,0x9200,             /* LD X0 */
                 0x01C4,0x9400,             /* AND S0 */
                 0x01C6,0x9000,             /* OR T0 */
                 0xCC00,                    /* OUT Y0 */
                 0x01C3,0x8FC7,0xCC01,      /* LDI C199, OUT Y1 */
                 0x2EFF,0xCC02,             /* LD M8255, OUT Y2 */
                 0x000F};                   /* END */
  load(p,WORDS(p)); PLC_BIT_ON(Addr_X*8); PLC_BIT_ON(Addr_S*8);
  PLC_BIT_ON(Addr_M8*8+255); scan();
  assert(PLC_Runtime_LastError()==0 && PLC_BIT_TEST(Addr_Y*8));
  assert(PLC_BIT_TEST(Addr_Y*8+1) && PLC_BIT_TEST(Addr_Y*8+2));
  assert(PLC_BIT_TEST(Addr_S*8) && !PLC_BIT_TEST(0));
  /* Six basic contact families and SET/RST in a complete scan. */
  const u16 q[]={0x2000,0x4001,0x5002,0x6003,0x7004,   /* LD M0, AND M1, ANI M2, OR M3, ORI M4 */
                 0xD005,                               /* SET M5 */
                 0x3006,0xE007,                        /* LDI M6, RST M7 */
                 0x000F};                              /* END */
  load(q,WORDS(q)); PLC_BIT_ON(0); PLC_BIT_ON(1); PLC_BIT_ON(7); scan();
  assert(PLC_BIT_TEST(5) && !PLC_BIT_TEST(7));
  /* S4095 and Y511 use extended output operands. */
  const u16 r[]={0x01C2,0x8E00,             /* LD M8000 */
                 0x0002,0xA3FF,             /* OUT S4095 */
                 0x0002,0x8DFF,             /* OUT Y511 */
                 0x000F};                   /* END */
  load(r,WORDS(r)); scan(); assert(PLC_BIT_TEST(Addr_S*8+4095));
  assert(PLC_BIT_TEST(Addr_Y*8+511));
}
/**
 * Logic-block and branch stacks. With M1 and M2 on, (M0 OR M1) AND M2 is true: ORB and ANB
 * combine the loaded blocks, MPS/MRD/MPP branch the result to three coils, and INV turns
 * the final value around, so Y2 stays off while Y0 and Y1 are on.
 */
static void test_stacks(void) {
  const u16 p[]={0x2000,0x2001,0xFFF9,      /* LD M0, LD M1, ORB */
                 0x2002,0xFFF8,             /* LD M2, ANB */
                 0xFFFA,0xCC00,             /* MPS, OUT Y0 */
                 0xFFFB,0x5003,0xCC01,      /* MRD, ANI M3, OUT Y1 */
                 0xFFFC,0xFFFD,0xCC02,      /* MPP, INV, OUT Y2 */
                 0x000F};                   /* END */
  load(p,WORDS(p)); PLC_BIT_ON(1); PLC_BIT_ON(2); scan();
  assert(!PLC_Runtime_LastError());
  assert(PLC_BIT_TEST(Addr_Y*8) && PLC_BIT_TEST(Addr_Y*8+1));
  assert(!PLC_BIT_TEST(Addr_Y*8+2));
}
/**
 * PLS and PLF. Program: LD M0, PLS Y0, PLF Y1, END. A pulse lasts exactly one scan; PLF does
 * not fire from the low level at startup; STOP forgets the edge history, so after a restart
 * with M0 still on, PLS fires again.
 */
static void test_pulse(void) {
  const u16 p[]={0x2000,                    /* LD M0 */
                 0x0008,0x8C00,             /* PLS Y0 */
                 0x0009,0x8C01,             /* PLF Y1 */
                 0x000F};                   /* END */
  load(p,WORDS(p)); scan(); assert(!PLC_BIT_TEST(Addr_Y*8+1));
  PLC_BIT_ON(0); scan(); assert(PLC_BIT_TEST(Addr_Y*8));
  scan(); assert(!PLC_BIT_TEST(Addr_Y*8));
  PLC_BIT_OFF(0); scan(); assert(PLC_BIT_TEST(Addr_Y*8+1));
  scan(); assert(!PLC_BIT_TEST(Addr_Y*8+1));
  PLC_BIT_ON(0); scan(); PLC_8BIT(Addr_M8)=0x0A; scan(); stopped();
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_BIT_TEST(Addr_Y*8));
}
/**
 * Timer time bases, saturation, reset and retention.
 * First program: four timers with preset K2 on the same rung - T0 (100 ms), T200 (10 ms),
 * T246 (1 ms, retentive) and T250 (100 ms, retentive) - and Y0 follows the contact of T0.
 * Non-retentive timers (T0..T245) reset when their rung drops or the PLC stops; the
 * retentive ones (T246..T255) keep their value, also across STOP and RUN. Further programs
 * check a D-register preset (given as a byte offset) and the last timer/counter numbers.
 */
static void test_timers(void) {
  const u16 p[]={0x2000,                    /* LD M0 */
                 0x0600,0x8002,0x8000,      /* OUT T0 K2 */
                 0x06C8,0x8002,0x8000,      /* OUT T200 K2 */
                 0x06F6,0x8002,0x8000,      /* OUT T246 K2 */
                 0x06FA,0x8002,0x8000,      /* OUT T250 K2 */
                 0x01C2,0x9000,0xCC00,      /* LD T0, OUT Y0 */
                 0x000F};                   /* END */
  load(p,WORDS(p)); PLC_BIT_ON(0); scan();
  tick(1); scan(); assert(PLC_16BIT(Addr_TVal+246*2)==1);
  tick(1); scan(); assert(PLC_BIT_TEST(Addr_TO*8+246));
  tick(18); scan(); assert(PLC_BIT_TEST(Addr_TO*8+200));
  tick(179); scan(); assert(!PLC_BIT_TEST(Addr_TO*8));
  tick(1); scan(); assert(PLC_BIT_TEST(Addr_Y*8));
  assert(PLC_16BIT(Addr_TVal)==2 && PLC_16BIT(Addr_TMax)==2);
  PLC_BIT_OFF(0); scan(); assert(PLC_16BIT(Addr_TVal)==0);
  assert(PLC_16BIT(Addr_TVal+246*2)==2 && PLC_16BIT(Addr_TVal+250*2)==2);
  tick(1000); scan(); assert(PLC_16BIT(Addr_TVal+246*2)==2);
  /* Legacy OUT T D operand is a byte offset, so 8602/8600 means D1. */
  const u16 q[]={0x2000,                    /* LD M0 */
                 0x0601,0x8602,0x8600,      /* OUT T1 D1 (D preset as byte offset 2) */
                 0x000F};                   /* END */
  load(q,WORDS(q)); PLC_16BIT(Addr_D0000+2)=3; PLC_BIT_ON(0); scan();
  tick(300); scan(); assert(PLC_16BIT(Addr_TVal+2)==3);
  PLC_8BIT(Addr_M8)=0x0A; scan(); tick(1000);
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_16BIT(Addr_TVal+2)==0);
  /* T255 and C199 sit at the upper end of their ranges. */
  const u16 r[]={0x2000,                    /* LD M0 */
                 0x06FF,0x8001,0x8000,      /* OUT T255 K1 */
                 0x0EC7,0x8001,0x8000,      /* OUT C199 K1 */
                 0x01C2,0x90FF,0xCC00,      /* LD T255, OUT Y0 */
                 0x01C2,0x8FC7,0xCC01,      /* LD C199, OUT Y1 */
                 0x000F};                   /* END */
  load(r,WORDS(r)); PLC_BIT_ON(0); scan(); tick(100); scan();
  assert(PLC_BIT_TEST(Addr_Y*8) && PLC_BIT_TEST(Addr_Y*8+1));
  assert(PLC_16BIT(Addr_TVal+510)==1 && PLC_16BIT(Addr_CVal+398)==1);
  PLC_8BIT(Addr_M8)=0x0A; scan(); tick(1000);
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_16BIT(Addr_TVal+510)==1);
}
/**
 * Counter edges and RST Cn / RST Tn.
 * First program: C0 counts the rising edges of M0 up to K2, M1 resets it, Y0 follows the
 * contact of C0. A steady rung counts once; releasing M0 re-arms the edge detector. RST also
 * clears a retentive timer (second program).
 */
static void test_counters_reset(void) {
  const u16 p[]={0x2000,                    /* LD M0 */
                 0x0E00,0x8002,0x8000,      /* OUT C0 K2 */
                 0x2001,0x000C,0x8E00,      /* LD M1, RST C0 */
                 0x01C2,0x8F00,0xCC00,      /* LD C0, OUT Y0 */
                 0x000F};                   /* END */
  load(p,WORDS(p)); PLC_BIT_ON(0); scan(); assert(PLC_16BIT(Addr_CVal)==1);
  scan(); assert(PLC_16BIT(Addr_CVal)==1);
  PLC_BIT_OFF(0); scan(); PLC_BIT_ON(0); scan();
  assert(PLC_16BIT(Addr_CVal)==2 && PLC_BIT_TEST(Addr_Y*8));
  PLC_BIT_ON(1); scan(); assert(PLC_16BIT(Addr_CVal)==0);
  assert(!PLC_BIT_TEST(Addr_CO*8));
  const u16 q[]={0x2000,                    /* LD M0 */
                 0x06F6,0x8005,0x8000,      /* OUT T246 K5 */
                 0x2001,0x000C,0x86F6,      /* LD M1, RST T246 */
                 0x000F};                   /* END */
  load(q,WORDS(q)); PLC_BIT_ON(0); scan(); tick(4); scan();
  PLC_BIT_OFF(0); PLC_BIT_ON(1); scan(); assert(PLC_16BIT(Addr_TVal+492)==0);
}
/**
 * MOV and INC with K constants and with registers of both D blocks.
 * D0 = K0x1234, INC D0, then the result is copied to D1999, the last register of the
 * second block (tag 88, index 999 relative to D1000).
 */
static void test_data(void) {
  const u16 p[]={0x01C2,0x8E00,                       /* LD M8000 */
                 0x0028,0x8034,0x8012,0x8600,0x8600,  /* MOV K4660 D0 (K is 80LL 80HH) */
                 0x0040,0x8600,0x8600,                /* INC D0 */
                 0x0028,0x8600,0x8600,0x86E7,0x8803,  /* MOV D0 D1999 */
                 0x000F};                             /* END */
  load(p,WORDS(p)); scan(); assert(PLC_16BIT(Addr_D0000)==0x1235);
  assert(PLC_16BIT(Addr_D0000+3998)==0x1235);
}
/**
 * Run a program that the runtime has to reject. The Y image is pre-filled with ones, so a
 * missing output clear is noticed. One scan must latch @p expected, leave the PLC stopped
 * with Y cleared and M8065 on, and must not have executed anything before the whole
 * program was validated: D0 stays zero (the first program writes D0 ahead of its bad
 * opcode, so an early start would show).
 */
static void bad(const u16 *p,size_t n,plc_runtime_error expected) {
  load(p,n); memset((void *)(uintptr_t)(RAM_ADDR+Addr_Y),0xFF,64); scan();
  assert(PLC_Runtime_LastError()==expected); stopped();
  assert(PLC_16BIT(Addr_D0000)==0); /* No partial execution before validation. */
  assert(PLC_BIT_TEST(M8065));
}
/**
 * Programs that must fault, one per error class. The comment after each program names the
 * defect; every one of them is detected by the validation pass before anything runs.
 */
static void test_faults(void) {
  /* 0038 (ADD) is not part of the subset; the fault points at its word offset, 7. */
  const u16 unsupported[]={0x01C2,0x8E00,0x0028,0x8001,0x8000,0x8600,0x8600,0x0038,0x000F};
  bad(unsupported,WORDS(unsupported),PLC_RUNTIME_OPCODE);
  assert(PLC_Runtime_ErrorStep()==7);
  /* OUT T0 with a D preset whose byte offset (8601) is odd. */
  const u16 operand[]={0x2000,0x0600,0x8601,0x8600,0x000F};
  bad(operand,WORDS(operand),PLC_RUNTIME_OPERAND);
  /* OUT C200: the 32-bit counters are not supported. */
  const u16 highcounter[]={0x2000,0x0EC8,0x8001,0x8000,0x000F};
  bad(highcounter,WORDS(highcounter),PLC_RUNTIME_OPERAND);
  /* OUT X0: inputs are read-only. */
  const u16 xwrite[]={0x2000,0x0002,0x9200,0x000F};
  bad(xwrite,WORDS(xwrite),PLC_RUNTIME_OPERAND);
  /* ANB with a single block on the stack. */
  const u16 stack[]={0x2000,0xFFF8,0x000F};
  bad(stack,WORDS(stack),PLC_RUNTIME_STACK);
  /* Ten LD in a row: more than the 8 logic blocks the stack can hold. */
  const u16 overflow[]={0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x000F};
  bad(overflow,WORDS(overflow),PLC_RUNTIME_STACK);
  /* Extended LD whose operand lacks the 8000 marker. */
  const u16 marker[]={0x01C2,0x4E00,0x000F};
  bad(marker,WORDS(marker),PLC_RUNTIME_OPERAND);
  /* INC D2000: index 1000 of the second block is beyond D1999. */
  const u16 outside[]={0x2000,0x0040,0x86E8,0x8803,0x000F};
  bad(outside,WORDS(outside),PLC_RUNTIME_OPERAND);
  /* A literal END inside a malformed operand is not an instruction END. */
  const u16 fake_end[]={0x2000,0x0600,0x8001,0x000F};
  bad(fake_end,WORDS(fake_end),PLC_RUNTIME_OPERAND);
  /* A program of MAXSTEP NOP words has no END at all. */
  const u16 noend[]={0}; load(noend,0);
  memset((void *)PLC_START_ADDR,0,MAXSTEP*2); scan();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_NO_END); stopped();
  /* MOV in the very last program word: its operands would lie beyond the program area. */
  ((u16 *)PLC_START_ADDR)[MAXSTEP-1]=0x0028;
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_Runtime_LastError()==PLC_RUNTIME_BOUNDS);
  /* Explicit RUN clears the latch and revalidates the repaired program. */
  ((u16 *)PLC_START_ADDR)[0]=0x000F; PLC_8BIT(Addr_M8)=0x09; scan();
  assert(!PLC_Runtime_LastError() && PLC_BIT_TEST(M8000));
  /* Bit accessors ignore addresses outside the image. */
  PLC_BIT_ON(PLC_RAM_SIZE*8U); PLC_BIT_OFF(0xFFFFFFFFU);
  assert(PLC_BIT_TEST(0xFFFFFFFFU)==0);
}

/**
 * Robustness: 1000 programs of 32 pseudo-random words followed by END (xorshift32 with a
 * fixed seed, so a failure is reproducible). Whatever the words mean, a scan has to either
 * run or latch a fault, and a fault has to leave the PLC stopped with the outputs cleared.
 * The sanitizers watch for any out-of-bounds access during validation and execution.
 */
static void test_malformed_random(void) {
  u32 seed=0x12345678U;
  const u16 empty[]={0};
  for (unsigned run=0;run<1000;run++) {
    load(empty,0);
    for (unsigned i=0;i<32;i++) {
      seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
      ((u16 *)PLC_START_ADDR)[i]=(u16)seed;
    }
    ((u16 *)PLC_START_ADDR)[32]=0x000F;
    scan();
    if (PLC_Runtime_LastError()) stopped();
  }
}
/**
 * RUN/STOP requests that arrive from an interrupt in the middle of a scan. The mock injects
 * the request at a chosen critical section (inject_at); no acknowledged request may be lost,
 * a STOP must win over everything that follows it, and the caller's interrupt mask must be
 * restored exactly. Program: LD M8000, OUT Y0, END.
 */
static void test_request_races(void) {
  const u16 p[]={0x01C2,0x8E00,           /* LD M8000 */
                 0xCC00,                  /* OUT Y0 */
                 0x000F};                 /* END */
  load(p,WORDS(p)); scan(); assert(PLC_BIT_TEST(Addr_Y*8));
  PLC_Runtime_RequestRun(0); assert(!PLC_Runtime_RunRequested());
  /* RUN arrives while main is clearing M8000 during STOP cleanup, not
   * before cleanup. It must remain in the mailbox for the next parser. */
  critical_exits=0; inject_at=2; injected_run=1;
  scan(); stopped(); assert(PLC_Runtime_RunRequested());
  scan(); assert(PLC_BIT_TEST(Addr_Y*8) && PLC_Runtime_RunRequested());
  /* STOP arriving immediately after atomic mailbox consumption cannot be
   * cleared by that exchange or by M8002/M8003 status RMWs. */
  critical_exits=0; inject_at=1; injected_run=0;
  scan(); stopped(); assert(!PLC_Runtime_RunRequested());
  scan(); stopped();
  PLC_Runtime_RequestRun(1); scan(); assert(PLC_BIT_TEST(Addr_Y*8));
  /* Take-request exit #1; first-scan M8002 status RMW exit #2. */
  PLC_Runtime_RequestRun(0); scan();
  PLC_Runtime_RequestRun(1); critical_exits=0; inject_at=4; injected_run=0;
  scan(); stopped(); assert(!PLC_Runtime_RunRequested());
  /* Preserve an already-masked caller state, do not spuriously enable IRQ. */
  primask=1; PLC_Runtime_RequestRun(1); scan(); assert(primask==1);
  primask=0;
}
int main(void) {
  /* Pulse bitmap + PLC image (page-aligned) and the program flash area. */
  map(PLC_STEPS_BASE & ~4095U,0x4000U); map(ROM_ADDR,0x4000U);
  test_start_stop(); test_clock_sample(); test_mapping_logic(); test_stacks();
  test_pulse(); test_timers(); test_counters_reset(); test_data(); test_faults();
  test_malformed_random();
  test_request_races();
  printf("runtime: %u complete bytecode scans passed (ASan/UBSan)\n",scans);
  return 0;
}
