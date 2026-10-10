#include <main.h>
#include "PLC_Dialogue.h"
#include "PLC_CONF.H"

extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

/* Register-only STM32F103 RTC. The BluePill must have a working 32.768 kHz
 * LSE crystal; an inaccurate LSI fallback is deliberately not used.
 * These bounded polling budgets are iteration counts, not milliseconds.
 * Allow several seconds for crystal startup at the normal 72 MHz CPU clock.
 */
#ifndef RTC_LSE_WAIT_LIMIT
#define RTC_LSE_WAIT_LIMIT 72000000UL
#endif
#ifndef RTC_WAIT_LIMIT
#define RTC_WAIT_LIMIT 1000000UL
#endif
#define RTC_BACKUP_MAGIC 0x5A5AU

static u8 rtc_available;

static u8 rtc_wait(volatile u32 *reg, u32 mask, u32 limit) {
  while (limit-- != 0U) {
    if ((*reg & mask) == mask)
      return 1;
  }
  return 0;
}

static void rtc_unavailable(void) {
  rtc_available = 0;
  PLC_BIT_OFF(M8018);
}

static u8 rtc_clock_running(void) {
  return (RCC->BDCR & (RCC_BDCR_LSERDY | RCC_BDCR_RTCEN |
                      RCC_BDCR_RTCSEL)) ==
         (RCC_BDCR_LSERDY | RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE);
}

static u8 rtc_sync(void) {
  RTC->CRL &= ~RTC_CRL_RSF;
  return rtc_wait(&RTC->CRL, RTC_CRL_RSF, RTC_WAIT_LIMIT);
}

//=======================================================================================================
// Function name: u8 Is_Leap_Year(u16 year)
// Description:  Determine whether the year is a leap year
// Input: Year   
// Output: Whether the year is a leap year: 1 = yes, 0 = no 
// Global variables: 
// Calling module: 
// Author: Xiao Xiaosheng
// Date: June 5, 2014
// Notes: Month  1 2 3 4 5 6 7 8 9 10 11 12
//     Leap year  31 29 31 30 31 30 31 31 30 31 30 31  Total: 366 days = 31622400 seconds
//     Common year 31 28 31 30 31 30 31 31 30 31 30 31  Total: 365 days = 31536000 seconds
//-------------------------------------------------------------------------------------------------------
// Modified by:
// Date:
// Notes: 
//=======================================================================================================
const u8 mon_table[12]={31,28,31,30,31,30,31,31,30,31,30,31};

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
//=======================================================================================================
// Function name: u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec)
// Description:  Set the clock
// Input:  Year-month-day-hour-minute-second   
// Output:  0 = success; otherwise, an error code.
// Global variables: 
// Calling module: 
// Author: Xiao Xiaosheng
// Date: June 5, 2014
// Notes: Based on January 1, 2016; valid years are 2016-2136.
//        Returns 1 for an invalid date, 2 if the RTC is unavailable/times out.
//-------------------------------------------------------------------------------------------------------
// Modified by:
// Date:
// Notes: 
//=======================================================================================================
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
  RTC->CRL |= RTC_CRL_CNF;    //Enter configuration mode
  RTC->CNTH = seccount >> 16;   //Write the new counter-register value
  RTC->CNTL = seccount & 0xFFFFU;
  RTC->CRL &= ~RTC_CRL_CNF;   //Exit configuration mode
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
/*************************************Day-of-week formula******************************************
// (year + year/4 + year/400 - year/100 - year offset + month offset + day) / 7
// The remainder gives the day of the week.
 Note: All divisions in the formula use integer quotients.
    Year offset: 1 for common years, 2 for leap years.
    Month offsets, 1. Common year: Jan 0, Feb 3, Mar 3, Apr 6, May 1, Jun 4,
            Jul 0, Aug 3, Sep 5, Oct 0, Nov 3, Dec 5.
        2. Leap year: Jan 0, Feb 3, Mar 4, Apr 0, May 2, Jun 5,
            Jul 0, Aug 3, Sep 6, Oct 1, Nov 4, Dec 6.
Example: What day of the week was October 1, 1949?
  (1949+1949/4+1949/400-1949/100-1+0+1)/7
 = (1949+487+4-19-1+0+1)/7
 = 345, remainder 6
 Therefore, that date was a Saturday.
**********************************************************************************************/                                     
//=======================================================================================================
// Function name: u8 RTC_Get_Week(u16 year,u8 month,u8 day)
// Description:  Get the day of the week from a Gregorian date (only years 2000-2136 are allowed)
// Input:  Gregorian year, month, and day 
// Output:  Day-of-week number    
// Global variables: 
// Calling module: 
// Author: Xiao Xiaosheng
// Date: June 5, 2014
// Notes: 
//-------------------------------------------------------------------------------------------------------
// Modified by:
// Date:
// Notes: 
//=======================================================================================================
const u8 week_tab[] ={0,1,4,4,0,2,5,0,3,6,1,4,6};

u8 RTC_Get_Week(u16 Year,u8 Month,u8 Date) {
 if (Month < 1 || Month > 12 || Date < 1 || Date > 31)
   return 0;
 if (Month < 3 && Is_Leap_Year(Year))
   Date--;
 return ( Date + Year + Year/4 + Year/400 -Year/100 + week_tab[Month]-2 ) % 7;
}
//=======================================================================================================
// Function name: u8 RTC_Get(void)
// Description:  Convert seconds to calendar time to obtain the current time
// Input:  Gregorian year, month, and day 
// Output:  Updates PLC calendar registers when the RTC is available.
// Global variables: 
// Calling module: 
// Author: Xiao Xiaosheng
// Date: June 5, 2014
// Notes: 
//-------------------------------------------------------------------------------------------------------
// Modified by:
// Date:
// Notes: 
//=======================================================================================================
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

void RTC_Init(void) {
u32 backup_access;
u8 retained;

  rtc_unavailable();
  PLC_BIT_OFF(M8015);
  RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
  (void)RCC->APB1ENR;          //Complete the peripheral clock enable before access
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
    RTC->CNTH = 0;             //January 1, 2016, 00:00:00
    RTC->CNTL = 0;
    RTC->CRL &= ~RTC_CRL_CNF;
    if (!rtc_wait(&RTC->CRL, RTC_CRL_RTOFF, RTC_WAIT_LIMIT))
      goto init_done;
  }
  /* Do not overwrite a retained clock with an unexpected prescaler. */
  if ((RTC->PRLH & 0xFU) != 0 || (RTC->PRLL & 0xFFFFU) != 32767U)
    goto init_done;
  BKP->DR1 = RTC_BACKUP_MAGIC; //Mark valid only after setup has succeeded
  rtc_available = 1;
  PLC_BIT_ON(M8018);

init_done:
  if (!backup_access)
    PWR->CR &= ~PWR_CR_DBP;
}
