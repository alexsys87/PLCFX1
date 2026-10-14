/**
 * @file plc_rtc.c
 * @brief Register-level driver for the STM32F103 real-time clock and the FX clock registers.
 *
 * The RTC counts seconds since 2016-01-01 00:00:00 (valid until 2136). RTC_Get() converts the
 * counter into D8013..D8019 (second, minute, hour, day, month, year, weekday); RTC_Set()
 * converts a calendar date back into the counter. Relays used:
 *   M8018  ON while a working RTC is present (cleared when the LSE fails)
 *   M8015  when ON, RTC_Get() writes D8013..D8018 into the RTC instead of reading it
 *
 * The BluePill must carry a working 32.768 kHz LSE crystal; an inaccurate LSI fallback is
 * deliberately not used. The polling budgets below are iteration counts, not milliseconds;
 * they allow several seconds for crystal start-up at the normal 72 MHz CPU clock.
 *
 * Calendar routines originate from the "Chuan Ren Ji" PLC code (Xiao Xiaosheng, 2014).
 */
#include "plc_rtc.h"
#include "plc_memory_map.h"
#include "plc_special_devices.h"

extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

#ifndef RTC_LSE_WAIT_LIMIT
#define RTC_LSE_WAIT_LIMIT 72000000UL      /* LSE start-up budget (loop iterations) */
#endif
#ifndef RTC_WAIT_LIMIT
#define RTC_WAIT_LIMIT 1000000UL           /* register synchronisation budget (loop iterations) */
#endif
#define RTC_BACKUP_MAGIC 0x5A5AU           /* marker in BKP->DR1: the retained clock is valid */

static u8 rtc_available;                   /* 1 after a successful RTC_Init() */

/** Poll @p reg until every bit of @p mask is set; gives up after @p limit iterations (not ms). */
static u8 rtc_wait(volatile u32 *reg, u32 mask, u32 limit) {
  while (limit-- != 0U) {
    if ((*reg & mask) == mask)
      return 1;
  }
  return 0;
}

/** Mark the clock unusable and clear the "RTC present" relay M8018. */
static void rtc_unavailable(void) {
  rtc_available = 0;
  PLC_BIT_OFF(M8018);
}

/** True while the LSE oscillator runs and drives the RTC (LSE ready, RTC enabled, LSE selected). */
static u8 rtc_clock_running(void) {
  return (RCC->BDCR & (RCC_BDCR_LSERDY | RCC_BDCR_RTCEN |
                      RCC_BDCR_RTCSEL)) ==
         (RCC_BDCR_LSERDY | RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE);
}

/** Wait for the APB shadow registers to resynchronise with the RTC core. */
static u8 rtc_sync(void) {
  RTC->CRL &= ~RTC_CRL_RSF;
  return rtc_wait(&RTC->CRL, RTC_CRL_RSF, RTC_WAIT_LIMIT);
}

/** Days per month of a common year. */
const u8 mon_table[12]={31,28,31,30,31,30,31,31,30,31,30,31};

/** Gregorian leap-year test: divisible by 4, except centuries that are not divisible by 400. */
u8 Is_Leap_Year(u16 year) {

  if ( year%4 == 0 ) {
    if ( year%100 == 0 ) {
      if ( year%400 == 0 )
        return 1;
      else
        return 0;
     } else
       return 1;
  } else
    return 0;
}

/**
 * Set the clock.
 * Valid dates are 2016-01-01 .. 2136-12-31. Returns 0 on success, 1 for an invalid
 * date/time and 2 when the RTC is unavailable or a register write timed out.
 */
u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec) {
u16 t;
u32 seccount = 0;
u32 backup_access;
u8 days;

  if (syear < 2016 || syear > 2136 || smon < 1 || smon > 12 ||
      hour > 23 || min > 59 || sec > 59)
    return 1;
  days = mon_table[smon - 1];
  if (smon == 2 && Is_Leap_Year(syear))
    days++;
  if (sday < 1 || sday > days)
    return 1;
  if (!rtc_available || !rtc_clock_running()) {
    rtc_unavailable();
    return 2;
  }
  for ( t = 2016; t < syear; t++ ) {
    if ( Is_Leap_Year(t) )
      seccount += 31622400;
    else
      seccount += 31536000;
  }
  smon -= 1;
  for ( t = 0; t < smon; t++) {
    seccount += (u32)mon_table[t] * 86400;
    if ( Is_Leap_Year(syear) && t == 1 )
      seccount += 86400;
  }
  seccount += (u32)(sday - 1) * 86400;
  seccount += (u32)hour * 3600;
  seccount += (u32)min * 60;
  seccount += sec;

  backup_access = PWR->CR & PWR_CR_DBP;
  PWR->CR |= PWR_CR_DBP;
  if (!rtc_wait(&RTC->CRL, RTC_CRL_RTOFF, RTC_WAIT_LIMIT))
    goto write_failed;
  RTC->CRL |= RTC_CRL_CNF;    /* enter configuration mode */
  RTC->CNTH = seccount >> 16;   /* write the new counter value */
  RTC->CNTL = seccount & 0xFFFFU;
  RTC->CRL &= ~RTC_CRL_CNF;   /* leave configuration mode */
  if (!rtc_wait(&RTC->CRL, RTC_CRL_RTOFF, RTC_WAIT_LIMIT))
    goto write_failed;
  if (!backup_access)
    PWR->CR &= ~PWR_CR_DBP;
  return 0;

write_failed:
  if (!backup_access)
    PWR->CR &= ~PWR_CR_DBP;
  rtc_unavailable();
  return 2;
}

/** Month offsets of the day-of-week formula in RTC_Get_Week(), indexed by month (entry 0 is unused). */
const u8 week_tab[] ={0,1,4,4,0,2,5,0,3,6,1,4,6};

/**
 * Day of the week of a Gregorian date, 0 = Sunday.
 *
 * The result is (year + year/4 + year/400 - year/100 + month offset + day - 2) mod 7, where all
 * divisions are integer quotients and the day is reduced by one for January and February of a
 * leap year. A month outside 1..12 or a day outside 1..31 gives 0.
 */
u8 RTC_Get_Week(u16 Year,u8 Month,u8 Date) {
 if (Month < 1 || Month > 12 || Date < 1 || Date > 31)
   return 0;
 if (Month < 3 && Is_Leap_Year(Year))
   Date--;
 return ( Date + Year + Year/4 + Year/400 -Year/100 + week_tab[Month]-2 ) % 7;
}

/**
 * Refresh the PLC calendar registers from the RTC (or write them, see M8015).
 *
 * With M8015 ON the registers D8013..D8018 are validated and written into the RTC. Otherwise,
 * when an RTC is present (M8018), the counter is converted to year/month/day/hour/minute/second
 * and the weekday; the 32-bit counter is read twice to avoid a torn read at a rollover.
 */
void RTC_Get(void) {
u32 timecount, temp, high, low;
u16 year;
u8 month, days, attempt;

  if ( PLC_BIT_TEST(M8015) ) {
    u16 set_year, set_month, set_day, set_hour, set_minute, set_second;
    /* Read each volatile PLC word in a separate full expression. */
    set_year = D8018;
    set_month = D8017;
    set_day = D8016;
    set_hour = D8015;
    set_minute = D8014;
    set_second = D8013;
    /* Check the full PLC words before narrowing them to the public u8 API. */
    if (set_month >= 1 && set_month <= 12 && set_day >= 1 && set_day <= 31 &&
        set_hour <= 23 && set_minute <= 59 && set_second <= 59)
      RTC_Set(set_year, set_month, set_day, set_hour, set_minute, set_second);
  } else if (rtc_available && PLC_BIT_TEST(M8018)) {
    if (!rtc_clock_running()) {
      rtc_unavailable();
      return;
    }
    /* Avoid a torn read when the low half rolls over between APB reads. */
    for (attempt = 0; attempt < 4; attempt++) {
      high = RTC->CNTH & 0xFFFFU;
      low = RTC->CNTL & 0xFFFFU;
      if (high == (RTC->CNTH & 0xFFFFU))
        break;
    }
    if (attempt == 4) {
      rtc_unavailable();
      return;
    }
    timecount = (high << 16) | low;
    temp = timecount / 86400;
    year = 2016;
    while (temp >= (u32)(Is_Leap_Year(year) ? 366 : 365)) {
      temp -= Is_Leap_Year(year) ? 366U : 365U;
      year++;
    }
    month = 1;
    for (;;) {
      days = mon_table[month - 1];
      if (month == 2 && Is_Leap_Year(year))
        days++;
      if (temp < days)
        break;
      temp -= days;
      month++;
    }
    D8018 = year;
    D8017 = month;
    days = (u8)(temp + 1U);
    D8016 = days;
    temp = timecount % 86400;
    D8015 = temp / 3600;
    D8014 = (temp % 3600) / 60;
    D8013 = (temp % 3600) % 60;
    D8019 = RTC_Get_Week(year, month, days);
  }
}

/**
 * Start the RTC. A retained, valid clock (marker in BKP->DR1, LSE selected) is kept; any other
 * backup-domain state is reset and the counter restarts at 2016-01-01 00:00:00. On any timeout
 * the clock stays unavailable (M8018 off).
 */
void RTC_Init(void) {
u32 backup_access;
u8 retained;

  rtc_unavailable();
  PLC_BIT_OFF(M8015);
  RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
  (void)RCC->APB1ENR;          /* read back so the clocks are running before use */
  backup_access = PWR->CR & PWR_CR_DBP;
  PWR->CR |= PWR_CR_DBP;
  (void)PWR->CR;

  retained = BKP->DR1 == RTC_BACKUP_MAGIC &&
             (RCC->BDCR & RCC_BDCR_RTCSEL) == RCC_BDCR_RTCSEL_LSE &&
             !(RCC->BDCR & RCC_BDCR_LSEBYP);
  if (!retained) {
    /* Only an uninitialized/incompatible backup domain may be reset. */
    RCC->BDCR |= RCC_BDCR_BDRST;
    RCC->BDCR &= ~RCC_BDCR_BDRST;
  }
  RCC->BDCR |= RCC_BDCR_LSEON;
  if (!rtc_wait(&RCC->BDCR, RCC_BDCR_LSERDY, RTC_LSE_WAIT_LIMIT))
    goto init_done;
  if (!retained)
    RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | RCC_BDCR_RTCSEL_LSE;
  RCC->BDCR |= RCC_BDCR_RTCEN;
  /* APB shadow registers must be synchronized on every reset, including
   * when the battery-backed counter and marker have been retained. */
  if (!rtc_sync() || !rtc_wait(&RTC->CRL, RTC_CRL_RTOFF, RTC_WAIT_LIMIT))
    goto init_done;
  if (!retained) {
    RTC->CRL |= RTC_CRL_CNF;
    RTC->PRLH = 0;
    RTC->PRLL = 32767U;
    RTC->CNTH = 0;             /* 2016-01-01 00:00:00 */
    RTC->CNTL = 0;
    RTC->CRL &= ~RTC_CRL_CNF;
    if (!rtc_wait(&RTC->CRL, RTC_CRL_RTOFF, RTC_WAIT_LIMIT))
      goto init_done;
  }
  /* Do not overwrite a retained clock with an unexpected prescaler. */
  if ((RTC->PRLH & 0xFU) != 0 || (RTC->PRLL & 0xFFFFU) != 32767U)
    goto init_done;
  BKP->DR1 = RTC_BACKUP_MAGIC; /* mark valid only after setup has succeeded */
  rtc_available = 1;
  PLC_BIT_ON(M8018);

init_done:
  if (!backup_access)
    PWR->CR &= ~PWR_CR_DBP;
}
