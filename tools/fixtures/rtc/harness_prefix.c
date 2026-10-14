/**
 * @file harness_prefix.c
 * @brief First part of the RTC test program: mock peripherals, mock relays and status injection.
 *
 * tools/test_rtc.py concatenates harness_prefix.c, the production src/plc_rtc.c (without its
 * #include lines) and harness_tests.c into one translation unit, so the driver is compiled against
 * the mocks below and the tests can reach its static state (rtc_available). The runner also inserts
 * calls to status_begin() and status_tick() into the driver's polling loop rtc_wait() in a temporary
 * copy; the production file is not modified. The mocks model the register contents and the
 * status-bit transitions, not hardware timing.
 */

#include "mock_environment.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* The CMSIS device header points RTC, RCC, PWR and BKP at the real peripherals. Replace them by
 * ordinary variables of the same types, so that the driver's register accesses go to RAM. */
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
/* Tiny polling budgets, so that a timeout is reached after a few iterations (the driver only
 * defines its long defaults when these are absent). */
#define RTC_LSE_WAIT_LIMIT 32U
#define RTC_WAIT_LIMIT 32U
/* Backing store of D8013..D8019 (see mock_environment.h). */
volatile u16 plc_dates[7];
/* Mock relay store: bit n of this word is the relay with the number n. */
static u32 plc_bits;
/* Failure switches. fail_lse: the LSE oscillator never becomes ready. fail_sync: the RTC shadow
 * registers never synchronise (RSF stays clear). fail_write: RTOFF stays clear, so every write to
 * the RTC times out. */
static int fail_lse, fail_sync, fail_write;
/* fail_write_after = n makes the n-th and every later wait for RTOFF time out; write_waits counts
 * those waits since the last reset. */
static int fail_write_after, write_waits;
/* Mock of the relay accessors that the driver imports from the PLC image. */
u32 PLC_BIT_TEST(u32 addr) { return (plc_bits >> addr) & 1; }
void PLC_BIT_ON(u32 addr) { plc_bits |= 1U << addr; }
void PLC_BIT_OFF(u32 addr) { plc_bits &= ~(1U << addr); }
/**
 * Injected before every rtc_wait(): counts the waits for RTOFF and, from the fail_write_after-th on,
 * turns the write failure on and clears RTOFF so that the wait cannot succeed. Models only the status
 * transitions; the production loops are compiled separately for the ARM target.
 */
static void status_begin(volatile u32 *reg, u32 mask) {
  if (reg == &RTC->CRL && mask == RTC_CRL_RTOFF) {
    write_waits++;
    if (fail_write_after && write_waits >= fail_write_after) {
      fail_write=1;
      *reg &= ~RTC_CRL_RTOFF;
    }
  }
}
/**
 * Injected into every iteration of rtc_wait(): the status bit being polled is set the way the
 * hardware would set it - LSERDY once the oscillator has started, RSF and RTOFF only while the RTC is
 * enabled - unless the matching failure switch is on.
 */
static void status_tick(volatile u32 *reg, u32 mask) {
  if (reg == &RCC->BDCR && mask == RCC_BDCR_LSERDY && !fail_lse)
    *reg |= mask;
  if (reg == &RTC->CRL && (RCC->BDCR & RCC_BDCR_RTCEN)) {
    if (mask == RTC_CRL_RSF && !fail_sync) *reg |= mask;
    if (mask == RTC_CRL_RTOFF && !fail_write) *reg |= mask;
  }
}
