
#include "main.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef RTC
#undef RCC
#undef PWR
#undef BKP
static RTC_TypeDef mock_rtc;
static RCC_TypeDef mock_rcc;
static PWR_TypeDef mock_pwr;
static BKP_TypeDef mock_bkp;
#define RTC (&mock_rtc)
#define RCC (&mock_rcc)
#define PWR (&mock_pwr)
#define BKP (&mock_bkp)
#define RTC_LSE_WAIT_LIMIT 32U
#define RTC_WAIT_LIMIT 32U
volatile u16 plc_dates[7];
static u32 plc_bits;
static int fail_lse, fail_sync, fail_write;
static int fail_write_after, write_waits;
u32 PLC_BIT_TEST(u32 addr) { return (plc_bits >> addr) & 1; }
void PLC_BIT_ON(u32 addr) { plc_bits |= 1U << addr; }
void PLC_BIT_OFF(u32 addr) { plc_bits &= ~(1U << addr); }
/* Model status transitions only; production loops are separately ARM-compiled. */
static void status_begin(volatile u32 *reg, u32 mask) {
  if (reg == &RTC->CRL && mask == RTC_CRL_RTOFF) {
    write_waits++;
    if (fail_write_after && write_waits >= fail_write_after) {
      fail_write=1;
      *reg &= ~RTC_CRL_RTOFF;
    }
  }
}
static void status_tick(volatile u32 *reg, u32 mask) {
  if (reg == &RCC->BDCR && mask == RCC_BDCR_LSERDY && !fail_lse)
    *reg |= mask;
  if (reg == &RTC->CRL && (RCC->BDCR & RCC_BDCR_RTCEN)) {
    if (mask == RTC_CRL_RSF && !fail_sync) *reg |= mask;
    if (mask == RTC_CRL_RTOFF && !fail_write) *reg |= mask;
  }
}
