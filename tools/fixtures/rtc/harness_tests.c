/**
 * @file harness_tests.c
 * @brief Last part of the RTC test program: the assertions that exercise src/plc_rtc.c.
 *
 * Appended to harness_prefix.c and the driver source by tools/test_rtc.py. The calendar lines on
 * standard input come from an independent oracle (the Python datetime module) and cover every day
 * from 2016-01-01 to 2136-12-31, 44195 dates; the remaining assertions are explicit cases.
 */

/**
 * Power-on state: clear every mock peripheral, the PLC date words and the relays, mark the driver
 * unavailable and switch all failure injection off.
 */
static void reset(void) {
  memset(&mock_rtc,0,sizeof(mock_rtc)); memset(&mock_rcc,0,sizeof(mock_rcc));
  memset(&mock_pwr,0,sizeof(mock_pwr)); memset(&mock_bkp,0,sizeof(mock_bkp));
  for (unsigned i=0; i<7; i++) plc_dates[i]=0;
  plc_bits=0; rtc_available=0;
  fail_lse=fail_sync=fail_write=0;
  fail_write_after=write_waits=0;
}
/** Set a date, read it back through RTC_Get() and compare it with D8013..D8018. */
static void roundtrip(u16 y,u8 m,u8 d,u8 h,u8 n,u8 s) {
  assert(RTC_Set(y,m,d,h,n,s)==0); RTC_Get();
  assert(D8018==y && D8017==m && D8016==d && D8015==h && D8014==n && D8013==s);

}
/** Run the RTC test cases; the calendar oracle is read from standard input. */
int main(void) {
  /* First start: the clock counts from 2016-01-01 (a Friday, weekday 5) with a 1 Hz prescaler, the
   * retained-clock marker is written, M8018 is on and the backup write access is restored. */
  reset(); RTC_Init(); assert(PLC_BIT_TEST(M8018));
  assert(BKP->DR1==RTC_BACKUP_MAGIC && RTC->PRLH==0 && RTC->PRLL==32767);
  assert(!(PWR->CR & PWR_CR_DBP)); RTC_Get();
  assert(D8018==2016 && D8017==1 && D8016==1 && D8019==5);
  /* Every calendar day: setting it must store the right counter value and read back the right
   * date and weekday. */
  unsigned dates=0, y,m,d,h,n,s,w; unsigned long seconds;
  while (scanf("%u %u %u %u %u %u %lu %u", &y,&m,&d,&h,&n,&s,&seconds,&w)==8) {
    roundtrip((u16)y,(u8)m,(u8)d,(u8)h,(u8)n,(u8)s);
    assert(((RTC->CNTH<<16)|RTC->CNTL)==(u32)seconds);
    assert(D8019==w); dates++;
  }
  assert(dates==44195);
  /* Weekday of century dates: 2100 is not a leap year, 2000 is. */
  assert(RTC_Get_Week(2100,3,1)==1);
  assert(RTC_Get_Week(2000,2,29)==2);
  /* Invalid dates and times are rejected with 1 before the clock is touched: year outside
   * 2016..2136, month 0 and 13, day 0, 31 April, 29 February 2100, hour 24, minute 60, second 60. */
  assert(RTC_Set(2015,1,1,0,0,0)==1);
  assert(RTC_Set(2137,1,1,0,0,0)==1);
  assert(RTC_Set(2016,0,1,0,0,0)==1);
  assert(RTC_Set(2016,13,1,0,0,0)==1);
  assert(RTC_Set(2016,1,0,0,0,0)==1);
  assert(RTC_Set(2016,4,31,0,0,0)==1);
  assert(RTC_Set(2100,2,29,0,0,0)==1);
  assert(RTC_Set(2016,1,1,24,0,0)==1);
  assert(RTC_Set(2016,1,1,0,60,0)==1);
  assert(RTC_Set(2016,1,1,0,0,60)==1);
  /* Restart with a retained clock: RTC_Init() keeps the counter and therefore the date. */
  roundtrip(2026,10,10,21,0,0);
  u32 high=RTC->CNTH, low=RTC->CNTL;
  RTC_Init(); assert(PLC_BIT_TEST(M8018)); assert(RTC->CNTH==high && RTC->CNTL==low);
  RTC_Get(); assert(D8018==2026 && D8017==10 && D8016==10);
  /* M8015 with a month word of 257 (it would truncate to the valid month 1) is rejected as a whole;
   * the counter stays unchanged. */
  PLC_BIT_ON(M8015); D8017=257; RTC_Get();
  assert(RTC->CNTH==high && RTC->CNTL==low); PLC_BIT_OFF(M8015);
  /* The PLC-word snapshot used by M8015 must preserve argument order. */
  D8018=2027; D8017=11; D8016=12; D8015=13; D8014=14; D8013=15;
  PLC_BIT_ON(M8015); RTC_Get(); PLC_BIT_OFF(M8015); RTC_Get();
  assert(D8018==2027 && D8017==11 && D8016==12 &&
         D8015==13 && D8014==14 && D8013==15);
  /* A write whose wait for RTOFF times out returns 2, marks the clock unavailable (M8018 off) and
   * gives the backup write access back. */
  RTC->CRL &= ~RTC_CRL_RTOFF; fail_write=1;
  assert(RTC_Set(2026,1,1,0,0,0)==2); assert(!PLC_BIT_TEST(M8018));
  assert(!(PWR->CR & PWR_CR_DBP));
  /* No LSE on the first start: M8018 and M8015 are cleared, no marker is written, the LSE stays
   * requested but never ready, the RTC is not enabled and the LSI is not used as a fallback. Writing
   * the clock reports 2. */
  reset(); fail_lse=1; PLC_BIT_ON(M8018); PLC_BIT_ON(M8015); RTC_Init();
  assert(!PLC_BIT_TEST(M8018) && !PLC_BIT_TEST(M8015) && BKP->DR1==0);
  assert((RCC->BDCR & RCC_BDCR_LSEON) && !(RCC->BDCR & RCC_BDCR_LSERDY));
  assert(!(RCC->CSR & RCC_CSR_LSION) && !(RCC->BDCR & RCC_BDCR_RTCEN));
  assert(!(PWR->CR & PWR_CR_DBP));
  assert(RTC_Set(2026,1,1,0,0,0)==2);
  /* LSE failure on a restart with a retained clock: the clock is reported unavailable, but the
   * marker, the counter and the prescaler are left intact. */
  reset(); RTC_Init(); roundtrip(2026,10,10,0,0,0);
  high=RTC->CNTH; low=RTC->CNTL;
  RCC->BDCR &= ~RCC_BDCR_LSERDY; fail_lse=1; RTC_Init();
  assert(!PLC_BIT_TEST(M8018) && BKP->DR1==RTC_BACKUP_MAGIC);
  assert(RTC->CNTH==high && RTC->CNTL==low && RTC->PRLL==32767);
  /* Timeouts during initialisation - RSF never set, the first wait for RTOFF fails, the second one
   * (after the counter was loaded) fails: the clock stays unavailable, no marker is written and the
   * backup write access is restored. */
  reset(); fail_sync=1; RTC_Init(); assert(!PLC_BIT_TEST(M8018) && BKP->DR1==0);
  reset(); fail_write=1; RTC_Init(); assert(!PLC_BIT_TEST(M8018) && BKP->DR1==0);
  reset(); fail_write_after=2; RTC_Init();
  assert(!PLC_BIT_TEST(M8018) && BKP->DR1==0 && !(PWR->CR & PWR_CR_DBP));
  /* RTC_Set(): the second wait for RTOFF, after the counter was written, times out. */
  reset(); RTC_Init(); write_waits=0; fail_write_after=2;
  assert(RTC_Set(2026,1,1,0,0,0)==2 && !PLC_BIT_TEST(M8018));
  assert(!(PWR->CR & PWR_CR_DBP));
  /* A retained clock with an unexpected prescaler is neither used nor overwritten. */
  reset(); RTC_Init(); roundtrip(2026,10,10,0,0,0);
  high=RTC->CNTH; low=RTC->CNTL; RTC->PRLL=1;
  RTC_Init(); assert(!PLC_BIT_TEST(M8018)); assert(RTC->CNTH==high && RTC->CNTL==low);
  /* Backup write access that was already enabled stays enabled. Losing LSERDY at run time makes
   * RTC_Get() report the clock unavailable. */
  reset(); PWR->CR=PWR_CR_DBP; RTC_Init(); assert(PWR->CR & PWR_CR_DBP);
  assert(RTC_Set(2016,1,1,0,0,0)==0); assert(PWR->CR & PWR_CR_DBP);
  RCC->BDCR &= ~RCC_BDCR_LSERDY; RTC_Get(); assert(!PLC_BIT_TEST(M8018));
  printf("PASS: host model; %u independent calendar/weekday checks; validation, retained clock, pre/post-write timeouts, no-LSE, availability and DBP tests\n",dates);
}
