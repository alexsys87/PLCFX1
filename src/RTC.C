#include <main.h>
#include "stm32f10x_rtc.h"
#include "PLC_Dialogue.h"
#include "PLC_CONF.H"


extern u32 PLC_BIT_TEST(u32 addr);
extern void PLC_BIT_ON(u32 addr);
extern void PLC_BIT_OFF(u32 addr);

//=======================================================================================================
// 函数名称: u8 Is_Leap_Year(u16 year)
// 功能描述： 判断是否是闰年函数
// 输　入: 年份   
// 输　出: 该年份是不是闰年.1,是.0,不是 
// 全局变量: 
// 调用模块: 
// 作　者: 小小晟
// 日　期: 2014年6月5日
// 备 注: 月份  1 2 3 4 5 6 7 8 9 10 11 12
//     闰年  31 29 31 30 31 30 31 31 30 31 30 31  一共366天=31622400秒钟
//     非闰年 31 28 31 30 31 30 31 31 30 31 30 31  一共365天=31536000秒钟
//-------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备 注: 
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
// 函数名称: u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec)
// 功能描述： 设置时钟
// 输　入:  年-月-日-时-分-秒   
// 输　出:  0,成功;其他:错误代码.
// 全局变量: 
// 调用模块: 
// 作　者: 小小晟
// 日　期: 2014年6月5日
// 备 注: 以2010年1月1日为基准 2011~2136年为合法年份
//-------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备 注: 
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
  RTC->CRL |= RTC_CRL_CNF;    //怅膻麒螯 疱骅?觐眙桡箴桊钼囗?
  RTC->CNTH = seccount >> 16;   //玎镨襦螯 眍忸?珥圜屙桢 聍弪眍泐 疱汨耱疣
  RTC->CNTL = seccount;       //
  RTC->CRL &= ~RTC_CRL_CNF;   //恹轵?桤 疱骅爨 觐眙桡箴桊钼囗?
  while ( (RTC->CRL & RTC_FLAG_RTOFF) == (uint16_t)RESET ) { }
  PWR->CR &= ~PWR_CR_DBP;
  return 0;
}
/*************************************星期计算公式******************************************
//（年+年/4+年/400-年/100-年基数+月基数+日）/7＝……余星期几
 注：式中分数均取整
    年基数，平年1,闰年2,
    月基数，1、平年：一月0, 二月3, 三月3, 四月6, 五月1, 六月4,
            七月0, 八月3, 九月5, 十月0, 十一月3,十二月5.     
        2、闰年：一月0, 二月3, 三月4, 四月0, 五月2, 六月5,
            七月0, 八月3, 九月6, 十月1, 十一月4, 十二月6.
如：1949年10月1日是星期几？
  （1949+1949/4+1949/400-1949/100-1+0+1）/7
 ＝（1949+487+4-19-1+0+1）/7
 ＝345……6
 即该日为星期六。  
**********************************************************************************************/                                     
//=======================================================================================================
// 函数名称: u8 RTC_Get_Week(u16 year,u8 month,u8 day)
// 功能描述： 输入公历日期得到星期(只允许2000-2136年) 获得现在是星期几
// 输　入:  公历年月日 
// 输　出:  星期号    
// 全局变量: 
// 调用模块: 
// 作　者: 小小晟
// 日　期: 2014年6月5日
// 备 注: 
//-------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备 注: 
//=======================================================================================================
const u8 week_tab[] ={0,1,4,4,0,2,5,0,3,6,1,4,6};

u8 RTC_Get_Week(u16 Year,u8 Month,u8 Date) {
 if ( ( Month < 3 ) && ( ( !( Year & 0x03 ) && ( Year % 1000 ) ) || ( !( Year % 400 ) ) ) )
   Date--;
 return ( Date + Year + Year/4 + Year/400 -Year/100 + week_tab[Month]-2 ) % 7;
}
//=======================================================================================================
// 函数名称: u8 RTC_Get(void)
// 功能描述： 把秒变成时间 得到当前的时间
// 输　入:  公历年月日 
// 输　出:  //返回值:0,成功;其他:错误代码.  
// 全局变量: 
// 调用模块: 
// 作　者: 小小晟
// 日　期: 2014年6月5日
// 备 注: 
//-------------------------------------------------------------------------------------------------------
// 修改人:
// 日　期:
// 备 注: 
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

//    RCC->BDCR &= ~0x10;
//    BKP_DeInit();
//    RCC_LSEConfig(RCC_LSE_ON); 
//    while ( RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) ;
//    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE); 
//    RCC_RTCCLKCmd(ENABLE); 


    RCC->BDCR |=  RCC_BDCR_BDRST; //恹镱腠栩?襻痤?钺豚耱?疱珏疴睇?溧眄
    RCC->BDCR &= ~RCC_BDCR_BDRST;
 
    //恹狃囹?桉蝾黜桕铎 蜞牝钼 桁矬朦耦?忭屮龛?赈囵?32768 ?镱溧螯 蜞牝桊钼囗桢
    RCC->BDCR |=  RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE;
 
    while ( (RTC->CRL & RTC_FLAG_RTOFF) == (uint16_t)RESET ) { }

    RTC->CRL  |=  RTC_CRL_CNF;
    RTC->PRLL  = 0x7FFF;         //疱汨耱?溴脲龛 磬 32768
    RTC->CRL  &=  ~RTC_CRL_CNF;
 
    //篑蜞眍忤螯 徼?疣琊屮屙? 疣犷螓 ?漕驿囹 篑蜞眍怅?徼蜞 泐蝾忭铖蜩
    RCC->BDCR |= RCC_BDCR_LSEON;
    while ((RCC->BDCR & RCC_BDCR_LSEON) != RCC_BDCR_LSEON) {}
 
    RTC->CRL &= (uint16_t)~RTC_CRL_RSF;
    while ( (RTC->CRL & RTC_CRL_RSF) != RTC_CRL_RSF ) {}
/*
    RCC->BDCR |= RCC_LSE_ON; // 篑蜞磬怆桠噱?徼?疣琊屮屙? 疣犷螓
    while ( !(RCC->BDCR & RCC_CSR_LSIRDY) ) {} // ?驿屐 泐蝾忭铖蜩
    RCC->BDCR |= RCC_RTCCLKSource_LSE;
    RCC->BDCR |= RCC_BDCR_RTCEN;

    RTC_WaitForSynchro(); 
//    RTC->CRL &= (uint16_t)~RTC_FLAG_RSF;
//    while ( (RTC->CRL & RTC_FLAG_RSF) == 0 ) { }
    RTC_WaitForLastTask(); 
//    while ( (RTC->CRL & RTC_FLAG_RTOFF) == 0 ) { }
    RTC_ITConfig(RTC_IT_SEC, ENABLE); 
//    RTC->CRH |= RTC_IT_SEC;
    RTC_WaitForLastTask(); 
//    while ( (RTC->CRL & RTC_FLAG_RTOFF) == 0 ) { }
    RTC_SetPrescaler(32767); 
//    RTC->CRL |= RTC_CRL_CNF;    //怅膻麒螯 疱骅?觐眙桡箴桊钼囗?
//    RTC->PRLH = 0;
//    RTC->PRLL = 32767;
//    RTC->CRL &= ~RTC_CRL_CNF;   //恹轵?桤 疱骅爨 觐眙桡箴桊钼囗?
    RTC_WaitForLastTask(); 
//    while ( (RTC->CRL & RTC_FLAG_RTOFF) == 0 ) { }
*/
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
//    RCC_ClearFlag();  
////    RCC->CSR |= ((uint32_t)0x01000000);
//    RTC_ITConfig(RTC_IT_SEC, ENABLE); 
//    RTC->CRH |= RTC_IT_SEC;
////    RTC_WaitForLastTask();
//    while ( (RTC->CRL & RTC_FLAG_RTOFF) == 0 ) { }
  }

  PLC_BIT_ON(M8018);
  PLC_BIT_OFF(M8015);
}
