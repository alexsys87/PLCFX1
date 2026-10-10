#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "../../../src/ladder.c"

volatile u8 PLC_UPDATE_PROG;
static u32 primask;
static unsigned critical_exits, inject_at;
static u8 injected_run;
u32 __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask=1; }
void __set_PRIMASK(u32 value) {
  assert(primask==1);
  primask=value;
  ++critical_exits;
  if (!value && inject_at && critical_exits==inject_at) {
    inject_at=0;
    PLC_Runtime_RequestRun(injected_run); /* Simulated USART ISR. */
  }
}
static unsigned scans;
#define WORDS(a) (sizeof(a)/sizeof((a)[0]))
static void map(u32 base, u32 size) {
  assert(mmap((void *)(uintptr_t)base,size,PROT_READ|PROT_WRITE,
              MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0) == (void *)(uintptr_t)base);
}
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
static void scan(void) { PLC_Main_Parser(); ++scans; }
static void tick(unsigned n) { while (n--) PLC_Runtime_Tick1ms(); }
static void stopped(void) {
  assert(!PLC_BIT_TEST(M8000) && PLC_BIT_TEST(M8001));
  for (unsigned i=0;i<64;i++) assert(PLC_8BIT(Addr_Y+i)==0);
}
static void test_start_stop(void) {
  const u16 p[] = {0x01C2,0x8E00,0xC000,0x01C2,0x8E02,0xC001,0x000F};
  load(p,WORDS(p)); scan(); assert(PLC_BIT_TEST(0) && PLC_BIT_TEST(1));
  scan(); assert(PLC_BIT_TEST(0) && !PLC_BIT_TEST(1));
  PLC_8BIT(Addr_M8) = 0x0A; PLC_BIT_ON(Addr_Y*8U+511); scan(); stopped();
  PLC_8BIT(Addr_M8) = 0x09; scan(); assert(PLC_BIT_TEST(1));
  PLC_UPDATE_PROG=1; scan(); stopped();
  /* Extra bits in the M8 byte must not spuriously cancel RUN. */
  PLC_UPDATE_PROG=0; PLC_8BIT(Addr_M8)=0x39; scan(); assert(PLC_BIT_TEST(0));
}
static void test_clock_sample(void) {
  const u16 p[]={0x01C2,0x8E0D,0xCC00,0x000F};
  load(p,WORDS(p)); scan(); assert(!PLC_BIT_TEST(Addr_Y*8));
  tick(500); scan(); assert(PLC_BIT_TEST(Addr_Y*8));
  tick(500); scan(); assert(!PLC_BIT_TEST(Addr_Y*8));
  tick(5); scan(); assert(PLC_BIT_TEST(M8011));
  tick(45); scan(); assert(PLC_BIT_TEST(M8012));
  tick(28950); scan(); assert(PLC_BIT_TEST(M8014));
  tick(30000); scan(); assert(!PLC_BIT_TEST(M8014));
}
static void test_mapping_logic(void) {
  const u16 p[]={0x01C2,0x9200,0x01C4,0x9400,0x01C6,0x9000,
                 0xCC00,0x01C3,0x8FC7,0xCC01,0x2EFF,0xCC02,0x000F};
  load(p,WORDS(p)); PLC_BIT_ON(Addr_X*8); PLC_BIT_ON(Addr_S*8);
  PLC_BIT_ON(Addr_M8*8+255); scan();
  assert(PLC_Runtime_LastError()==0 && PLC_BIT_TEST(Addr_Y*8));
  assert(PLC_BIT_TEST(Addr_Y*8+1) && PLC_BIT_TEST(Addr_Y*8+2));
  assert(PLC_BIT_TEST(Addr_S*8) && !PLC_BIT_TEST(0));
  /* Six basic contact families and SET/RST in a complete scan. */
  const u16 q[]={0x2000,0x4001,0x5002,0x6003,0x7004,0xD005,
                 0x3006,0xE007,0x000F};
  load(q,WORDS(q)); PLC_BIT_ON(0); PLC_BIT_ON(1); PLC_BIT_ON(7); scan();
  assert(PLC_BIT_TEST(5) && !PLC_BIT_TEST(7));
  /* S4095 and Y511 use extended output operands. */
  const u16 r[]={0x01C2,0x8E00,0x0002,0xA3FF,0x0002,0x8DFF,0x000F};
  load(r,WORDS(r)); scan(); assert(PLC_BIT_TEST(Addr_S*8+4095));
  assert(PLC_BIT_TEST(Addr_Y*8+511));
}
static void test_stacks(void) {
  const u16 p[]={0x2000,0x2001,0xFFF9,0x2002,0xFFF8,0xFFFA,
                 0xCC00,0xFFFB,0x5003,0xCC01,0xFFFC,0xFFFD,0xCC02,0x000F};
  load(p,WORDS(p)); PLC_BIT_ON(1); PLC_BIT_ON(2); scan();
  assert(!PLC_Runtime_LastError());
  assert(PLC_BIT_TEST(Addr_Y*8) && PLC_BIT_TEST(Addr_Y*8+1));
  assert(!PLC_BIT_TEST(Addr_Y*8+2));
}
static void test_pulse(void) {
  const u16 p[]={0x2000,0x0008,0x8C00,0x0009,0x8C01,0x000F};
  load(p,WORDS(p)); scan(); assert(!PLC_BIT_TEST(Addr_Y*8+1));
  PLC_BIT_ON(0); scan(); assert(PLC_BIT_TEST(Addr_Y*8));
  scan(); assert(!PLC_BIT_TEST(Addr_Y*8));
  PLC_BIT_OFF(0); scan(); assert(PLC_BIT_TEST(Addr_Y*8+1));
  scan(); assert(!PLC_BIT_TEST(Addr_Y*8+1));
  PLC_BIT_ON(0); scan(); PLC_8BIT(Addr_M8)=0x0A; scan(); stopped();
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_BIT_TEST(Addr_Y*8));
}
static void test_timers(void) {
  const u16 p[]={0x2000,0x0600,0x8002,0x8000,0x06C8,0x8002,0x8000,
                 0x06F6,0x8002,0x8000,0x06FA,0x8002,0x8000,
                 0x01C2,0x9000,0xCC00,0x000F};
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
  const u16 q[]={0x2000,0x0601,0x8602,0x8600,0x000F};
  load(q,WORDS(q)); PLC_16BIT(Addr_D0000+2)=3; PLC_BIT_ON(0); scan();
  tick(300); scan(); assert(PLC_16BIT(Addr_TVal+2)==3);
  PLC_8BIT(Addr_M8)=0x0A; scan(); tick(1000);
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_16BIT(Addr_TVal+2)==0);
  const u16 r[]={0x2000,0x06FF,0x8001,0x8000,0x0EC7,0x8001,0x8000,
                 0x01C2,0x90FF,0xCC00,0x01C2,0x8FC7,0xCC01,0x000F};
  load(r,WORDS(r)); PLC_BIT_ON(0); scan(); tick(100); scan();
  assert(PLC_BIT_TEST(Addr_Y*8) && PLC_BIT_TEST(Addr_Y*8+1));
  assert(PLC_16BIT(Addr_TVal+510)==1 && PLC_16BIT(Addr_CVal+398)==1);
  PLC_8BIT(Addr_M8)=0x0A; scan(); tick(1000);
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_16BIT(Addr_TVal+510)==1);
}
static void test_counters_reset(void) {
  const u16 p[]={0x2000,0x0E00,0x8002,0x8000,0x2001,0x000C,0x8E00,
                 0x01C2,0x8F00,0xCC00,0x000F};
  load(p,WORDS(p)); PLC_BIT_ON(0); scan(); assert(PLC_16BIT(Addr_CVal)==1);
  scan(); assert(PLC_16BIT(Addr_CVal)==1);
  PLC_BIT_OFF(0); scan(); PLC_BIT_ON(0); scan();
  assert(PLC_16BIT(Addr_CVal)==2 && PLC_BIT_TEST(Addr_Y*8));
  PLC_BIT_ON(1); scan(); assert(PLC_16BIT(Addr_CVal)==0);
  assert(!PLC_BIT_TEST(Addr_CO*8));
  const u16 q[]={0x2000,0x06F6,0x8005,0x8000,0x2001,0x000C,0x86F6,0x000F};
  load(q,WORDS(q)); PLC_BIT_ON(0); scan(); tick(4); scan();
  PLC_BIT_OFF(0); PLC_BIT_ON(1); scan(); assert(PLC_16BIT(Addr_TVal+492)==0);
}
static void test_data(void) {
  const u16 p[]={0x01C2,0x8E00,0x0028,0x8034,0x8012,0x8600,0x8600,
                 0x0040,0x8600,0x8600,0x0028,0x8600,0x8600,0x86E7,0x8803,0x000F};
  load(p,WORDS(p)); scan(); assert(PLC_16BIT(Addr_D0000)==0x1235);
  assert(PLC_16BIT(Addr_D0000+3998)==0x1235);
}
static void bad(const u16 *p,size_t n,plc_runtime_error expected) {
  load(p,n); memset((void *)(uintptr_t)(RAM_ADDR+Addr_Y),0xFF,64); scan();
  assert(PLC_Runtime_LastError()==expected); stopped();
  assert(PLC_16BIT(Addr_D0000)==0); /* No partial execution before validation. */
  assert(PLC_BIT_TEST(M8065));
}
static void test_faults(void) {
  const u16 unsupported[]={0x01C2,0x8E00,0x0028,0x8001,0x8000,0x8600,0x8600,0x0038,0x000F};
  bad(unsupported,WORDS(unsupported),PLC_RUNTIME_OPCODE);
  assert(PLC_Runtime_ErrorStep()==7);
  const u16 operand[]={0x2000,0x0600,0x8601,0x8600,0x000F};
  bad(operand,WORDS(operand),PLC_RUNTIME_OPERAND);
  const u16 highcounter[]={0x2000,0x0EC8,0x8001,0x8000,0x000F};
  bad(highcounter,WORDS(highcounter),PLC_RUNTIME_OPERAND);
  const u16 xwrite[]={0x2000,0x0002,0x9200,0x000F};
  bad(xwrite,WORDS(xwrite),PLC_RUNTIME_OPERAND);
  const u16 stack[]={0x2000,0xFFF8,0x000F};
  bad(stack,WORDS(stack),PLC_RUNTIME_STACK);
  const u16 overflow[]={0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x2000,0x000F};
  bad(overflow,WORDS(overflow),PLC_RUNTIME_STACK);
  const u16 marker[]={0x01C2,0x4E00,0x000F};
  bad(marker,WORDS(marker),PLC_RUNTIME_OPERAND);
  const u16 outside[]={0x2000,0x0040,0x86E8,0x8803,0x000F};
  bad(outside,WORDS(outside),PLC_RUNTIME_OPERAND);
  /* A literal END inside a malformed operand is not an instruction END. */
  const u16 fake_end[]={0x2000,0x0600,0x8001,0x000F};
  bad(fake_end,WORDS(fake_end),PLC_RUNTIME_OPERAND);
  const u16 noend[]={0}; load(noend,0);
  memset((void *)PLC_START_ADDR,0,MAXSTEP*2); scan();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_NO_END); stopped();
  ((u16 *)PLC_START_ADDR)[MAXSTEP-1]=0x0028;
  PLC_8BIT(Addr_M8)=0x09; scan(); assert(PLC_Runtime_LastError()==PLC_RUNTIME_BOUNDS);
  /* Explicit RUN clears the latch and revalidates the repaired program. */
  ((u16 *)PLC_START_ADDR)[0]=0x000F; PLC_8BIT(Addr_M8)=0x09; scan();
  assert(!PLC_Runtime_LastError() && PLC_BIT_TEST(M8000));
  PLC_BIT_ON(PLC_RAM_SIZE*8U); PLC_BIT_OFF(0xFFFFFFFFU);
  assert(PLC_BIT_TEST(0xFFFFFFFFU)==0);
}
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
static void test_request_races(void) {
  const u16 p[]={0x01C2,0x8E00,0xCC00,0x000F};
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
  map(PLC_STEPS_BASE & ~4095U,0x4000U); map(ROM_ADDR,0x4000U);
  test_start_stop(); test_clock_sample(); test_mapping_logic(); test_stacks();
  test_pulse(); test_timers(); test_counters_reset(); test_data(); test_faults();
  test_malformed_random();
  test_request_races();
  printf("runtime: %u complete bytecode scans passed (ASan/UBSan)\n",scans);
  return 0;
}
