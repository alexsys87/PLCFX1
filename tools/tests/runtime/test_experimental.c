/* Selected executable-handler smoke tests, not full legacy validation. */
#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#define PLC_RUNTIME_EXPERIMENTAL 1
#include "../../../src/ladder.c"

volatile u8 PLC_UPDATE_PROG;
volatile u16 PLC_RUN_TIME;
u16 X_DIY;
static u32 primask;
u32 __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask=1; }
void __set_PRIMASK(u32 value) { assert(primask==1); primask=value; }
void RTC_Set(u16 year,u8 month,u8 day,u8 hour,u8 minute,u8 second) {
  (void)year;(void)month;(void)day;(void)hour;(void)minute;(void)second;
}
static void map(u32 base,u32 size) {
  assert(mmap((void *)(uintptr_t)base,size,PROT_READ|PROT_WRITE,
    MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)==(void *)(uintptr_t)base);
}
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static void load(const u16 *words,size_t count) {
  memset((void *)(uintptr_t)PLC_RAM_BASE,0,PLC_RAM_SIZE);
  memset((void *)(uintptr_t)ROM_ADDR,0,14U*1024U);
  memcpy((void *)PLC_START_ADDR,words,count*2U);
  PLC_NEW_PROG=PLC_UPDATE_PROG=0; pending_run_request=REQUEST_NONE;
  PLC_Runtime_Init(); PLC_Runtime_RequestRun(1);
}
static void stopped(void) {
  assert(!PLC_BIT_TEST(M8000) && PLC_BIT_TEST(M8001));
  for (unsigned i=0;i<64U;++i) assert(PLC_8BIT(Addr_Y+i)==0);
}
static void good_scan(void) {
  PLC_Main_Parser();
  if (PLC_Runtime_LastError()) fprintf(stderr,"Fault %u at step %u\n",
    (unsigned)PLC_Runtime_LastError(),(unsigned)PLC_Runtime_ErrorStep());
  assert(PLC_Runtime_LastError()==0);
}
int main(void) {
  map(0x20001000,0x1000); map(PLC_RAM_BASE,PLC_RAM_SIZE); map(ROM_ADDR,0x4000);
  /* Actual words selected from restored source, not official FNC indexes. */
  const u16 arithmetic[]={0x01C2,0x8E00,
    0x0028,0x8005,0x8000,0x8600,0x8600, /* MOV K5 D0 */
    0x0038,0x8600,0x8600,0x8003,0x8000,0x8601,0x8600, /* ADD D0 K3 D1 */
    0x0042,0x8601,0x8600, /* DEC D1 */
    0x0024,0x8003,0x8000,0x8003,0x8000,0x8400,0x8000, /* CMP K3 K3 M0 */
    0xCC00,0x000F};
  load(arithmetic,COUNT(arithmetic)); good_scan();
  assert(PLC_16BIT(Addr_D0000)==5 && PLC_16BIT(Addr_D0000+2)==7);
  assert(PLC_BIT_TEST(Addr_Y*8U));
  assert(!PLC_BIT_TEST(0) && PLC_BIT_TEST(1) && !PLC_BIT_TEST(2));
  PLC_Runtime_RequestRun(0); PLC_Main_Parser(); stopped();
  PLC_Runtime_RequestRun(1); good_scan();
  PLC_UPDATE_PROG=1; PLC_Main_Parser(); stopped();
  /* Extended X operand must not alias M0 by losing address bit 12. */
  const u16 contact[]={0x01C2,0x9200,0xCC00,0x000F};
  load(contact,COUNT(contact)); PLC_BIT_ON(Addr_X*8U); good_scan();
  assert(PLC_BIT_TEST(Addr_Y*8U) && !PLC_BIT_TEST(0));
  const u16 pulses[]={0x2000,0x0008,0x8C00,0x0009,0x8C01,0x000F};
  load(pulses,COUNT(pulses)); good_scan();
  assert(!PLC_BIT_TEST(Addr_Y*8U) && !PLC_BIT_TEST(Addr_Y*8U+1));
  PLC_BIT_ON(0); good_scan(); assert(PLC_BIT_TEST(Addr_Y*8U));
  good_scan(); assert(!PLC_BIT_TEST(Addr_Y*8U));
  PLC_BIT_OFF(0); good_scan(); assert(PLC_BIT_TEST(Addr_Y*8U+1));
  good_scan(); assert(!PLC_BIT_TEST(Addr_Y*8U+1));
  const u16 pop[]={0xFFFF,0x000F};
  load(pop,COUNT(pop)); PLC_Main_Parser(); stopped();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_LEGACY_INCOMPLETE);
  const u16 jump_loop[]={0xB000,0x01C2,0x8E00,0x0010,0x8800,0x8000,0x000F};
  load(jump_loop,COUNT(jump_loop)); PLC_Main_Parser(); stopped();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_LEGACY_BUDGET);
  const u16 missing[]={0x01C2,0x8E00,0xCC00,0x0109,0x000F}; /* DEXP absent in PR2 */
  load(missing,COUNT(missing)); PLC_Main_Parser(); stopped();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_LEGACY_MISSING && PLC_Runtime_ErrorStep()==3);
  const u16 incomplete[]={0x01C2,0x8E00,0x0074,0x000F}; /* REF original empty */
  load(incomplete,COUNT(incomplete)); PLC_Main_Parser(); stopped();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_LEGACY_INCOMPLETE);
  const u16 unsupported[]={0x001E,0x000F}; /* WDT original explicit error */
  load(unsupported,COUNT(unsupported)); PLC_Main_Parser(); stopped();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_LEGACY_UNSUPPORTED);
  const u16 bad_address[]={0x01C2,0x8E00,0x0028,0x8001,0x8000,0x86FF,0x86FF,0x000F};
  load(bad_address,COUNT(bad_address)); PLC_Main_Parser(); stopped();
  assert(PLC_Runtime_LastError()==PLC_RUNTIME_BOUNDS);
  const u16 only_end[]={0x000F}; load(only_end,COUNT(only_end)); good_scan();
  memset((void *)PLC_START_ADDR,0,MAXSTEP*2U);
  PLC_Main_Parser(); stopped(); assert(PLC_Runtime_LastError()==PLC_RUNTIME_BOUNDS);
  puts("PASS: experimental MOV/ADD/DEC/CMP, mapping, STOP/update, missing/empty/error and bounds smoke fixtures.");
  return 0;
}
