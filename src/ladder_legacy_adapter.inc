/* Software RUN/STOP and main-owned image lifecycle shared with the minimal
 * firmware contract. The execution state machine is strictly legacy-only. */
enum { REQUEST_NONE, REQUEST_STOP, REQUEST_RUN };
static volatile u8 pending_run_request;
static volatile u32 milliseconds;
static u32 previous_ms;
static u16 timer_fraction[256];
static u8 initialized, running;
static plc_runtime_error last_error;
static u16 error_step;
static union { uint64_t alignment; u8 bytes[8]; } rejected_access;

static void legacy_fault(plc_runtime_error error) {
  if (!last_error) {
    last_error = error;
    error_step = CMD_ADDR < MAXSTEP ? (u16)CMD_ADDR : (u16)(MAXSTEP-1U);
  }
  PLC_PRG_STOP = 1;
}
static void PLC_Legacy_Missing(void) { legacy_fault(PLC_RUNTIME_LEGACY_MISSING); }
static void PLC_Legacy_Incomplete(void) { legacy_fault(PLC_RUNTIME_LEGACY_INCOMPLETE); }
static volatile void *legacy_memory(uintptr_t address, u32 size, u32 alignment) {
  if (!(address & (alignment-1U)) &&
      ((address >= PLC_RAM_BASE && address <= PLC_RAM_BASE + PLC_RAM_SIZE-size) ||
       (address >= PLC_STEPS_BASE && address <= PLC_STEPS_BASE + PLC_STEPS_SIZE-size)))
    return (volatile void *)address;
  legacy_fault(PLC_RUNTIME_BOUNDS);
  return rejected_access.bytes;
}
static u16 legacy_word(void) {
  uintptr_t pc = (uintptr_t)PLC_PC, start = (uintptr_t)PLC_START_ADDR;
  if (pc < start || pc >= start + MAXSTEP*2U || (pc & 1U)) {
    legacy_fault(PLC_RUNTIME_BOUNDS);
    return 0;
  }
  return *PLC_PC;
}
static void legacy_bit_put(u32 bit, u8 on) {
  u32 saved = 0;
  if (bit >= PLC_RAM_SIZE*8U) { legacy_fault(PLC_RUNTIME_BOUNDS); return; }
  if (bit / 8U == Addr_M8) { saved=__get_PRIMASK(); __disable_irq(); }
  if (on) PLC_8BIT(bit/8U) |= PLC_BIT_OR[bit&7U];
  else PLC_8BIT(bit/8U) &= PLC_BIT_AND[bit&7U];
  if (bit / 8U == Addr_M8) __set_PRIMASK(saved);
}
static u32 legacy_pulse_test(u32 step) {
  if (step >= MAXSTEP) { legacy_fault(PLC_RUNTIME_BOUNDS); return 0; }
  return (PLC_Bu(RAM_STEP_ADDR+step/8U) >> (step&7U)) & 1U;
}
static void legacy_pulse_put(u32 step, u8 on) {
  u32 addr;
  if (step >= MAXSTEP) { legacy_fault(PLC_RUNTIME_BOUNDS); return; }
  addr=RAM_STEP_ADDR+step/8U;
  if (on) PLC_Bu(addr) |= PLC_BIT_OR[step&7U];
  else PLC_Bu(addr) &= PLC_BIT_AND[step&7U];
}
static void legacy_clear_pulses(void) {
  memset((void *)(uintptr_t)PLC_STEPS_BASE,0,PLC_STEPS_SIZE);
}
static void legacy_stop(void) {
  u16 n;
  memset((void *)(uintptr_t)(RAM_ADDR+Addr_Y),0,64);
  memset((void *)(uintptr_t)(RAM_ADDR+Addr_TE),0,32);
  memset((void *)(uintptr_t)(RAM_ADDR+Addr_CE),0,32);
  for (n=0;n<246U;++n) {
    PLC_16BIT(Addr_TVal+n*2U)=0; timer_fraction[n]=0;
    PLC_BIT_OFF(Addr_TO*8U+n);
  }
  PLC_BIT_OFF(M8000); PLC_BIT_ON(M8001);
  PLC_BIT_OFF(M8002); PLC_BIT_ON(M8003);
  PLC_PRG_STOP=1; PLC_PULS=1;
  PLC_STL_CMD=PLC_STL_Status=0;
  if (running) legacy_clear_pulses();
  running=0;
}
void PLC_Runtime_RequestRun(u8 run) {
  pending_run_request=run ? REQUEST_RUN : REQUEST_STOP;
}
u8 PLC_Runtime_RunRequested(void) {
  u8 request=pending_run_request;
  if (request != REQUEST_NONE) return request == REQUEST_RUN;
  return PLC_BIT_TEST(M8000) && !PLC_BIT_TEST(M8001);
}
static u8 take_request(void) {
  u32 saved=__get_PRIMASK(); u8 request;
  __disable_irq(); request=pending_run_request; pending_run_request=REQUEST_NONE;
  __set_PRIMASK(saved); return request;
}
void PLC_Runtime_ClearFault(void) {
  last_error=PLC_RUNTIME_OK; error_step=0;
  D8067=D8068=D8069=0; PLC_BIT_OFF(M8065);
}
plc_runtime_error PLC_Runtime_LastError(void) { return last_error; }
u16 PLC_Runtime_ErrorStep(void) { return error_step; }
void PLC_Runtime_Init(void) {
  milliseconds=previous_ms=0; running=0; PLC_PULS=1; PLC_PRG_STOP=1;
  PLC_ACC_BIT=PLC_MPS_BIT=PLC_STL_CMD=PLC_STL_Status=0;
  PLC_STL_Count=0;
  memset(timer_fraction,0,sizeof timer_fraction);
  memset(PLC_P_Addr,0,sizeof PLC_P_Addr);
  memset(p_save,0,sizeof p_save); memset(process,0,sizeof process);
  legacy_clear_pulses(); PLC_Runtime_ClearFault(); initialized=1;
}
void PLC_Runtime_Tick1ms(void) { ++milliseconds; }
static void legacy_put_bit(u32 bit,u8 value) {
  if (value) PLC_BIT_ON(bit); else PLC_BIT_OFF(bit);
}
/* Scan-owned ticking replaces the missing historical TimerInc ISR path.
 * Legacy timer coil/preset handlers retain their experimental limitations. */
static void legacy_advance_timers(u32 elapsed) {
  u16 n;
  for (n=0;n<256U;++n) if (PLC_BIT_TEST(Addr_TE*8U+n)) {
    u16 base=n<200U || n>=250U ? 100U : n<246U ? 10U : 1U;
    u32 units=elapsed/base;
    u16 remainder=(u16)(elapsed%base+timer_fraction[n]);
    u16 current=PLC_16BIT(Addr_TVal+n*2U), maximum=PLC_16BIT(Addr_TMax+n*2U);
    units+=remainder/base; timer_fraction[n]=remainder%base;
    if (current < maximum) {
      u32 gap=(u32)maximum-current;
      PLC_16BIT(Addr_TVal+n*2U)=units>=gap ? maximum : (u16)(current+units);
    }
  }
}
static void legacy_publish_fault(void) {
  if (last_error) {
    D8067=(u16)last_error; D8068=D8069=error_step; PLC_BIT_ON(M8065);
    legacy_stop();
  }
}
void PLC_Main_Parser(void) {
  u16 i; u32 now, budget=MAXSTEP*4U;
  u8 request;
  if (!initialized) PLC_Runtime_Init();
  now=milliseconds; request=take_request();
  if (request == REQUEST_STOP) { legacy_stop(); previous_ms=now; return; }
  if (request == REQUEST_RUN) { PLC_BIT_ON(M8000); PLC_BIT_OFF(M8001); }
  legacy_put_bit(M8011,(now%10U)>=5U); legacy_put_bit(M8012,(now%100U)>=50U);
  legacy_put_bit(M8013,(now%1000U)>=500U); legacy_put_bit(M8014,(now%60000U)>=30000U);
  if (PLC_NEW_PROG) {
    PLC_NEW_PROG=0; running=0; PLC_Runtime_ClearFault(); legacy_clear_pulses();
    memset(timer_fraction,0,sizeof timer_fraction);
    memset(p_save,0,sizeof p_save); memset(process,0,sizeof process);
  }
  if (last_error && PLC_BIT_TEST(M8000) && !PLC_BIT_TEST(M8001)) PLC_Runtime_ClearFault();
  if (last_error || PLC_UPDATE_PROG || !PLC_BIT_TEST(M8000) || PLC_BIT_TEST(M8001)) {
    legacy_stop(); previous_ms=now; return;
  }
  /* Legacy labels are found by raw words, not a validated instruction map.
   * Limit the historical 8-bit label index to its actual 128-entry array. */
  memset(PLC_P_Addr,0,sizeof PLC_P_Addr);
  for (i=0;i<MAXSTEP;++i) {
    u16 w=(PLC_START_ADDR)[i];
    if ((w&0xFF00U)==0xB000U && (w&0xFFU)<128U) PLC_P_Addr[w&0xFFU]=PLC_START_ADDR+i;
  }
  if (!running) { legacy_clear_pulses(); PLC_PULS=1; previous_ms=now; }
  legacy_advance_timers(now-previous_ms); previous_ms=now;
  running=1; PLC_PRG_STOP=0;
  legacy_put_bit(M8002,PLC_PULS); legacy_put_bit(M8003,!PLC_PULS);
  PLC_ACC_BIT=PLC_MPS_BIT=0; PLC_STL_CMD=PLC_STL_Status=0; PLC_STL_Count=0;
  PLC_PC=PLC_START_ADDR;
  while (!PLC_PRG_STOP && !last_error) {
    if (pending_run_request==REQUEST_STOP || PLC_UPDATE_PROG ||
        !PLC_BIT_TEST(M8000) || PLC_BIT_TEST(M8001)) { legacy_stop(); return; }
    if (!budget--) { legacy_fault(PLC_RUNTIME_LEGACY_BUDGET); break; }
    CMD_CMD=legacy_word();
    if (last_error) break;
    CMD_REG=CMD_CMD&0x0FFFU; CMD_ADDR=(u32)(PLC_PC-PLC_START_ADDR);
    PLC_Err=PLC_PC++;
    FUNC_BASIC[CMD_CMD>>12]();
  }
  legacy_publish_fault();
  if (last_error) return;
  if (pending_run_request==REQUEST_STOP) { legacy_stop(); return; }
  PLC_PULS=0; PLC_BIT_OFF(M8002); PLC_BIT_ON(M8003);
}
void PLC_ProInstructParse(void) { PLC_Main_Parser(); }
