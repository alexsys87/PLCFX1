
static void reset(void) {
  memset(&mock_rtc,0,sizeof(mock_rtc)); memset(&mock_rcc,0,sizeof(mock_rcc));
  memset(&mock_pwr,0,sizeof(mock_pwr)); memset(&mock_bkp,0,sizeof(mock_bkp));
  for (unsigned i=0; i<7; i++) plc_dates[i]=0;
  plc_bits=0; rtc_available=0;
  fail_lse=fail_sync=fail_write=0;
  fail_write_after=write_waits=0;
}
static void roundtrip(u16 y,u8 m,u8 d,u8 h,u8 n,u8 s) {
  assert(RTC_Set(y,m,d,h,n,s)==0); RTC_Get();
  assert(D8018==y && D8017==m && D8016==d && D8015==h && D8014==n && D8013==s);

}
int main(void) {
  reset(); RTC_Init(); assert(PLC_BIT_TEST(M8018));
  assert(BKP->DR1==RTC_BACKUP_MAGIC && RTC->PRLH==0 && RTC->PRLL==32767);
  assert(!(PWR->CR & PWR_CR_DBP)); RTC_Get();
  assert(D8018==2016 && D8017==1 && D8016==1 && D8019==5);
  unsigned dates=0, y,m,d,h,n,s,w; unsigned long seconds;
  while (scanf("%u %u %u %u %u %u %lu %u", &y,&m,&d,&h,&n,&s,&seconds,&w)==8) {
    roundtrip((u16)y,(u8)m,(u8)d,(u8)h,(u8)n,(u8)s);
    assert(((RTC->CNTH<<16)|RTC->CNTL)==(u32)seconds);
    assert(D8019==w); dates++;
  }
  assert(dates==44195);
  assert(RTC_Get_Week(2100,3,1)==1);
  assert(RTC_Get_Week(2000,2,29)==2);
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
  roundtrip(2026,10,10,21,0,0);
  u32 high=RTC->CNTH, low=RTC->CNTL;
  RTC_Init(); assert(PLC_BIT_TEST(M8018)); assert(RTC->CNTH==high && RTC->CNTL==low);
  RTC_Get(); assert(D8018==2026 && D8017==10 && D8016==10);
  PLC_BIT_ON(M8015); D8017=257; RTC_Get();
  assert(RTC->CNTH==high && RTC->CNTL==low); PLC_BIT_OFF(M8015);
  /* The PLC-word snapshot used by M8015 must preserve argument order. */
  D8018=2027; D8017=11; D8016=12; D8015=13; D8014=14; D8013=15;
  PLC_BIT_ON(M8015); RTC_Get(); PLC_BIT_OFF(M8015); RTC_Get();
  assert(D8018==2027 && D8017==11 && D8016==12 &&
         D8015==13 && D8014==14 && D8013==15);
  RTC->CRL &= ~RTC_CRL_RTOFF; fail_write=1;
  assert(RTC_Set(2026,1,1,0,0,0)==2); assert(!PLC_BIT_TEST(M8018));
  assert(!(PWR->CR & PWR_CR_DBP));
  reset(); fail_lse=1; PLC_BIT_ON(M8018); PLC_BIT_ON(M8015); RTC_Init();
  assert(!PLC_BIT_TEST(M8018) && !PLC_BIT_TEST(M8015) && BKP->DR1==0);
  assert((RCC->BDCR & RCC_BDCR_LSEON) && !(RCC->BDCR & RCC_BDCR_LSERDY));
  assert(!(RCC->CSR & RCC_CSR_LSION) && !(RCC->BDCR & RCC_BDCR_RTCEN));
  assert(!(PWR->CR & PWR_CR_DBP));
  assert(RTC_Set(2026,1,1,0,0,0)==2);
  reset(); RTC_Init(); roundtrip(2026,10,10,0,0,0);
  high=RTC->CNTH; low=RTC->CNTL;
  RCC->BDCR &= ~RCC_BDCR_LSERDY; fail_lse=1; RTC_Init();
  assert(!PLC_BIT_TEST(M8018) && BKP->DR1==RTC_BACKUP_MAGIC);
  assert(RTC->CNTH==high && RTC->CNTL==low && RTC->PRLL==32767);
  reset(); fail_sync=1; RTC_Init(); assert(!PLC_BIT_TEST(M8018) && BKP->DR1==0);
  reset(); fail_write=1; RTC_Init(); assert(!PLC_BIT_TEST(M8018) && BKP->DR1==0);
  reset(); fail_write_after=2; RTC_Init();
  assert(!PLC_BIT_TEST(M8018) && BKP->DR1==0 && !(PWR->CR & PWR_CR_DBP));
  reset(); RTC_Init(); write_waits=0; fail_write_after=2;
  assert(RTC_Set(2026,1,1,0,0,0)==2 && !PLC_BIT_TEST(M8018));
  assert(!(PWR->CR & PWR_CR_DBP));
  reset(); RTC_Init(); roundtrip(2026,10,10,0,0,0);
  high=RTC->CNTH; low=RTC->CNTL; RTC->PRLL=1;
  RTC_Init(); assert(!PLC_BIT_TEST(M8018)); assert(RTC->CNTH==high && RTC->CNTL==low);
  reset(); PWR->CR=PWR_CR_DBP; RTC_Init(); assert(PWR->CR & PWR_CR_DBP);
  assert(RTC_Set(2016,1,1,0,0,0)==0); assert(PWR->CR & PWR_CR_DBP);
  RCC->BDCR &= ~RCC_BDCR_LSERDY; RTC_Get(); assert(!PLC_BIT_TEST(M8018));
  printf("PASS: host model; %u independent calendar/weekday checks; validation, retained clock, pre/post-write timeouts, no-LSE, availability and DBP tests\n",dates);
}
