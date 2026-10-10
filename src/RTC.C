#include <main.h>
#include "stm32f10x_rtc.h"
#include "PLC_Dialogue.h"
#include "PLC_CONF.H"

extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

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
// Notes: Based on January 1, 2010; valid years are 2011-2136
//-------------------------------------------------------------------------------------------------------
// Modified by:
// Date:
// Notes: 
//=======================================================================================================
u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec) {
u16 t;
u32 seccount = 0;

  if ( syear < 2016 || syear > 2136 )
    return 1;
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

/*  PWR->CR |= 0x08;
//  PWR_BackupAccessCmd(ENABLE);
  RTC_WaitForLastTask();
  RTC_SetCounter(seccount);
  RTC_WaitForLastTask();
*/
  PWR->CR |= PWR_CR_DBP;
  while ( (RTC->CRL & RTC_FLAG_RTOFF) == (uint16_t)RESET ) { }
  RTC->CRL |= RTC_CRL_CNF;    //Enter configuration mode
  RTC->CNTH = seccount >> 16;   //Write the new counter-register value
  RTC->CNTL = seccount;       //
  RTC->CRL &= ~RTC_CRL_CNF;   //Exit configuration mode
  while ( (RTC->CRL & RTC_FLAG_RTOFF) == (uint16_t)RESET ) { }
  PWR->CR &= ~PWR_CR_DBP;
  return 0;
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
 if ( ( Month < 3 ) && ( ( !( Year & 0x03 ) && ( Year % 1000 ) ) || ( !( Year % 400 ) ) ) )
   Date--;
 return ( Date + Year + Year/4 + Year/400 -Year/100 + week_tab[Month]-2 ) % 7;
}
//=======================================================================================================
// Function name: u8 RTC_Get(void)
// Description:  Convert seconds to calendar time to obtain the current time
// Input:  Gregorian year, month, and day 
// Output:  //Return value: 0 = success; otherwise, an error code.  
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
static u16 daycnt=0;
u32 timecount=0; 
u32 temp=0;
u16 temp1=0;

  if ( PLC_BIT_TEST(M8015) ) {
    RTC_Set(D8018, D8017, D8016, D8015, D8014, D8013);
  } else if ( PLC_BIT_TEST(M8018) ) {
    timecount = (RTC->CNTH << 16) | RTC->CNTL ;
//    timecount = RTC_GetCounter();
    temp = timecount / 86400;
    if ( daycnt != temp ) {
      daycnt = temp;
      temp1 = 2016;
      while ( temp >= 365 ) {             
        if ( Is_Leap_Year(temp1) ) {
          if ( temp >= 366 ) temp -= 366;
          else { temp1++; break;} 
        } else temp -= 365;
        temp1++; 
      }  
      D8018 = temp1;
      temp1 = 0;
      while ( temp >= 28) {
        if ( Is_Leap_Year(D8017) && temp1 == 1 ) {
          if ( temp >= 29) temp -= 29;
          else break;
        } else {
          if ( temp >= mon_table[temp1] ) temp -= mon_table[temp1];
          else break;
        }
        temp1++; 
      }
      D8017 = temp1 + 1;
      D8016 = temp + 1;
    }
    temp = timecount % 86400;
    D8015 = temp / 3600;
    D8014 = (temp % 3600) / 60;
    D8013 = (temp % 3600) % 60;
    D8019 = RTC_Get_Week(D8018,D8017,D8016);
  }
}

void RTC_Init(void) {
u16 u16_WaitForOscSource;

  RCC->APB1ENR |= RCC_APB1Periph_PWR | RCC_APB1Periph_BKP;

  if ( BKP->DR1 != 0x5A5A ) {
    PWR->CR |= PWR_CR_DBP;

//    while ( RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) ;

    RCC->BDCR |=  RCC_BDCR_BDRST; //Reset the backup domain
    RCC->BDCR &= ~RCC_BDCR_BDRST;
 
    //Select the external 32768 Hz crystal as the clock source and enable clocking
    RCC->BDCR |=  RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE;
 
    while ( (RTC->CRL & RTC_FLAG_RTOFF) == (uint16_t)RESET ) { }

    RTC->CRL  |=  RTC_CRL_CNF;
    RTC->PRLL  = 0x7FFF;         //Prescaler register: divide by 32768
    RTC->CRL  &=  ~RTC_CRL_CNF;
 
    //Set the enable bit and wait for the ready bit
    RCC->BDCR |= RCC_BDCR_LSEON;
    while ((RCC->BDCR & RCC_BDCR_LSEON) != RCC_BDCR_LSEON) {}
 
    RTC->CRL &= (uint16_t)~RTC_CRL_RSF;
    while ( (RTC->CRL & RTC_CRL_RSF) != RTC_CRL_RSF ) {}

    BKP->DR1 = 0x5A5A;
    while ( (RTC->CRL & RTC_FLAG_RTOFF) == (uint16_t)RESET ) { }
    PWR->CR &= ~PWR_CR_DBP;

  } else {
///    for ( u16_WaitForOscSource = 0; u16_WaitForOscSource < 5000; u16_WaitForOscSource++ );
///    if ( ( RCC->CSR & ((uint32_t)1 << ( RCC_FLAG_PORRST & 0x1F))) != 0 );
//    if ( RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET );
////    else
////      if ( ( RCC->CSR & ((uint32_t)1 << ( RCC_FLAG_PINRST & 0x1F))) != 0 );
//      if ( RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET ){}

////    RCC->CSR |= ((uint32_t)0x01000000);

////    RTC_WaitForLastTask();
//    while ( (RTC->CRL & RTC_FLAG_RTOFF) == 0 ) { }
  }

  PLC_BIT_ON(M8018);
  PLC_BIT_OFF(M8015);
}
