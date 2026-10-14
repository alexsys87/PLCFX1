/**
 * @file test_experimental.c
 * @brief Known-answer tests for the experimental FX1N-style instruction interpreter (src/plc_exp_*.c).
 *
 * Every test assembles a program from raw words, runs complete scans through the interpreter and
 * compares the PLC image with values that were worked out from the Mitsubishi instruction
 * definitions, not by running the implementation. The interpreter modules are linked as separate
 * units (tools/test_experimental.py builds them with AddressSanitizer and UBSan); only the
 * interrupt-mask intrinsics and the real-time-clock setter are mocked. The PLC image, the pulse
 * bitmap and the program flash are mapped at their target addresses, so the sanitizers see every
 * access. Nothing here is a hardware or GX Developer interoperability test.
 *
 * Layout of the file:
 *   1. mocks and fixture helpers (load, stopped, good_scan)
 *   2. a tiny assembler: macros that encode operands and fixed opcodes
 *   3. the tests, one function per instruction family (t_*)
 *   4. t_fuzz(): every table slot with random, mostly valid operands
 */
#include "platform.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "plc_exp_internal.h"
#include "plc_exp_handlers.h"

/* Stand-ins for firmware objects outside the tested modules (serial protocol, App/main.c, plc_io.c). */
volatile u8 PLC_UPDATE_PROG;
volatile u16 PLC_RUN_TIME;
u16 X_DIY;
static u32 primask;  /* modelled PRIMASK, 1 = interrupts masked */
static u16 rtc_arguments[6];  /* arguments of the last RTC_Set() call */
static unsigned rtc_calls;  /* number of RTC_Set() calls */
/* Mock of the interrupt-mask intrinsics: a critical section is __disable_irq() ... __set_PRIMASK(saved);
 * the mock only checks that the two are paired. */
u32 __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask=1; }
void __set_PRIMASK(u32 value) { assert(primask==1); primask=value; }
static u8 rtc_set_result;                    /* what the mocked RTC_Set() returns: 0 ok, 1 invalid, 2 unavailable */
/** Mock of the real-time-clock setter used by TWR: records the arguments and returns rtc_set_result. */
u8 RTC_Set(u16 year,u8 month,u8 day,u8 hour,u8 minute,u8 second) {
  rtc_arguments[0]=year; rtc_arguments[1]=month; rtc_arguments[2]=day;
  rtc_arguments[3]=hour; rtc_arguments[4]=minute; rtc_arguments[5]=second;
  ++rtc_calls;
  return rtc_set_result;
}
/** Map a zero-filled region at a fixed address (PLC image, pulse bitmap and flash of the target). */
static void map(u32 base,u32 size) {
  assert(mmap((void *)(uintptr_t)base,size,PROT_READ|PROT_WRITE,
    MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)==(void *)(uintptr_t)base);
}
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))  /* number of elements of an array */
/**
 * Start a test case: clear the PLC image and the program area (zero words are NOPs), copy @p count
 * words to the program start, reset the interpreter and request RUN. The next scan executes the
 * program.
 */
static void load(const u16 *words,size_t count) {
  memset((void *)(uintptr_t)PLC_RAM_BASE,0,PLC_RAM_SIZE);
  memset((void *)(uintptr_t)ROM_ADDR,0,14U*1024U);
  memcpy((void *)PLC_START_ADDR,words,count*2U);
  PLC_NEW_PROG=PLC_UPDATE_PROG=0; plc_exp_pending_run_request=PLC_EXP_REQUEST_NONE;
  PLC_Runtime_Init(); PLC_Runtime_RequestRun(1);
}
/** Assert the STOP state: M8000 off, M8001 on and the whole Y image cleared. */
static void stopped(void) {
  assert(!PLC_BIT_TEST(M8000) && PLC_BIT_TEST(M8001));
  for (unsigned i=0;i<64U;++i) assert(PLC_8BIT(Addr_Y+i)==0);
}
/**
 * Run one scan that must not fault. A fault is reported with the calling line, the error code and
 * the step of the faulting instruction; good_scan() passes the line automatically.
 */
static void good_scan_at(int line) {
  PLC_Main_Parser();
  if (PLC_Runtime_LastError()) fprintf(stderr,"line %d: fault %u at step %u\n",line,
    (unsigned)PLC_Runtime_LastError(),(unsigned)PLC_Runtime_ErrorStep());
  assert(PLC_Runtime_LastError()==0);
}
#define good_scan() good_scan_at(__LINE__)

/* Tiny assembler
 *
 * An operand item is a pair of program words, (TYPE << 8 | low byte) and (DEVICE << 8 | high byte),
 * where the low and high byte belong to the 16-bit device number or constant; plc_exp_operands.c
 * documents the complete encoding. A 32-bit operand consists of two such items.
 *
 *   K(v)  KL(v)     decimal constant, 16 / 32 bit                    80 80
 *   H(v)  HL(v)     hexadecimal constant                             82 80
 *   D(n)  DL(n)     data register Dn (D0..D999), 16 / 32 bit         86 86
 *   D1K(n)          D1000 + n                                        86 88
 *   TT(n)  CC(n)    current value of timer Tn / counter Cn           86 82 / 86 84
 *   CCL(n)          32-bit counter value (the second item repeats)   86 84
 *   D80(n)          special register, given as its number D80nn      86 80
 *   BT(b)           bit device with the linear bit address b         84 80
 *   GRP(n,b)        KnX group of n nibbles starting at bit b         84 80+2n
 *   PT(n)           pointer Pn (value 2n)                            88 80
 *   ZD(n,s)         Dn indexed by Zs (s = 0..3)                      A6 86
 */
/** Opcode of a handler: its index in the extended dispatch table (OPP adds the P suffix). */
static u16 opcode_of(void (*f)(void)) {
  for (unsigned i=0;i<512U;++i) if (FUNC_EXT_0[i]==f) return (u16)i;
  assert(!"handler not in table"); return 0;
}
#define OP(name)  opcode_of(name)
#define OPP(name) ((u16)(opcode_of(name)|0x1000U))
/* Low and high byte of a number. */
#define LO8(x) ((unsigned)(x) & 0xFFU)
#define HI8(x) (((unsigned)(x) >> 8) & 0xFFU)
/* One operand item: the words (t1 << 8 | low byte of n) and (t2 << 8 | high byte of n). */
#define I2(t1,t2,n) (u16)(((t1)<<8)|LO8(n)), (u16)(((t2)<<8)|HI8(n))
#define K(v)      I2(0x80,0x80,(v))
#define KL(v)     I2(0x80,0x80,(v)), I2(0x80,0x80,(unsigned)(v)>>16)
#define H(v)      I2(0x82,0x80,(v))
#define HL(v)     I2(0x82,0x80,(v)), I2(0x82,0x80,(unsigned)(v)>>16)
#define D(n)      I2(0x86,0x86,(n))
#define DL(n)     I2(0x86,0x86,(n)), I2(0x86,0x86,(n)+1)
#define D1K(n)    I2(0x86,0x88,(n))
#define TT(n)     I2(0x86,0x82,(n))
#define CC(n)     I2(0x86,0x84,(n))
#define CCL(n)    I2(0x86,0x84,(n)), I2(0x86,0x84,(n))
#define D80(n)    I2(0x86,0x80,(n)-8000)
#define BT(b)     I2(0x84,0x80,(b))
#define GRP(n,b)  I2(0x84,0x80+2*(n),(b))
#define GRPL(n,b) GRP(n,b), GRP(n,b)
#define PT(n)     I2(0x88,0x80,2*(n))
#define ZD(n,s)   I2(0xA6,0x86,(n)|((s)<<14))              /* D(n) + Zs (s = 0..3) */
/* One-word forms and fixed opcodes, named after the handler they exercise. */
#define W_LD(b)  (0x2000U|(b))
#define W_LDI(b) (0x3000U|(b))
#define W_AND(b) (0x4000U|(b))
#define W_ANI(b) (0x5000U|(b))
#define W_OR(b)  (0x6000U|(b))
#define W_ORI(b) (0x7000U|(b))
#define W_OUT(b) (0xC000U|(b))
#define W_SET(b) (0xD000U|(b))
#define W_RST(b) (0xE000U|(b))
#define W_STL(n) (0xF000U|(n))
#define W_RET    0xF700U
#define W_ANB    0xFFF8U
#define W_ORB    0xFFF9U
#define W_MPS    0xFFFAU
#define W_MRD    0xFFFBU
#define W_MPP    0xFFFCU
#define W_INV    0xFFFDU
/* SRET and FEND are the fixed words 0014 and 001C; END is 000F. */
#define W_SRET   0x0014U
#define W_FEND   0x001CU
#define W_END    0x000FU
#define ON_  0x01C2,0x8E00                                /* LD M8000 */
#define OFF_ 0x01C2,0x8000                                /* LD M0 (stays OFF in these tests) */
/* Assemble the words, append END and load the program. */
#define PRG(...) do { const u16 w_[]={__VA_ARGS__,0x000F}; load(w_,COUNT(w_)); } while (0)
/* Run one scan that must stop the PLC with the given error and leave the outputs cleared. */
#define FAULT(code) do { PLC_Main_Parser(); stopped(); assert(PLC_Runtime_LastError()==(code)); } while (0)
/* Linear bit addresses of M n, Y n, X n and S n (decimal numbering). */
#define MB(n) (n)
#define YB(n) (Addr_Y*8U+(n))
#define XB(n) (Addr_X*8U+(n))
#define SB(n) (Addr_S*8U+(n))
/* Word of data register Dn; d32() reads the register pair Dn, Dn+1 as one 32-bit value (low word
 * first) and set32() writes it. */
#define dreg(n) PLC_16BIT(Addr_D0000+2U*(n))
static u32 d32(unsigned n) { return (u32)dreg(n) | ((u32)dreg(n+1U) << 16); }
static void set32(unsigned n,u32 v) { dreg(n)=(u16)v; dreg(n+1U)=(u16)(v>>16); }
/* Reinterpret a float as its IEEE bit pattern and back. */
static u32 fbits(float f) { u32 u; memcpy(&u,&f,4); return u; }
static float fval(u32 u) { float f; memcpy(&f,&u,4); return f; }
/* Float comparison with a relative tolerance. */
#define NEAR(a,b) (fabsf((a)-(b)) < 1e-5f*fmaxf(1.0f,fabsf(b)))
/* State of a bit address in the PLC image. */
#define BIT(b) (PLC_BIT_TEST(b)!=0)

/* Numbers of the D8000-area registers (D80nn minus 8000) behind V0..V7 and Z0..Z7, written out
 * independently of the implementation. */
static const u8 TEST_V_REG[8]={29,183,185,187,189,191,193,195};
static const u8 TEST_Z_REG[8]={28,182,184,186,188,190,192,194};

/**
 * Index registers. V and Z are stored as pairs - Z0 at D8028 with V0 at D8029, Z1 at D8182 with V1
 * at D8183, and so on up to Z7 at D8194 with V7 at D8195. DZ0_Z3() and DZ4_Z7() combine a pair into
 * one 32-bit value with V in the high word. The pairs are spelled out here independently of the
 * implementation; Z2, Z3 and Z7 once paired the wrong registers.
 */
static void t_index_registers(void) {
  D8029=0xFEDC; D8028=0x1234; assert(DZ0_Z3(0)==0xFEDC1234U);
  D8183=0x8000; D8182=0xFFFF; assert(DZ0_Z3(1)==0x8000FFFFU);
  D8185=0xABCD; D8184=0x0102; assert(DZ0_Z3(2)==0xABCD0102U);
  D8187=0xFFFF; D8186=0x5678; assert(DZ0_Z3(3)==0xFFFF5678U);
  D8189=0x8001; D8188=0x0203; assert(DZ4_Z7(0)==0x80010203U);
  D8191=0xFEED; D8190=0xBEEF; assert(DZ4_Z7(1)==0xFEEDBEEFU);
  D8193=0xCAFE; D8192=0x7654; assert(DZ4_Z7(2)==0xCAFE7654U);
  D8195=0x1111; D8194=0x4321; assert(DZ4_Z7(3)==0x11114321U);
  assert(DZ0_Z3(4)==0 && DZ4_Z7(4)==0);
}

/**
 * First end-to-end programs: the time instructions TADD, TSUB and TWR, integer instructions with
 * CMP, RUN/STOP and the update inhibit, an extended X contact (address bit 12 must not be lost) and
 * PLS/PLF edges.
 */
static void t_smoke(void) {
  const u16 time_ops[]={0x01C2,0x8E00,
    0x0154,0x8600,0x8600,0x8603,0x8600,0x8606,0x8600,
    0x0156,0x8606,0x8600,0x8603,0x8600,0x8609,0x8600,
    0x015E,0x860C,0x8600,0x000F};
  load(time_ops,COUNT(time_ops));
  dreg(0)=10; dreg(1)=20; dreg(2)=30; dreg(3)=1; dreg(4)=2; dreg(5)=3;
  dreg(12)=2027; dreg(13)=11; dreg(14)=12; dreg(15)=13; dreg(16)=14; dreg(17)=15;
  good_scan();
  assert(dreg(6)==11 && dreg(7)==22 && dreg(8)==33);
  assert(dreg(9)==10 && dreg(10)==20 && dreg(11)==30);
  assert(rtc_arguments[0]==2027 && rtc_arguments[1]==11 && rtc_arguments[2]==12 &&
         rtc_arguments[3]==13 && rtc_arguments[4]==14 && rtc_arguments[5]==15);
  const u16 arithmetic[]={0x01C2,0x8E00,
    0x0028,0x8005,0x8000,0x8600,0x8600, /* MOV K5 D0 */
    0x0038,0x8600,0x8600,0x8003,0x8000,0x8601,0x8600, /* ADD D0 K3 D1 */
    0x0042,0x8601,0x8600, /* DEC D1 */
    0x0024,0x8003,0x8000,0x8003,0x8000,0x8400,0x8000, /* CMP K3 K3 M0 */
    0xCC00,0x000F};
  load(arithmetic,COUNT(arithmetic)); good_scan();
  assert(dreg(0)==5 && dreg(1)==7);
  assert(BIT(Addr_Y*8U));
  assert(!BIT(0) && BIT(1) && !BIT(2));
  PLC_Runtime_RequestRun(0); PLC_Main_Parser(); stopped();
  PLC_Runtime_RequestRun(1); good_scan();
  PLC_UPDATE_PROG=1; PLC_Main_Parser(); stopped();
  /* Extended X operand must not alias M0 by losing address bit 12. */
  const u16 contact[]={0x01C2,0x9200,0xCC00,0x000F};
  load(contact,COUNT(contact)); PLC_BIT_ON(Addr_X*8U); good_scan();
  assert(BIT(Addr_Y*8U) && !BIT(0));
  const u16 pulses[]={W_LD(0),OP(PLS),0x8000|YB(0),OP(PLF),0x8000|YB(1),W_END};
  load(pulses,COUNT(pulses)); good_scan();
  assert(!BIT(Addr_Y*8U) && !BIT(Addr_Y*8U+1));
  PLC_BIT_ON(0); good_scan(); assert(BIT(Addr_Y*8U));
  good_scan(); assert(!BIT(Addr_Y*8U));
  PLC_BIT_OFF(0); good_scan(); assert(BIT(Addr_Y*8U+1));
  good_scan(); assert(!BIT(Addr_Y*8U+1));
}

/* Instructions that stop the PLC by design: hardware-dependent or without verified semantics. */
static void (* const rejected[])(void)={REF,REFF,MTR,DHSCS,DHSCR,DHSZ,SPD,DSPD,PLSY,PWM,PLSR,IST,INCD,TTMR,
  STMR,RAMP,ROTS,TKY,HKY,DSW,SEGL,ARWS,ASC,PR,FROM,TO,RS,PRUN,ASCI,HEX,VRRD,VRSC,RS2,PID,
  DESTR,DEVAL,EBCD,EBIN,RD3A,WR3A,IRET,EI,DI};
/* Need program context (labels, FOR stack) or constrained multi-operand shapes that random
 * operands practically never form; they have dedicated known-answer tests above. */
static void (* const known_answer_only[])(void)={CJ,CALL,SRET,NEXT,SMOV,SFTR,SFTL,ANS,SORT2};
/** Is the handler @p f one of the first @p n entries of @p list? */
static int listed(void (*f)(void),void (* const *list)(void),unsigned n) {
  for (unsigned i=0;i<n;++i) if (list[i]==f) return 1;
  return 0;
}
/**
 * Programs that must stop the PLC: POP (not implemented), an endless CJ loop (dispatch budget),
 * a bad operand, a program without END, every rejected instruction and a reserved opcode.
 */
static void t_faults(void) {
  const u16 pop[]={0xFFFF,0x000F};
  load(pop,COUNT(pop)); FAULT(PLC_RUNTIME_LEGACY_UNSUPPORTED);
  const u16 jump_loop[]={0xB000,0x01C2,0x8E00,0x0010,0x8800,0x8000,0x000F};
  load(jump_loop,COUNT(jump_loop)); FAULT(PLC_RUNTIME_LEGACY_BUDGET);
  const u16 bad_address[]={0x01C2,0x8E00,0x0028,0x8001,0x8000,0x86FF,0x86FF,0x000F};
  load(bad_address,COUNT(bad_address)); FAULT(PLC_RUNTIME_OPERAND);   /* D65535 is rejected by the operand decoder */
  const u16 only_end[]={0x000F}; load(only_end,COUNT(only_end)); good_scan();
  memset((void *)PLC_START_ADDR,0,MAXSTEP*2U);
  PLC_Main_Parser(); stopped(); assert(PLC_Runtime_LastError()==PLC_RUNTIME_BOUNDS);
  /* Rejected instructions: hardware-dependent or without verified semantics. */
  for (unsigned i=0;i<COUNT(rejected);++i) {
    PRG(ON_,OP(rejected[i]),K(1),K(2),K(3),K(4),K(5));
    FAULT(PLC_RUNTIME_LEGACY_UNSUPPORTED);
  }
  PRG(ON_,0x0A28,K(1),D(0)); FAULT(PLC_RUNTIME_OPCODE);      /* reserved opcode bits 9..11 */
}

/* ------------------------------------------------------------------------- */
/**
 * Operand decoding: skipping the operands of a false rung, the D1000, T, C and D8000 areas, Z
 * indexing (including negative indices), KnM groups for reading and writing, and the illegal
 * forms.
 */
static void t_operands(void) {
  /* A false rung must skip exactly the operand words: 32-bit K takes four words. */
  PRG(OFF_,OP(DMOV),KL(100000),DL(0), OFF_,OP(MUL),K(3),K(4),D(2), ON_,OP(MOV),K(5),D(9));
  good_scan(); assert(d32(0)==0 && d32(2)==0 && dreg(9)==5);
  /* D1000 area, T and C operands, D8000 area */
  PRG(ON_,OP(MOV),K(77),D1K(5), ON_,OP(MOV),K(12),TT(3), ON_,OP(MOV),K(9),CC(4), ON_,OP(MOV),K(33),D80(8010));
  good_scan();
  assert(PLC_16BIT(Addr_D0000+2U*1005U)==77 && PLC_16BIT(Addr_TVal+6U)==12 &&
         PLC_16BIT(Addr_CVal+8U)==9 && D8010==33);
  /* Z index: D(2)+Z0 and K+Z0, then V0-indexed D2 read through the same decoder */
  PRG(ON_,OP(MOV),K(5),I2(0x86,0x86,0)/*D0*/, ON_,OP(MOV),D(0),ZD(10,0), ON_,OP(MOV),I2(0xA0,0x80,100),D(1));
  D8028=3; good_scan();
  assert(dreg(13)==5 && dreg(1)==103);
  D8028=0xFFFF;                                                  /* Z = -1 */
  PRG(ON_,OP(MOV),ZD(10,0),D(0)); dreg(9)=0x4242; D8028=0xFFFF; good_scan(); assert(dreg(0)==0x4242);
  D8028=0xFFF6;                                                  /* Z = -10 -> D0: allowed */
  PRG(ON_,OP(MOV),ZD(10,0),D(1)); dreg(0)=0x1111; D8028=0xFFF6; good_scan(); assert(dreg(1)==0x1111);
  D8028=0xFFF5;                                                  /* D(-1): rejected, nothing written */
  PRG(ON_,OP(MOV),ZD(10,0),D(1)); D8028=0xFFF5; FAULT(PLC_RUNTIME_OPERAND);
  /* Bit groups: K4M0 read and K2Y0 / K4M16 write */
  PRG(ON_,OP(MOV),H(0xA5C3),GRP(4,0), ON_,OP(MOV),GRP(4,0),D(0), ON_,OP(MOV),H(0x3C),GRP(2,Addr_Y*8U));
  good_scan();
  assert(dreg(0)==0xA5C3 && PLC_8BIT(0)==0xC3 && PLC_8BIT(1)==0xA5 && PLC_8BIT(Addr_Y)==0x3C);
  PRG(ON_,OP(DMOV),HL(0x12345678UL),DL(2), ON_,OP(DMOV),DL(2),GRPL(8,0));
  good_scan(); assert(d32(2)==0x12345678UL && PLC_8BIT(0)==0x78 && PLC_8BIT(3)==0x12);
  /* K5 in a 16-bit instruction, a group crossing an area, and a write to X are all rejected */
  PRG(ON_,OP(MOV),GRP(5,0),D(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(MOV),K(1),GRP(4,Addr_Y*8U-8U)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(MOV),K(1),GRP(4,Addr_X*8U)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(MOV),K(1),K(2)); FAULT(PLC_RUNTIME_OPERAND);       /* constant destination */
  PRG(ON_,OP(MOV),K(1),D(2000)); FAULT(PLC_RUNTIME_OPERAND);    /* beyond D1999 */
  PRG(ON_,OP(MOV),K(1),CC(250)); FAULT(PLC_RUNTIME_OPERAND);    /* C250 is 32-bit only */
  PRG(ON_,OP(DMOV),KL(7),CCL(210)); good_scan(); assert(PLC_32BIT(Addr_CHVal+40U)==7);
  /* a 32-bit operand where the second item is not marked is malformed */
  { const u16 w_[]={ON_,OP(DMOV),I2(0x80,0x80,1),0x0001,0x0001,D(0),0x000F}; load(w_,COUNT(w_)); FAULT(PLC_RUNTIME_OPERAND); }
}

/**
 * Comparison and transfer: CMP and ZCP (16 and 32 bit), MOV, CML, SMOV, BMOV (overlap-safe), FMOV,
 * XCH, BCD, BIN and the pulse form (P suffix) that runs once per rising edge.
 */
static void t_move_compare(void) {
  PRG(ON_,OP(CMP),K(-5),K(3),BT(MB(10)), ON_,OP(CMP),K(3),K(3),BT(MB(20)), ON_,OP(CMP),K(9),K(3),BT(MB(30)));
  good_scan();
  assert(!BIT(10) && !BIT(11) && BIT(12));      /* -5 < 3 */
  assert(!BIT(20) && BIT(21) && !BIT(22));
  assert(BIT(30) && !BIT(31) && !BIT(32));
  PRG(ON_,OP(DCMP),KL(-100000),KL(7),BT(MB(40)));  good_scan(); assert(!BIT(40) && !BIT(41) && BIT(42));
  PRG(ON_,OP(ZCP),K(10),K(20),K(15),BT(MB(50)), ON_,OP(ZCP),K(10),K(20),K(5),BT(MB(60)), ON_,OP(ZCP),K(10),K(20),K(25),BT(MB(70)));
  good_scan(); assert(!BIT(50)&&BIT(51)&&!BIT(52) && BIT(60)&&!BIT(61)&&!BIT(62) && !BIT(70)&&!BIT(71)&&BIT(72));
  PRG(ON_,OP(DZCP),KL(-5),KL(5),KL(0),BT(MB(80))); good_scan(); assert(!BIT(80)&&BIT(81)&&!BIT(82));
  PRG(ON_,OP(MOV),K(-2),D(0), ON_,OP(DMOV),KL(-123456),DL(2), ON_,OP(CML),H(0x00FF),D(4), ON_,OP(DCML),HL(0xFFFF0000UL),DL(6));
  good_scan();
  assert(dreg(0)==0xFFFE && d32(2)==(u32)-123456 && dreg(4)==0xFF00 && d32(6)==0x0000FFFFUL);
  /* SMOV: digits 4..3 of 1234 into digits 3..2 of 5678 -> 5128 */
  PRG(ON_,OP(SMOV),D(0),K(4),K(2),D(1),K(3));
  dreg(0)=1234; dreg(1)=5678; good_scan(); assert(dreg(1)==5128);
  /* BMOV, including an overlapping upward copy */
  PRG(ON_,OP(BMOV),D(0),D(10),K(4)); for (unsigned i=0;i<4;++i) dreg(i)=100+i; good_scan();
  for (unsigned i=0;i<4;++i) assert(dreg(10+i)==100U+i);
  PRG(ON_,OP(BMOV),D(0),D(1),K(5)); for (unsigned i=0;i<5;++i) dreg(i)=10+i; good_scan();
  assert(dreg(0)==10 && dreg(1)==10 && dreg(2)==11 && dreg(3)==12 && dreg(4)==13 && dreg(5)==14);
  PRG(ON_,OP(BMOV),D(1),D(0),K(5)); for (unsigned i=0;i<6;++i) dreg(i)=20+i; good_scan();
  assert(dreg(0)==21 && dreg(4)==25 && dreg(5)==25);
  PRG(ON_,OP(BMOV),GRP(4,0),D(0),K(2)); PLC_8BIT(0)=0x34; PLC_8BIT(1)=0x12; PLC_8BIT(2)=0x78; PLC_8BIT(3)=0x56;
  good_scan(); assert(dreg(0)==0x1234 && dreg(1)==0x5678);
  PRG(ON_,OP(BMOV),D(0),D(1),K(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(FMOV),K(7),D(20),K(5), ON_,OP(DFMOV),KL(0x10001),DL(30),K(3));
  good_scan(); for (unsigned i=0;i<5;++i) assert(dreg(20+i)==7); assert(dreg(25)==0);
  assert(d32(30)==0x10001UL && d32(32)==0x10001UL && d32(34)==0x10001UL && d32(36)==0);
  PRG(ON_,OP(XCH),D(0),D(1), ON_,OP(DXCH),DL(4),DL(6), ON_,OP(XCH),D(8),D(8));
  dreg(0)=1; dreg(1)=2; set32(4,0xAAAABBBBUL); set32(6,0x11112222UL); dreg(8)=9; good_scan();
  assert(dreg(0)==2 && dreg(1)==1 && d32(4)==0x11112222UL && d32(6)==0xAAAABBBBUL && dreg(8)==9);
  PRG(ON_,OP(BCD),K(1234),D(0), ON_,OP(DBCD),KL(12345678),DL(2), ON_,OP(BIN),H(0x9999),D(4), ON_,OP(DBIN),HL(0x12345678UL),DL(6));
  good_scan(); assert(dreg(0)==0x1234 && d32(2)==0x12345678UL && dreg(4)==9999 && d32(6)==12345678UL);
  PRG(ON_,OP(BCD),K(10000),D(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(BCD),K(-1),D(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(BIN),H(0x12AB),D(0)); FAULT(PLC_RUNTIME_OPERAND);
  /* pulse execution (P suffix) fires once per rising edge */
  const u16 pls[]={0x01C2,0x9200,OPP(INC),D(0),0x000F};
  load(pls,COUNT(pls)); good_scan(); assert(dreg(0)==0);
  PLC_BIT_ON(XB(0)); good_scan(); assert(dreg(0)==1); good_scan(); assert(dreg(0)==1);
  PLC_BIT_OFF(XB(0)); good_scan(); PLC_BIT_ON(XB(0)); good_scan(); assert(dreg(0)==2);
}

/**
 * Arithmetic and logic with the flags M8020 (zero), M8021 (borrow) and M8022 (carry): ADD, SUB, MUL,
 * DIV (16 and 32 bit, widths of the results, INT_MIN / -1), INC, DEC, NEG, WAND, WOR and WXOR.
 */
static void t_arith(void) {
  PRG(ON_,OP(SUB),K(10),K(3),D(0), ON_,OP(SUB),D(0),K(10),D(1));               /* operand order matters */
  good_scan(); assert(dreg(0)==7 && dreg(1)==0xFFFD);
  PRG(ON_,OP(ADD),K(30000),K(30000),D(0)); good_scan();
  assert(dreg(0)==60000 && BIT(M8022) && !BIT(M8021) && !BIT(M8020));
  PRG(ON_,OP(ADD),K(-30000),K(-30000),D(0)); good_scan();
  assert(dreg(0)==5536 && !BIT(M8022) && BIT(M8021));
  PRG(ON_,OP(ADD),K(5),K(-5),D(0)); good_scan(); assert(dreg(0)==0 && BIT(M8020) && !BIT(M8021) && !BIT(M8022));
  PRG(ON_,OP(SUB),K(-32768),K(1),D(0)); good_scan(); assert(dreg(0)==32767 && BIT(M8021));
  PRG(ON_,OP(DADD),KL(2000000000L),KL(2000000000L),DL(0)); good_scan();
  assert(d32(0)==4000000000UL && BIT(M8022));
  PRG(ON_,OP(DSUB),KL(100000),KL(200000),DL(0)); good_scan(); assert(d32(0)==(u32)-100000 && !BIT(M8021));
  PRG(ON_,OP(DSUB),KL(-2000000000L),KL(2000000000L),DL(0)); good_scan(); assert(BIT(M8021));
  PRG(ON_,OP(MUL),K(300),K(400),D(0), ON_,OP(MUL),K(-3),K(7),D(2));
  good_scan(); assert(d32(0)==120000UL && d32(2)==(u32)-21);
  PRG(ON_,OP(MUL),K(300),K(400),GRP(8,0)); good_scan(); assert(PLC_8BIT(0)==0xC0 && PLC_8BIT(1)==0xD4 && PLC_8BIT(2)==1);
  PRG(ON_,OP(DMUL),KL(100000),KL(100000),DL(0)); good_scan();
  assert(d32(0)==0x540BE400UL && d32(2)==2UL);
  PRG(ON_,OP(DMUL),KL(-100000),KL(100000),DL(0)); good_scan();
  assert(d32(0)==0xABF41C00UL && d32(2)==0xFFFFFFFDUL);
  PRG(ON_,OP(DIV),K(-7),K(2),D(0), ON_,OP(DIV),K(7),K(-2),D(2), ON_,OP(DIV),K(100),K(7),D(4));
  good_scan(); assert(dreg(0)==0xFFFD && dreg(1)==0xFFFF && dreg(2)==0xFFFD && dreg(3)==1 && dreg(4)==14 && dreg(5)==2);
  PRG(ON_,OP(DDIV),KL(100000),KL(7),DL(0)); good_scan(); assert(d32(0)==14285UL && d32(2)==5UL);
  PRG(ON_,OP(DIV),K(1),K(0),D(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(DDIV),KL(1),KL(0),DL(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(DIV),K(-32768),K(-1),D(0)); good_scan(); assert(dreg(0)==0x8000 && dreg(1)==0);   /* no UB at INT_MIN/-1 */
  PRG(ON_,OP(DDIV),KL(-2147483647L-1L),KL(-1),DL(0)); good_scan(); assert(d32(0)==0x80000000UL && d32(2)==0);
  PRG(ON_,OP(INC),D(0), ON_,OP(DEC),D(1), ON_,OP(DINC),DL(2), ON_,OP(DDEC),DL(4), ON_,OP(NEG),D(6), ON_,OP(DNEG),DL(8));
  dreg(0)=0x7FFF; dreg(1)=0; set32(2,0xFFFFFFFFUL); set32(4,0); dreg(6)=5; set32(8,100000); good_scan();
  assert(dreg(0)==0x8000 && dreg(1)==0xFFFF && d32(2)==0 && d32(4)==0xFFFFFFFFUL && dreg(6)==0xFFFB && d32(8)==(u32)-100000);
  PRG(ON_,OP(WAND),H(0xF0F0),H(0x3C3C),D(0), ON_,OP(WOR),H(0xF0F0),H(0x3C3C),D(1), ON_,OP(WXOR),H(0xF0F0),H(0x3C3C),D(2),
      ON_,OP(DWAND),HL(0xF0F0F0F0UL),HL(0x0FF00FF0UL),DL(4), ON_,OP(DWOR),HL(0xF0F00000UL),HL(0x0000F0F0UL),DL(6),
      ON_,OP(DWXOR),HL(0xFFFF0000UL),HL(0xFF00FF00UL),DL(8));
  good_scan(); assert(dreg(0)==0x3030 && dreg(1)==0xFCFC && dreg(2)==0xCCCC &&
                      d32(4)==0x00F000F0UL && d32(6)==0xF0F0F0F0UL && d32(8)==0x00FFFF00UL);
  /* a false rung leaves flags and data alone */
  PRG(OFF_,OP(ADD),K(1),K(1),D(0)); good_scan(); assert(dreg(0)==0 && !BIT(M8020));
}

/**
 * Rotation and shift: ROR and ROL (the last bit rotated out is copied to the carry M8022), RCR and
 * RCL (rotation through the carry), the bit shifts SFTL and SFTR, the word shifts WSFL and WSFR, and
 * the FIFO pair SFWR / SFWL.
 */
static void t_rotate_shift(void) {
  PRG(ON_,OP(ROR),D(0),K(1)); dreg(0)=0x000F; good_scan(); assert(dreg(0)==0x8007 && BIT(M8022));
  PRG(ON_,OP(ROL),D(0),K(1)); dreg(0)=0x8001; good_scan(); assert(dreg(0)==0x0003 && BIT(M8022));
  PRG(ON_,OP(ROR),D(0),K(4)); dreg(0)=0x1234; good_scan(); assert(dreg(0)==0x4123 && !BIT(M8022));
  PRG(ON_,OP(RCR),D(0),K(1), ON_,OP(RCR),D(0),K(1)); dreg(0)=1; good_scan(); assert(dreg(0)==0x8000 && !BIT(M8022));
  PRG(ON_,OP(RCL),D(0),K(1)); dreg(0)=0x8000; good_scan(); assert(dreg(0)==0 && BIT(M8022));
  PRG(ON_,OP(RCL),D(0),K(2)); dreg(0)=0x4000; PLC_BIT_ON(M8022); good_scan(); assert(dreg(0)==0x0002 && BIT(M8022));
  PRG(ON_,OP(DROR),DL(0),K(8)); set32(0,0x12345678UL); good_scan(); assert(d32(0)==0x78123456UL);
  PRG(ON_,OP(DROL),DL(0),K(4)); set32(0,0xF0000001UL); good_scan(); assert(d32(0)==0x0000001FUL && BIT(M8022));
  PRG(ON_,OP(DRCR),DL(0),K(1)); set32(0,2); good_scan(); assert(d32(0)==1 && !BIT(M8022));
  PRG(ON_,OP(DRCL),DL(0),K(1)); set32(0,0x80000000UL); good_scan(); assert(d32(0)==0 && BIT(M8022));
  PRG(ON_,OP(ROR),D(0),K(17)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(ROR),GRP(2,0),K(1)); FAULT(PLC_RUNTIME_OPERAND);                    /* needs K4 */
  PRG(ON_,OP(ROR),GRP(4,0),K(4)); PLC_8BIT(0)=0x21; PLC_8BIT(1)=0x43; good_scan(); assert(PLC_8BIT(0)==0x32 && PLC_8BIT(1)==0x14);
  /* bit shifts: M20..M35 (n1 = 16), source M100..M103 = 1,0,1,1 */
  static const u8 src[4]={1,0,1,1};
  PRG(ON_,OP(SFTL),BT(MB(100)),BT(MB(20)),K(16),K(4)); { u8 old[16];
    for (unsigned i=0;i<16;++i) { old[i]=(u8)((i*7+3)%2); plc_exp_put_bit(20+i,old[i]); }
    for (unsigned i=0;i<4;++i) plc_exp_put_bit(100+i,src[i]);
    good_scan();
    for (unsigned i=0;i<16;++i) assert(BIT(20+i)==(i<4 ? src[i] : old[i-4])); }
  PRG(ON_,OP(SFTR),BT(MB(100)),BT(MB(20)),K(16),K(4)); { u8 old[16];
    for (unsigned i=0;i<16;++i) { old[i]=(u8)((i*5+1)%3==0); plc_exp_put_bit(20+i,old[i]); }
    for (unsigned i=0;i<4;++i) plc_exp_put_bit(100+i,src[i]);
    good_scan();
    for (unsigned i=0;i<16;++i) assert(BIT(20+i)==(i<12 ? old[i+4] : src[i-12])); }
  PRG(ON_,OP(SFTR),BT(MB(100)),BT(MB(20)),K(4),K(5)); FAULT(PLC_RUNTIME_OPERAND);     /* n2 > n1 */
  /* word shifts */
  PRG(ON_,OP(WSFL),D(0),D(10),K(5),K(2)); for (unsigned i=0;i<5;++i) dreg(10+i)=100+i; dreg(0)=7; dreg(1)=8; good_scan();
  assert(dreg(10)==7 && dreg(11)==8 && dreg(12)==100 && dreg(13)==101 && dreg(14)==102);
  PRG(ON_,OP(WSFR),D(0),D(10),K(5),K(2)); for (unsigned i=0;i<5;++i) dreg(10+i)=100+i; dreg(0)=7; dreg(1)=8; good_scan();
  assert(dreg(10)==102 && dreg(11)==103 && dreg(12)==104 && dreg(13)==7 && dreg(14)==8);
  /* FIFO: SFWR then SFWL (read) */
  PRG(ON_,OP(SFWR),D(0),D(10),K(4), ON_,OP(SFWR),D(1),D(10),K(4), ON_,OP(SFWR),D(2),D(10),K(4), ON_,OP(SFWR),D(3),D(10),K(4));
  dreg(0)=11; dreg(1)=22; dreg(2)=33; dreg(3)=44; good_scan();
  assert(dreg(10)==3 && dreg(11)==11 && dreg(12)==22 && dreg(13)==33 && BIT(M8022));   /* 4th write rejected: full */
  PRG(ON_,OP(SFWL),D(10),D(20),K(4)); dreg(10)=3; dreg(11)=11; dreg(12)=22; dreg(13)=33; good_scan();
  assert(dreg(20)==11 && dreg(10)==2 && dreg(11)==22 && dreg(12)==33 && !BIT(M8020));
  PRG(ON_,OP(SFWL),D(10),D(20),K(4)); dreg(10)=0; good_scan(); assert(BIT(M8020) && dreg(20)==0);
}

/**
 * Data processing: ZRST, DECO, ENCO, SUM, BON, MEAN, SQR, ANS / ANR, ALT, SEGD, SWAP, GRY, GBIN, UNI,
 * DIS, WTOB, BTOW, WSUM, SORT, SORT2, SER, ABSD, CCD and ZPUSH / ZPOP.
 */
static void t_data(void) {
  PRG(ON_,OP(ZRST),D(10),D(14), ON_,OP(ZRST),BT(MB(60)),BT(MB(70)), ON_,OP(ZRST),TT(2),TT(3), ON_,OP(ZRST),CC(1),CC(2));
  for (unsigned i=0;i<20;++i) dreg(i)=0xAAAA;
  for (unsigned i=58;i<73;++i) PLC_BIT_ON(i);
  PLC_16BIT(Addr_TVal+4U)=9; PLC_16BIT(Addr_CVal+2U)=5; PLC_BIT_ON(Addr_CO*8U+1U);
  good_scan();
  for (unsigned i=0;i<20;++i) assert(dreg(i)==((i>=10 && i<=14) ? 0 : 0xAAAA));
  for (unsigned i=58;i<73;++i) assert(BIT(i)==(i<60 || i>70));
  assert(PLC_16BIT(Addr_TVal+4U)==0 && PLC_16BIT(Addr_CVal+2U)==0 && !BIT(Addr_CO*8U+1U));
  PRG(ON_,OP(ZRST),D(5),D(4)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(ZRST),D(5),TT(7)); FAULT(PLC_RUNTIME_OPERAND);
  /* DECO / ENCO */
  PRG(ON_,OP(DECO),K(5),BT(MB(100)),K(3), ON_,OP(DECO),K(9),D(0),K(4), ON_,OP(DECO),K(0x1FD),BT(MB(120)),K(3));
  good_scan();
  for (unsigned i=0;i<8;++i) assert(BIT(100+i)==(i==5));
  assert(dreg(0)==(1U<<9));
  for (unsigned i=0;i<8;++i) assert(BIT(120+i)==(i==5));           /* 0x1FD masked to 3 bits = 5 */
  PRG(ON_,OP(ENCO),BT(MB(100)),D(0),K(3), ON_,OP(ENCO),D(1),D(2),K(4), ON_,OP(ENCO),BT(MB(110)),BT(MB(120)),K(3));
  PLC_BIT_ON(100+1); PLC_BIT_ON(100+6); dreg(1)=0x0408; PLC_BIT_ON(110+3); good_scan();
  assert(dreg(0)==6 && dreg(2)==10 && BIT(120) && BIT(121) && !BIT(122));
  PRG(ON_,OP(ENCO),D(1),D(2),K(4)); dreg(1)=0; FAULT(PLC_RUNTIME_OPERAND);
  /* SUM / BON / MEAN / SQR */
  PRG(ON_,OP(SUM),H(0xF0F1),D(0), ON_,OP(DSUM),HL(0xFFFFFFFFUL),DL(2), ON_,OP(SUM),K(0),D(4));
  good_scan(); assert(dreg(0)==9 && d32(2)==32 && dreg(4)==0 && BIT(M8020));
  PRG(ON_,OP(BON),H(0x0100),BT(MB(30)),K(8), ON_,OP(BON),H(0x0100),BT(MB(31)),K(7), ON_,OP(DBON),HL(0x80000000UL),BT(MB(32)),K(31));
  good_scan(); assert(BIT(30) && !BIT(31) && BIT(32));
  PRG(ON_,OP(BON),H(1),BT(MB(30)),K(16)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(MEAN),D(0),D(10),K(3), ON_,OP(DMEAN),DL(20),DL(30),K(2));
  dreg(0)=10; dreg(1)=20; dreg(2)=33; set32(20,100000); set32(22,-300000); good_scan();
  assert(dreg(10)==21 && d32(30)==(u32)-100000);
  PRG(ON_,OP(MEAN),D(0),D(10),K(65)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(SQR),K(144),D(0)); good_scan(); assert(dreg(0)==12 && !BIT(M8021) && !BIT(M8020));
  PRG(ON_,OP(SQR),K(150),D(1)); good_scan(); assert(dreg(1)==12 && BIT(M8021) && !BIT(M8020));   /* truncated */
  PRG(ON_,OP(DSQR),KL(1000000000L),DL(2)); good_scan(); assert(d32(2)==31622UL && BIT(M8021));
  PRG(ON_,OP(SQR),K(0),D(4)); good_scan(); assert(dreg(4)==0 && BIT(M8020) && !BIT(M8021));
  PRG(ON_,OP(SQR),K(-4),D(0)); FAULT(PLC_RUNTIME_OPERAND);
  /* ANS / ANR */
  PRG(0x01C2,0x9200,OP(ANS),TT(5),K(3),BT(SB(905)));                  /* LD X0 */
  PLC_BIT_ON(M8049); PLC_BIT_ON(XB(0));
  plc_exp_milliseconds=0; good_scan(); assert(!BIT(SB(905)));
  plc_exp_milliseconds=250; good_scan(); assert(!BIT(SB(905)));
  plc_exp_milliseconds=300; good_scan(); assert(BIT(SB(905)) && BIT(M8048) && D8049==905);
  PRG(0x01C2,0x9200,OP(ANS),TT(5),K(3),BT(SB(905)));
  PLC_BIT_ON(XB(0)); plc_exp_milliseconds=0; good_scan(); plc_exp_milliseconds=200; good_scan();
  PLC_BIT_OFF(XB(0)); plc_exp_milliseconds=400; good_scan();                  /* released early: timer resets */
  PLC_BIT_ON(XB(0)); plc_exp_milliseconds=500; good_scan(); plc_exp_milliseconds=600; good_scan(); assert(!BIT(SB(905)));
  PRG(ON_,OP(ANR)); PLC_BIT_ON(SB(950)); PLC_BIT_ON(SB(920)); PLC_BIT_ON(M8049); good_scan();
  assert(!BIT(SB(920)) && BIT(SB(950)) && D8049==950 && BIT(M8048));
  PRG(ON_,OP(ANS),TT(5),K(3),BT(MB(5))); FAULT(PLC_RUNTIME_OPERAND);
  /* ALT / SEGD / SWAP / GRY / GBIN / UNI / DIS / WTOB / BTOW / WSUM */
  PRG(ON_,OP(ALT),BT(MB(40)), ON_,OP(SEGD),K(0x1B),D(0), ON_,OP(SEGD),K(8),D(1), ON_,OP(SWAP),D(2), ON_,OP(DSWAP),DL(4));
  dreg(2)=0x12AB; set32(4,0x1122AABBUL); good_scan();
  assert(BIT(40) && dreg(0)==0x7C && dreg(1)==0x7F && dreg(2)==0xAB12);
  assert(d32(4)==0x2211BBAAUL);
  good_scan(); assert(!BIT(40));
  PRG(ON_,OP(GRY),K(5),D(0), ON_,OP(GBIN),H(7),D(1), ON_,OP(DGRY),KL(100000),DL(2), ON_,OP(DGBIN),HL(0x80000000UL),DL(4));
  good_scan(); assert(dreg(0)==7 && dreg(1)==5 && d32(2)==(100000UL^(100000UL>>1)) && d32(4)==0xFFFFFFFFUL);
  for (unsigned v=0;v<2000U;v+=7) {                                  /* GBIN inverts GRY */
    PRG(ON_,OP(GRY),D(0),D(1), ON_,OP(GBIN),D(1),D(2)); dreg(0)=(u16)(v*31U); good_scan(); assert(dreg(2)==(u16)(v*31U)); }
  PRG(ON_,OP(UNI),D(0),D(10),K(3), ON_,OP(DIS),D(10),D(20),K(4));
  dreg(0)=0x1234; dreg(1)=0x5678; dreg(2)=0x9ABC; good_scan();
  assert(dreg(10)==0xC84U && dreg(20)==4 && dreg(21)==8 && dreg(22)==0xC && dreg(23)==0);
  PRG(ON_,OP(WTOB),D(0),D(10),K(3), ON_,OP(BTOW),D(20),D(30),K(3));
  dreg(0)=0x2211; dreg(1)=0x4433; dreg(20)=0x0A; dreg(21)=0x0B; dreg(22)=0x1C; dreg(23)=0xFF;
  good_scan(); assert(dreg(10)==0x11 && dreg(11)==0x22 && dreg(12)==0x33);
  assert(dreg(30)==0x0B0A && dreg(31)==0x001C);
  PRG(ON_,OP(WSUM),D(0),D(10),K(3), ON_,OP(DWSUM),DL(20),DL(30),K(2));   /* 16-bit form: one item per operand */
  dreg(0)=30000; dreg(1)=30000; dreg(2)=30000; set32(20,2000000000UL); set32(22,2000000000UL); good_scan();
  assert(d32(10)==90000UL && d32(30)==0xEE6B2800UL && d32(32)==0);
  /* SORT / SER / ABSD / CCD / ZPUSH / ZPOP */
  PRG(ON_,OP(SORT),D(0),K(4),K(2),D(20),K(2));
  { static const u16 tbl[8]={3,30, 1,10, 4,40, 2,20}; for (unsigned i=0;i<8;++i) dreg(i)=tbl[i]; }
  good_scan(); { static const u16 exp[8]={1,10, 2,20, 3,30, 4,40};      /* sorted by column 2 */
    for (unsigned i=0;i<8;++i) assert(dreg(20+i)==exp[i]); }
  assert(BIT(M8029));
  PRG(ON_,OP(SORT2),D(0),K(3),K(1),D(20),K(1)); dreg(0)=5; dreg(1)=9; dreg(2)=1; PLC_BIT_ON(M8165); good_scan();
  assert(dreg(20)==9 && dreg(21)==5 && dreg(22)==1);
  PRG(ON_,OP(DSORT2),DL(0),K(3),K(1),DL(20),K(1)); set32(0,100000); set32(2,-5); set32(4,70000); good_scan();
  assert(d32(20)==(u32)-5 && d32(22)==70000UL && d32(24)==100000UL);
  PRG(ON_,OP(SORT),D(0),K(2),K(2),D(20),K(3)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(SER),D(0),K(7),D(20),K(6));
  { static const u16 v[6]={5,7,2,7,9,2}; for (unsigned i=0;i<6;++i) dreg(i)=v[i]; }
  good_scan(); assert(dreg(20)==2 && dreg(21)==1 && dreg(22)==3 && dreg(23)==5 && dreg(24)==4);
  PRG(ON_,OP(SER),D(0),K(100),D(20),K(3)); dreg(0)=1; dreg(1)=2; dreg(2)=3; good_scan();
  assert(dreg(20)==0 && dreg(21)==0 && dreg(22)==0 && dreg(23)==0 && dreg(24)==2);
  PRG(ON_,OP(DSER),DL(0),KL(-70000),DL(20),K(2)); set32(0,-70000); set32(2,5); good_scan();
  assert(d32(20)==1 && d32(22)==0 && d32(24)==0 && d32(26)==0 && d32(28)==1);
  PRG(ON_,OP(ABSD),D(0),CC(0),BT(MB(60)),K(2));
  dreg(0)=10; dreg(1)=20; dreg(2)=50; dreg(3)=30; PLC_16BIT(Addr_CVal)=15; good_scan();
  assert(BIT(60) && BIT(61));                                            /* 10<=15<20; pair 2 wraps: ON >= 50 or < 30 */
  PLC_16BIT(Addr_CVal)=40; good_scan(); assert(!BIT(60) && !BIT(61));
  PLC_16BIT(Addr_CVal)=60; good_scan(); assert(!BIT(60) && BIT(61));
  PLC_16BIT(Addr_CVal)=5; good_scan(); assert(!BIT(60) && BIT(61));
  PLC_16BIT(Addr_CVal)=35; good_scan(); assert(!BIT(60) && !BIT(61));
  PRG(ON_,OP(CCD),D(0),D(10),K(5));
  dreg(0)=0x0201; dreg(1)=0x0403; dreg(2)=0x00FF; good_scan(); assert(dreg(10)==(1+2+3+4+0xFF) && dreg(11)==(1^2^3^4^0xFF));
  PRG(ON_,OP(CCD),D(0),D(10),K(3)); PLC_BIT_ON(M8161); dreg(0)=0x11; dreg(1)=0x2233; dreg(2)=0x44; good_scan();
  assert(dreg(10)==0x11+0x33+0x44 && dreg(11)==(0x11^0x33^0x44));
  PRG(ON_,OP(ZPUSH),D(0), ON_,OP(ZPUSH),D(0), ON_,OP(ZPOP),D(0));
  for (unsigned r=0;r<8;++r) { PLC_16BIT(Addr_D8000+2U*TEST_Z_REG[r])=(u16)(100+r); PLC_16BIT(Addr_D8000+2U*TEST_V_REG[r])=(u16)(200+r); }
  good_scan(); assert(dreg(0)==1 && dreg(1)==100 && dreg(2)==200 && dreg(3)==101 && dreg(16)==207);
  PRG(ON_,OP(ZPOP),D(0)); FAULT(PLC_RUNTIME_OPERAND);
}

/**
 * Floating point: arithmetic, conversion, comparison and the math functions against known values,
 * and the arguments that must fault (division by zero, overflow, domain errors).
 */
static void t_float(void) {
  PRG(ON_,OP(DEADD),KL(fbits(1.5f)),KL(fbits(2.25f)),DL(0), ON_,OP(DESUB),KL(fbits(1.5f)),KL(fbits(2.25f)),DL(2),
      ON_,OP(DEMUL),KL(fbits(1.5f)),KL(fbits(-4.0f)),DL(4), ON_,OP(DEDIV),KL(fbits(9.0f)),KL(fbits(4.0f)),DL(6));
  good_scan();
  assert(fval(d32(0))==3.75f && fval(d32(2))==-0.75f && fval(d32(4))==-6.0f && fval(d32(6))==2.25f);
  PRG(ON_,OP(DEDIV),KL(fbits(1.0f)),KL(fbits(0.0f)),DL(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(DEMUL),KL(fbits(3.0e38f)),KL(fbits(10.0f)),DL(0)); FAULT(PLC_RUNTIME_OPERAND);   /* overflow */
  PRG(ON_,OP(DEMOV),KL(fbits(-0.5f)),DL(0), ON_,OP(DENEG),DL(2), ON_,OP(FLT),K(-7),DL(4),
      ON_,OP(DFLT),KL(-100000),DL(6));
  set32(2,fbits(8.0f)); good_scan();
  assert(fval(d32(0))==-0.5f && fval(d32(2))==-8.0f && fval(d32(4))==-7.0f && fval(d32(6))==-100000.0f);
  PRG(ON_,OP(INT),KL(fbits(-3.9f)),D(0), ON_,OP(INT),KL(fbits(3.9f)),D(1), ON_,OP(DINT),KL(fbits(123456.7f)),DL(2),
      ON_,OP(INT),KL(fbits(0.4f)),D(4));
  good_scan(); assert(dreg(0)==0xFFFD && dreg(1)==3 && d32(2)==123456UL && dreg(4)==0 && BIT(M8020));
  PRG(ON_,OP(INT),KL(fbits(40000.0f)),D(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(DINT),KL(fbits(3.0e9f)),DL(0)); FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(DECMP),KL(fbits(1.5f)),KL(fbits(2.5f)),BT(MB(10)), ON_,OP(DECMP),KL(fbits(2.5f)),KL(fbits(2.5f)),BT(MB(20)),
      ON_,OP(DEZCP),KL(fbits(1.0f)),KL(fbits(2.0f)),KL(fbits(1.5f)),BT(MB(30)), ON_,OP(DEZCP),KL(fbits(1.0f)),KL(fbits(2.0f)),KL(fbits(2.5f)),BT(MB(40)));
  good_scan(); assert(!BIT(10)&&!BIT(11)&&BIT(12) && !BIT(20)&&BIT(21)&&!BIT(22) && !BIT(30)&&BIT(31)&&!BIT(32) && !BIT(40)&&!BIT(41)&&BIT(42));
  struct { void (*fn)(void); float in, out; } fn[]={
    {DESQR,2.0f,1.41421356f},{DSIN,0.0f,0.0f},{DSIN,1.57079633f,1.0f},{DCOS,0.0f,1.0f},{DCOS,3.14159265f,-1.0f},
    {DTAN,0.78539816f,1.0f},{DASIN,0.5f,0.52359878f},{DACOS,0.5f,1.04719755f},{DATAN,1.0f,0.78539816f},
    {DRAD,180.0f,3.14159265f},{DDEG,3.14159265f,180.0f},{DEXP,0.0f,1.0f},{DEXP,1.0f,2.71828183f},
    {DLOGE,2.71828183f,1.0f},{DLOG10,1000.0f,3.0f},{DLOG10,0.001f,-3.0f} };
  for (unsigned i=0;i<COUNT(fn);++i) {
    PRG(ON_,OP(fn[i].fn),KL(fbits(fn[i].in)),DL(0)); good_scan();
    assert(fabsf(fval(d32(0))-fn[i].out) < 2e-6f*fmaxf(1.0f,fabsf(fn[i].out)) || !"float function"); }
  struct { void (*fn)(void); float in; } bad[]={{DESQR,-1.0f},{DASIN,1.5f},{DACOS,-1.5f},{DLOGE,0.0f},{DLOG10,-2.0f},{DEXP,100.0f}};
  for (unsigned i=0;i<COUNT(bad);++i) { PRG(ON_,OP(bad[i].fn),KL(fbits(bad[i].in)),DL(0)); FAULT(PLC_RUNTIME_OPERAND); }
}

/**
 * Time: TCMP, TZCP, TADD / TSUB with carry and borrow, HTOS, STOH, TRD, TWR (including the result
 * codes of the clock) and the HOUR meter.
 */
static void t_time(void) {
  /* TCMP: reference 12:30:00 against D0..D2 */
  PRG(ON_,OP(TCMP),K(12),K(30),K(0),D(0),BT(MB(10)));
  dreg(0)=12; dreg(1)=29; dreg(2)=59; good_scan(); assert(BIT(10) && !BIT(11) && !BIT(12));
  PRG(ON_,OP(TCMP),K(12),K(30),K(0),D(0),BT(MB(10))); dreg(0)=12; dreg(1)=30; dreg(2)=0; good_scan(); assert(!BIT(10) && BIT(11) && !BIT(12));
  PRG(ON_,OP(TCMP),K(12),K(30),K(0),D(0),BT(MB(10))); dreg(0)=13; dreg(1)=0; dreg(2)=0; good_scan(); assert(!BIT(10) && !BIT(11) && BIT(12));
  PRG(ON_,OP(TCMP),K(12),K(30),K(0),D(0),BT(MB(10))); dreg(0)=11; dreg(1)=59; dreg(2)=59; good_scan(); assert(BIT(10));   /* h/m/s are not compared independently */
  PRG(ON_,OP(TZCP),D(0),D(3),D(6),BT(MB(20)));
  dreg(0)=8; dreg(3)=17; dreg(6)=12; good_scan(); assert(!BIT(20) && BIT(21) && !BIT(22));
  PRG(ON_,OP(TZCP),D(0),D(3),D(6),BT(MB(20))); dreg(0)=8; dreg(3)=17; dreg(6)=7; dreg(7)=59; dreg(8)=59; good_scan(); assert(BIT(20) && !BIT(21) && !BIT(22));
  PRG(ON_,OP(TZCP),D(0),D(3),D(6),BT(MB(20))); dreg(0)=8; dreg(3)=17; dreg(6)=17; dreg(7)=0; dreg(8)=1; good_scan(); assert(!BIT(20) && !BIT(21) && BIT(22));
  /* TADD with minute and 24 h carry; TSUB with borrow */
  PRG(ON_,OP(TADD),D(0),D(3),D(6)); dreg(0)=10; dreg(1)=45; dreg(2)=50; dreg(3)=1; dreg(4)=20; dreg(5)=15; good_scan();
  assert(dreg(6)==12 && dreg(7)==6 && dreg(8)==5 && !BIT(M8022));
  PRG(ON_,OP(TADD),D(0),D(3),D(6)); dreg(0)=23; dreg(1)=59; dreg(2)=59; dreg(3)=0; dreg(4)=0; dreg(5)=2; good_scan();
  assert(dreg(6)==0 && dreg(7)==0 && dreg(8)==1 && BIT(M8022));
  PRG(ON_,OP(TSUB),D(0),D(3),D(6)); dreg(0)=1; dreg(1)=0; dreg(2)=0; dreg(3)=2; dreg(4)=30; dreg(5)=0; good_scan();
  assert(dreg(6)==22 && dreg(7)==30 && dreg(8)==0 && BIT(M8021));
  PRG(ON_,OP(TSUB),D(0),D(3),D(6)); dreg(0)=5; dreg(1)=5; dreg(2)=5; dreg(3)=5; dreg(4)=5; dreg(5)=5; good_scan(); assert(BIT(M8020) && dreg(6)==0);
  PRG(ON_,OP(TADD),D(0),D(3),D(6)); dreg(0)=24; FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(HTOS),D(0),D(10), ON_,OP(STOH),D(11),D(20)); dreg(0)=1; dreg(1)=2; dreg(2)=3; dreg(11)=7384; good_scan();
  assert(dreg(10)==3723 && dreg(20)==2 && dreg(21)==3 && dreg(22)==4);
  PRG(ON_,OP(HTOS),D(0),D(10)); dreg(0)=10; FAULT(PLC_RUNTIME_OPERAND);              /* 36000 s does not fit 16 bit */
  PRG(ON_,OP(TRD),D(0)); D8018=2026; D8017=10; D8016=11; D8015=12; D8014=34; D8013=56; D8019=6; good_scan();
  assert(dreg(0)==2026 && dreg(1)==10 && dreg(2)==11 && dreg(3)==12 && dreg(4)==34 && dreg(5)==56 && dreg(6)==6);
  PRG(ON_,OP(TWR),D(0)); rtc_calls=0; dreg(0)=26; dreg(1)=2; dreg(2)=28; dreg(3)=23; dreg(4)=59; dreg(5)=58; good_scan();
  assert(rtc_calls==1 && rtc_arguments[0]==2026 && rtc_arguments[1]==2 && rtc_arguments[5]==58);
  PRG(ON_,OP(TWR),D(0)); rtc_calls=0; dreg(0)=2026; dreg(1)=13; dreg(2)=1; FAULT(PLC_RUNTIME_OPERAND); assert(rtc_calls==0);
  /* the RTC result is not ignored: 1 = date outside the RTC range, 2 = RTC unavailable */
  PRG(ON_,OP(TWR),D(0)); dreg(0)=2026; dreg(1)=2; dreg(2)=28; rtc_set_result=1; FAULT(PLC_RUNTIME_OPERAND);
  PRG(ON_,OP(TWR),D(0)); dreg(0)=2026; dreg(1)=2; dreg(2)=28; rtc_set_result=2; FAULT(PLC_RUNTIME_LEGACY_UNSUPPORTED);
  rtc_set_result=0;
  /* HOUR: 3600 s per hour from scan-to-scan elapsed seconds */
  PRG(ON_,OP(HOUR),K(1),D(0),BT(MB(30)));
  dreg(0)=0; dreg(1)=3599; plc_exp_milliseconds=0; good_scan(); assert(!BIT(30)); plc_exp_milliseconds=1000; good_scan();
  assert(dreg(0)==1 && dreg(1)==0 && BIT(30));
}

/**
 * Program flow and logic: CJ, CALL / SRET (nesting, accumulator preservation, recursion limit),
 * FOR / NEXT, WDT, MC / MCR, STL step transitions, comparison contacts, the logic-block
 * instructions and the edge contacts.
 */
static void t_flow(void) {
  /* CJ skips instructions; a false CJ does not */
  PRG(ON_,OP(CJ),PT(1), ON_,OP(MOV),K(1),D(0), 0xB001, ON_,OP(MOV),K(2),D(1), OFF_,OP(CJ),PT(2), ON_,OP(MOV),K(3),D(2), 0xB002);
  good_scan(); assert(dreg(0)==0 && dreg(1)==2 && dreg(2)==3);
  PRG(ON_,OP(CJ),PT(9)); FAULT(PLC_RUNTIME_OPERAND);                   /* label P9 does not exist */
  { const u16 w_[]={ON_,OP(CJ),I2(0x88,0x80,3),0xB001,0x000F}; load(w_,COUNT(w_)); FAULT(PLC_RUNTIME_OPERAND); }   /* odd encoded pointer */
  /* nested CALL / SRET and return to the right place, FEND ends the main program */
  PRG(ON_,OP(CALL),PT(0), ON_,OP(INC),D(5), W_FEND,
      0xB000, ON_,OP(INC),D(0), ON_,OP(CALL),PT(1), ON_,OP(INC),D(1), W_SRET,
      0xB001, ON_,OP(INC),D(2), OFF_,OP(CALL),PT(2), ON_,OP(INC),D(3), W_SRET,
      0xB002, ON_,OP(INC),D(9), W_SRET);
  good_scan(); assert(dreg(0)==1 && dreg(1)==1 && dreg(2)==1 && dreg(3)==1 && dreg(5)==1 && dreg(9)==0);
  /* the accumulator is saved by CALL and restored by SRET, so the rung continues after the call */
  PRG(ON_,OP(CALL),PT(0),W_OUT(YB(0)),W_FEND, 0xB000,OFF_,OP(INC),D(0),W_SRET);
  good_scan(); assert(BIT(YB(0)) && dreg(0)==0);
  PRG(OFF_,OP(CALL),PT(0),W_OUT(YB(0)),W_FEND, 0xB000,ON_,OP(INC),D(0),W_SRET);
  PLC_BIT_ON(YB(0)); good_scan(); assert(!BIT(YB(0)) && dreg(0)==0);                  /* a false CALL does not call */
  PRG(ON_,W_SRET); FAULT(PLC_RUNTIME_STACK);                            /* SRET without CALL */
  PRG(0xB000,ON_,OP(CALL),PT(0),W_FEND); FAULT(PLC_RUNTIME_STACK);       /* unbounded recursion */
  /* FOR / NEXT, nested */
  PRG(OP(FOR),K(5), ON_,OP(INC),D(0), OP(FOR),K(3), ON_,OP(INC),D(1), OP(NEXT), OP(NEXT));
  good_scan(); assert(dreg(0)==5 && dreg(1)==15);
  PRG(OP(FOR),K(0), ON_,OP(INC),D(0), OP(NEXT)); good_scan(); assert(dreg(0)==1);            /* n < 1 runs once */
  PRG(OP(NEXT)); FAULT(PLC_RUNTIME_STACK);
  PRG(OP(FOR),K(2)); FAULT(PLC_RUNTIME_STACK);                           /* END with an open FOR */
  PRG(OP(FOR),K(30000),OP(FOR),K(30000),ON_,OP(INC),D(0),OP(NEXT),OP(NEXT)); FAULT(PLC_RUNTIME_LEGACY_BUDGET);
  PRG(ON_,OP(WDT)); good_scan();
  /* MC / MCR: coils inside an OFF master are driven OFF, SET/RST and instructions do nothing */
  PRG(0x01C2,0x8000|MB(5),OP(MC),0x8000|MB(10), ON_,0xCC00, ON_,OP(BSET),0x8000|MB(11), ON_,OP(INC),D(0), ON_,OP(MCR),0x8000, ON_,0xCC01);
  PLC_BIT_ON(YB(0)); PLC_BIT_ON(MB(11)); good_scan();
  assert(!BIT(10) && !BIT(YB(0)) && BIT(MB(11)) && dreg(0)==0 && BIT(YB(1)));
  PRG(0x01C2,0x8000|MB(5),OP(MC),0x8000|MB(10), ON_,0xCC00, ON_,OP(INC),D(0), ON_,OP(MCR),0x8000, ON_,0xCC01);
  PLC_BIT_ON(MB(5)); good_scan(); assert(BIT(10) && BIT(YB(0)) && dreg(0)==1 && BIT(YB(1)));
  PRG(ON_,OP(MCR),0x8000); FAULT(PLC_RUNTIME_STACK);
  PRG(ON_,OP(MC),0x8000|MB(10)); FAULT(PLC_RUNTIME_STACK);               /* END with an open MC */
  /* a parallel contact (OR, ORI, ORP, comparison OR) must not switch a rung ON behind an OFF master */
  { const u16 p[]={0x01C2,0x8000|MB(5),OP(MC),0x8000|MB(10),
                   W_LD(0),W_OR(20),W_OUT(YB(0)),  W_LD(0),W_ORI(21),W_OUT(YB(1)),
                   OFF_,OP(OR_EQU),K(1),K(1),W_OUT(YB(2)),  OFF_,OP(ORP),0x8000|MB(22),W_OUT(YB(3)),
                   ON_,OP(MCR),0x8000,
                   W_LD(0),W_OR(20),W_OUT(YB(4)),W_END};
    load(p,COUNT(p)); PLC_BIT_ON(20); PLC_BIT_ON(22); good_scan();
    assert(!BIT(YB(0)) && !BIT(YB(1)) && !BIT(YB(2)) && !BIT(YB(3)) && BIT(YB(4))); }
  /* the same inside a step that is not active */
  { const u16 p[]={W_STL(20), W_LD(0),W_OR(20),W_OUT(YB(0)), W_RET, W_LD(0),W_OR(20),W_OUT(YB(1)), W_END};
    load(p,COUNT(p)); PLC_BIT_ON(20); good_scan(); assert(!BIT(YB(0)) && BIT(YB(1))); }
  /* STL: S20 -> S21, the previous step's coils drop on the next scan */
  const u16 stl[]={0x01C2,0x8E02,OP(BSET),0x8000|SB(20),
    W_STL(20), ON_,W_OUT(YB(0)), 0x01C2,0x8000|MB(5),OP(BSET),0x8000|SB(21),
    W_STL(21), ON_,W_OUT(YB(1)), 0x01C2,0x8000|MB(6),OP(OUT),0x8000|SB(22),
    W_STL(22), ON_,W_OUT(YB(2)), W_RET, W_END};
  load(stl,COUNT(stl)); good_scan();
  assert(BIT(SB(20)) && BIT(YB(0)) && !BIT(YB(1)));
  PLC_BIT_ON(MB(5)); good_scan();
  assert(!BIT(SB(20)) && BIT(SB(21)) && BIT(YB(0)) && BIT(YB(1)));   /* Y0 was driven before the transition */
  good_scan(); assert(!BIT(YB(0)) && BIT(YB(1)) && !BIT(YB(2)));
  PLC_BIT_ON(MB(6)); good_scan(); assert(!BIT(SB(21)) && BIT(SB(22)) && BIT(YB(2)));
  good_scan(); assert(!BIT(YB(1)) && BIT(YB(2)));
  /* contacts: LD=, AND>, OR<, ANB/ORB/MPS/MRD/MPP/INV, LDP/LDF */
  PRG(OP(LD_EQU),D(0),K(5),0xCC00, OP(LD_GTR),D(0),K(9),0xCC01, ON_,OP(AND_LSS),D(0),K(9),0xCC02,
      OFF_,OP(OR_NEQ),D(0),K(5),0xCC03, OP(DLD_GEQ),DL(2),KL(100000),0xCC04, OP(DLD_LEQ),DL(2),KL(100000),0xCC05);
  dreg(0)=5; set32(2,100000); good_scan();
  assert(BIT(YB(0)) && !BIT(YB(1)) && BIT(YB(2)) && !BIT(YB(3)) && BIT(YB(4)) && BIT(YB(5)));
  PRG(ON_,OP(LD_EQU),D(0),K(5),W_ANB,W_OUT(YB(0)));                          /* LD M8000; LD D0=5; ANB; OUT Y0 */
  dreg(0)=5; good_scan(); assert(BIT(YB(0)));
  PRG(ON_,OP(LD_EQU),D(0),K(5),W_ANB,W_OUT(YB(0))); dreg(0)=6; good_scan(); assert(!BIT(YB(0)));
  PRG(0x01C2,0x8000|MB(1),W_MPS,0xCC00,W_MRD,0xCC01,W_MPP,0xCC02, W_INV,0xCC03);   /* LD M1 MPS OUT MRD OUT MPP OUT INV */
  PLC_BIT_ON(1); good_scan(); assert(BIT(YB(0)) && BIT(YB(1)) && BIT(YB(2)) && !BIT(YB(3)));
  const u16 edge[]={0x01CA,0x8000|MB(1),0xCC00, 0x01CB,0x8000|MB(1),0xCC01, 0x000F};
  load(edge,COUNT(edge)); good_scan(); assert(!BIT(YB(0)));
  PLC_BIT_ON(1); good_scan(); assert(BIT(YB(0)) && !BIT(YB(1))); good_scan(); assert(!BIT(YB(0)));
  PLC_BIT_OFF(1); good_scan(); assert(BIT(YB(1)) && !BIT(YB(0))); good_scan(); assert(!BIT(YB(1)));
}

/** Reference comparison for the contact tests: r is 0 '=', 1 '>', 2 '<', 3 '<>', 4 '<=', 5 '>='. */
static int rel(unsigned r,long long a,long long b) {
  switch (r) { case 0: return a==b; case 1: return a>b; case 2: return a<b; case 3: return a!=b; case 4: return a<=b; default: return a>=b; }
}
/**
 * Contact algebra, exhaustively: the basic contacts, the edge contacts, SET / RST, the logic-block
 * truth tables and all 36 comparison contacts for a set of 16 and 32-bit values, plus the gating by
 * master control.
 */
static void t_contacts(void) {
  for (unsigned m=0;m<4U;++m) {                       /* M1, M2 inputs */
    const u16 w_[]={W_LD(1),W_AND(2),W_OUT(YB(0)), W_LDI(1),W_ANI(2),W_OUT(YB(1)),
                    W_LD(1),W_OR(2),W_OUT(YB(2)),  W_LDI(1),W_ORI(2),W_OUT(YB(3)), W_END};
    load(w_,COUNT(w_)); plc_exp_put_bit(1,m&1U); plc_exp_put_bit(2,(m>>1)&1U); good_scan();
    unsigned a=m&1U, b=(m>>1)&1U;
    assert(BIT(YB(0))==(a&&b) && BIT(YB(1))==(!a&&!b) && BIT(YB(2))==(a||b) && BIT(YB(3))==(!a||!b));
  }
  { const u16 w_[]={OP(LDP),0x8000|1,W_OUT(YB(0)), OP(LDF),0x8000|1,W_OUT(YB(1)),
                    ON_,OP(ANDP),0x8000|1,W_OUT(YB(2)), ON_,OP(ANDF),0x8000|1,W_OUT(YB(3)),
                    OFF_,OP(ORP),0x8000|1,W_OUT(YB(4)), OFF_,OP(ORF),0x8000|1,W_OUT(YB(5)), W_END};
    load(w_,COUNT(w_)); good_scan();
    assert(!BIT(YB(0))&&!BIT(YB(1))&&!BIT(YB(2))&&!BIT(YB(3))&&!BIT(YB(4))&&!BIT(YB(5)));
    PLC_BIT_ON(1); good_scan();                                  /* rising edge */
    assert(BIT(YB(0))&&!BIT(YB(1))&&BIT(YB(2))&&!BIT(YB(3))&&BIT(YB(4))&&!BIT(YB(5)));
    good_scan();
    assert(!BIT(YB(0))&&!BIT(YB(2))&&!BIT(YB(4)));
    PLC_BIT_OFF(1); good_scan();                                 /* falling edge */
    assert(!BIT(YB(0))&&BIT(YB(1))&&!BIT(YB(2))&&BIT(YB(3))&&!BIT(YB(4))&&BIT(YB(5)));
    good_scan(); assert(!BIT(YB(1))&&!BIT(YB(3))&&!BIT(YB(5))); }
  /* SET / RST (one-word and extended forms) only act while the rung is true */
  { const u16 p[]={ON_,W_SET(MB(7)), ON_,W_RST(MB(8)), OFF_,W_SET(MB(9)), OFF_,W_RST(MB(10)),
                   ON_,OP(BSET),0x8000|YB(3), ON_,OP(RST),0x8000|MB(11), W_END};
    load(p,COUNT(p)); PLC_BIT_ON(8); PLC_BIT_ON(10); PLC_BIT_ON(11); good_scan();
    assert(BIT(7)&&!BIT(8)&&!BIT(9)&&BIT(10)&&BIT(YB(3))&&!BIT(11)); }
  /* ANB / ORB / MPS / MRD / MPP / INV truth tables */
  for (unsigned m=0;m<4U;++m) {
    const u16 p[]={W_LD(1),W_LD(2),W_ANB,W_OUT(YB(0)), W_LD(1),W_LD(2),W_ORB,W_OUT(YB(1)),
                   W_LD(1),W_MPS,W_AND(2),W_OUT(YB(2)),W_MRD,W_ANI(2),W_OUT(YB(3)),W_MPP,W_INV,W_OUT(YB(4)), W_END};
    unsigned a=m&1U, b=(m>>1)&1U;
    load(p,COUNT(p)); plc_exp_put_bit(1,a); plc_exp_put_bit(2,b); good_scan();
    assert(BIT(YB(0))==(a&&b) && BIT(YB(1))==(a||b) && BIT(YB(2))==(a&&b) && BIT(YB(3))==(a&&!b) && BIT(YB(4))==!a);
  }
  static void (* const ld_f[2][6])(void)={{LD_EQU,LD_GTR,LD_LSS,LD_NEQ,LD_LEQ,LD_GEQ},{DLD_EQU,DLD_GTR,DLD_LSS,DLD_NEQ,DLD_LEQ,DLD_GEQ}};
  static void (* const and_f[2][6])(void)={{AND_EQU,AND_GTR,AND_LSS,AND_NEQ,AND_LEQ,AND_GEQ},{DAND_EQU,DAND_GTR,DAND_LSS,DAND_NEQ,DAND_LEQ,DAND_GEQ}};
  static void (* const or_f[2][6])(void)={{OR_EQU,OR_GTR,OR_LSS,OR_NEQ,OR_LEQ,OR_GEQ},{DOR_EQU,DOR_GTR,DOR_LSS,DOR_NEQ,DOR_LEQ,DOR_GEQ}};
  static const long long v16[]={-300,-1,0,1,5,300}, v32[]={-70000,-1,0,1,5,70000};
  for (unsigned w=0;w<2U;++w) for (unsigned r=0;r<6U;++r) for (unsigned i=0;i<6U;++i) for (unsigned j=0;j<6U;++j) {
    long long a=w?v32[i]:v16[i], b=w?v32[j]:v16[j]; int want=rel(r,a,b);
    if (w) {
      { const u16 p[]={OP(ld_f[1][r]),KL((long)a),KL((long)b),W_OUT(YB(0)),W_END}; load(p,COUNT(p)); good_scan(); assert(BIT(YB(0))==want); }
      { const u16 p[]={ON_,OP(and_f[1][r]),KL((long)a),KL((long)b),W_OUT(YB(0)),W_END}; load(p,COUNT(p)); good_scan(); assert(BIT(YB(0))==want); }
      { const u16 p[]={OFF_,OP(or_f[1][r]),KL((long)a),KL((long)b),W_OUT(YB(0)),W_END}; load(p,COUNT(p)); good_scan(); assert(BIT(YB(0))==want); }
    } else {
      { const u16 p[]={OP(ld_f[0][r]),K((int)a),K((int)b),W_OUT(YB(0)),W_END}; load(p,COUNT(p)); good_scan(); assert(BIT(YB(0))==want); }
      { const u16 p[]={ON_,OP(and_f[0][r]),K((int)a),K((int)b),W_OUT(YB(0)),W_END}; load(p,COUNT(p)); good_scan(); assert(BIT(YB(0))==want); }
      { const u16 p[]={OFF_,OP(or_f[0][r]),K((int)a),K((int)b),W_OUT(YB(0)),W_END}; load(p,COUNT(p)); good_scan(); assert(BIT(YB(0))==want); }
    }
  }
  /* A false accumulator keeps AND false and a true one keeps OR true, whatever the comparison says. */
  { const u16 p[]={OFF_,OP(AND_EQU),K(1),K(1),W_OUT(YB(0)), ON_,OP(OR_NEQ),K(1),K(1),W_OUT(YB(1)),W_END};
    load(p,COUNT(p)); good_scan(); assert(!BIT(YB(0)) && BIT(YB(1))); }
  /* a comparison contact inside a master-control region loads FALSE while the master is off */
  { const u16 p[]={OFF_,OP(MC),0x8000|MB(10), OP(LD_EQU),K(1),K(1),W_OUT(YB(0)), ON_,OP(MCR),0x8000,W_END};
    load(p,COUNT(p)); good_scan(); assert(!BIT(YB(0))); }
  /* DABSD: 32-bit drum with a 32-bit counter */
  PRG(ON_,OP(DABSD),DL(0),CCL(200),BT(MB(60)),K(1));
  set32(0,100000); set32(2,200000); PLC_32BIT(Addr_CHVal)=150000U; good_scan(); assert(BIT(60));
  PLC_32BIT(Addr_CHVal)=250000U; good_scan(); assert(!BIT(60));
}

/* Linear bit address of the special relay M80nn. */
#define SP(n) (Addr_M8*8U+(n))
/**
 * Special relays. A program may drive only M8030..M8059 and M8100..M8255 (instruction mode flags such
 * as M8049 or M8165); writing any other special relay (status, clock, flag and error relays), or a bit
 * group that is not entirely inside a writable range, stops the PLC with PLC_RUNTIME_OPERAND.
 * Reading is always allowed.
 */
static void t_special_relays(void) {
  PRG(ON_,OP(BSET),0x8000|SP(165), ON_,OP(SORT2),D(0),K(3),K(1),D(20),K(1));       /* descending sort */
  dreg(0)=5; dreg(1)=9; dreg(2)=1; good_scan(); assert(BIT(SP(165)) && dreg(20)==9 && dreg(21)==5 && dreg(22)==1);
  PRG(ON_,OP(BSET),0x8000|SP(161), ON_,OP(CCD),D(0),D(10),K(3));                   /* 8-bit mode */
  dreg(0)=0x11; dreg(1)=0x2233; dreg(2)=0x44; good_scan(); assert(dreg(10)==0x88 && dreg(11)==(0x11^0x33^0x44));
  PRG(ON_,OP(BSET),0x8000|SP(168), ON_,OP(SMOV),D(0),K(4),K(2),D(1),K(3));         /* BCD mode: data stays BCD */
  dreg(0)=0x1234; dreg(1)=0x5678; good_scan(); assert(dreg(1)==0x5128);
  PRG(ON_,OP(OUT),0x8000|SP(49), ON_,OP(OUT),0x8000|SP(100), ON_,OP(OUT),0x8000|SP(59), ON_,OP(MOV),H(0x00F0),GRP(4,SP(32)));
  good_scan(); assert(BIT(SP(49)) && BIT(SP(100)) && BIT(SP(59)) && PLC_8BIT(Addr_M8+4)==0xF0);
  PRG(ON_,OP(OUT),0x8000|SP(30), ON_,OP(OUT),0x8000|SP(255)); good_scan();
  const unsigned read_only[]={0,1,2,3,11,13,20,21,22,29,60,63,65,67,99};
  for (unsigned i=0;i<COUNT(read_only);++i) {
    PRG(ON_,OP(OUT),0x8000|SP(read_only[i])); FAULT(PLC_RUNTIME_OPERAND);
    PRG(ON_,OP(BSET),0x8000|SP(read_only[i])); FAULT(PLC_RUNTIME_OPERAND);
  }
  PRG(ON_,OP(MOV),K(1),GRP(4,SP(24))); FAULT(PLC_RUNTIME_OPERAND);                /* spans M8024..M8039 */
  PRG(ON_,OP(MOV),K(1),GRP(4,SP(56))); FAULT(PLC_RUNTIME_OPERAND);                /* spans M8056..M8071 */
  PRG(ON_,OP(MOV),GRP(4,SP(0)),D(0)); good_scan();                                 /* reading status relays is fine */
}

/**
 * OUT Tn and OUT Cn: a 100 ms timer with saturation, reset by a dropped rung, a D preset given as an
 * even byte offset, 10 ms and retentive timers, 16-bit counters (edges, RST, held input), the 32-bit
 * up/down counter C200, rejected high-speed counters, and RST on T, C and D.
 */
static void t_timers_counters(void) {
  PRG(ON_,0x0600|0,K(5), 0x01C2,0x8000|Addr_TO*8U,0xCC00);
  plc_exp_milliseconds=0; good_scan(); assert(PLC_16BIT(Addr_TMax)==5 && !BIT(YB(0)));
  plc_exp_milliseconds=499; good_scan(); assert(PLC_16BIT(Addr_TVal)==4 && !BIT(YB(0)));
  plc_exp_milliseconds=500; good_scan(); assert(PLC_16BIT(Addr_TVal)==5 && BIT(YB(0)));
  plc_exp_milliseconds=900; good_scan(); assert(PLC_16BIT(Addr_TVal)==5);
  PRG(0x01C2,0x9200,0x0600|1,K(3));                                      /* T1 K3 driven by X0 */
  PLC_BIT_ON(XB(0)); plc_exp_milliseconds=0; good_scan(); plc_exp_milliseconds=200; good_scan();
  PLC_BIT_OFF(XB(0)); plc_exp_milliseconds=210; good_scan(); assert(PLC_16BIT(Addr_TVal+2U)==0);   /* released: reset */
  PRG(ON_,0x0600|2,I2(0x86,0x86,1)); FAULT(PLC_RUNTIME_OPERAND);        /* D preset must be an even byte offset */
  PRG(ON_,0x0600|2,I2(0x86,0x86,6)); dreg(3)=7; plc_exp_milliseconds=0; good_scan(); assert(PLC_16BIT(Addr_TMax+4U)==7);
  PRG(ON_,0x0600|2,K(40000)); FAULT(PLC_RUNTIME_OPERAND);
  /* 10 ms timer T200 with K15 and a retentive T250 */
  PRG(ON_,0x0600|200,K(15), ON_,0x0600|250,K(2)); plc_exp_milliseconds=0; good_scan(); plc_exp_milliseconds=155; good_scan();
  assert(PLC_16BIT(Addr_TVal+400U)==15 && PLC_16BIT(Addr_TVal+500U)==1);
  /* counters: counted on rising edges only; RST C resets */
  const u16 cnt[]={0x01C2,0x8000|MB(1),0x0E00|0,K(3), 0x01C2,0x8000|MB(2),OP(RST_TC),0x8E00|0,
    0x01C2,0x8000|Addr_CO*8U,0xCC00, 0x000F};
  load(cnt,COUNT(cnt)); good_scan();
  for (unsigned i=0;i<4;++i) { PLC_BIT_ON(1); good_scan(); assert(PLC_16BIT(Addr_CVal)==(i<3?i+1U:3U)); PLC_BIT_OFF(1); good_scan(); }
  assert(BIT(YB(0))); good_scan(); assert(PLC_16BIT(Addr_CVal)==3);
  { const u16 held[]={0x01C2,0x8000|MB(1),0x0E00|0,K(5),W_END};          /* a held input counts once */
    load(held,COUNT(held)); PLC_BIT_ON(1); good_scan(); good_scan(); good_scan();
    assert(PLC_16BIT(Addr_CVal)==1); PLC_BIT_OFF(1); good_scan(); PLC_BIT_ON(1); good_scan(); good_scan();
    assert(PLC_16BIT(Addr_CVal)==2);
    load(cnt,COUNT(cnt)); good_scan(); }
  PLC_BIT_ON(2); good_scan(); assert(PLC_16BIT(Addr_CVal)==0 && !BIT(YB(0)));
  /* C200: 32-bit up/down counter, direction relay M8200 (ON = down). The preset 70000 is a K32 constant,
   * which takes two items (low word, then high word). */
  { const u16 w_[]={0x01C2,0x8000|MB(1),0x0E00|200,K(70000&0xFFFF),K(70000>>16),0x01C2,0x8000|(Addr_CO*8U+200U),0xCC00,0x000F};
    load(w_,COUNT(w_)); PLC_32BIT(Addr_CHVal)=69998U; good_scan();
    PLC_BIT_ON(1); good_scan(); assert(PLC_32BIT(Addr_CHVal)==69999U && !BIT(YB(0))); PLC_BIT_OFF(1); good_scan();
    PLC_BIT_ON(1); good_scan(); assert(PLC_32BIT(Addr_CHVal)==70000U && BIT(YB(0))); PLC_BIT_OFF(1); good_scan();
    PLC_BIT_ON(M8200); PLC_BIT_ON(1); good_scan(); assert(PLC_32BIT(Addr_CHVal)==69999U && !BIT(YB(0))); }
  PRG(ON_,0x0E00|240,K(3)); FAULT(PLC_RUNTIME_LEGACY_UNSUPPORTED);        /* high-speed counters need hardware */
  PRG(ON_,OP(RST_TC),0x8E00|0); PLC_16BIT(Addr_CVal)=9; good_scan(); assert(PLC_16BIT(Addr_CVal)==0);
  PRG(ON_,OP(RST_D),D(3), ON_,OP(RST_D),TT(4), ON_,OP(RST_D),CC(5)); dreg(3)=9; PLC_16BIT(Addr_TVal+8U)=3; PLC_16BIT(Addr_CVal+10U)=4;
  good_scan(); assert(dreg(3)==0 && PLC_16BIT(Addr_TVal+8U)==0 && PLC_16BIT(Addr_CVal+10U)==0);
}

/* Fuzzing
 *
 * Every table slot is driven with random but frequently valid operands. The run must never trip a
 * sanitizer, loop, or write outside the PLC image; it either completes or latches a fault.
 */
/** State of rnd(). The seed is fixed, so a failing run can be reproduced. */
static u32 rng_state=0x13579BDFUL;
/** Linear congruential generator (Numerical Recipes constants); returns the upper 24 bits of the state. */
static u32 rnd(void) { rng_state=rng_state*1664525UL+1013904223UL; return rng_state>>8; }
/**
 * Store one operand item at @p w: word 0 is (t1 << 8) | low byte of @p n, word 1 is (t2 << 8) | bits
 * 8..15 of @p n. Returns the length of the item, 2 words.
 */
static unsigned put_item(u16 *w, unsigned t1, unsigned t2, u32 n) {
  w[0]=(u16)((t1<<8)|(n&0xFFU)); w[1]=(u16)((t2<<8)|((n>>8)&0xFFU)); return 2U;
}
/**
 * Write a random operand at @p w and return its length in words. The operand is a constant, device,
 * bit, group, pointer, indexed or junk item in the single-item (16-bit) or two-item (32-bit) form, so
 * that most generated programs get past the operand decoder and exercise the instruction body.
 */
static unsigned rand_operand(u16 *w) {
  static const u16 bitbase[]={0,0,0,0xC00,0xE00,0x1200,0x1400};
  unsigned kind=rnd()%12U, n=0, dbl=(rnd()&1U);
  u32 v=rnd()%12U;
  switch (kind) {
    case 0: case 1: n=put_item(w,0x80,0x80,v); if (dbl) n+=put_item(w+n,0x80,0x80,0); break;          /* K */
    case 2: n=put_item(w,0x82,0x80,rnd()&0xFFU); if (dbl) n+=put_item(w+n,0x82,0x80,rnd()&0xFFU); break; /* H */
    case 3: case 4: case 5: n=put_item(w,0x86,0x86,rnd()%24U); if (dbl) n+=put_item(w+n,0x86,0x86,rnd()%24U); break;
    case 6: n=put_item(w,0x84,0x80,bitbase[rnd()%COUNT(bitbase)]+rnd()%32U); break;                      /* bit */
    case 7: n=put_item(w,0x84,0x80+2U*(1U+rnd()%8U),bitbase[rnd()%COUNT(bitbase)]+8U*(rnd()%4U));
            if (dbl) { n+=put_item(w+n,0x84,0x80+2U*(1U+rnd()%8U),0); }
            break;                                                                                         /* KnM */
    case 8: n=put_item(w,0x86,0x80+2U*(rnd()%5U),rnd()%12U); if (dbl) n+=put_item(w+n,0x86,0x86,0); break; /* D8000/T/C/D/D1000 */
    case 9: n=put_item(w,0x88,0x80,2U*(rnd()%4U)); break;                                                /* pointer */
    case 10: n=put_item(w,(rnd()&1U)?0xA6:0x96,0x82U+(rnd()%8U),(rnd()%24U)|((rnd()%4U)<<14)); break;   /* indexed D */
    default: n=put_item(w,(unsigned)(0x80+2U*(rnd()%8U)),(unsigned)(0x80+(rnd()%12U)),rnd()&0xFFFU); break; /* junk */
  }
  return n;
}
/* For each opcode slot: the number of fuzz programs in which the instruction body was reached. */
static unsigned slot_ok[512];
/**
 * Fuzz every slot of the extended dispatch table: 400 programs per slot (LD M8000, the slot with the
 * P suffix on odd trials, six random operands, END), then 20000 programs of fully random words.
 * A program has to run or latch a fault. Every implemented slot, except the rejected instructions and
 * those in known_answer_only, must reach its instruction body at least once.
 */
static void t_fuzz(void) {
  unsigned completed=0, faulted=0, covered=0;
  for (unsigned op=0; op<0x200U; ++op) {
    for (unsigned trial=0; trial<400U; ++trial) {
      u16 w[48]; unsigned n=0, k;
      w[n++]=0x01C2; w[n++]=0x8E00;
      w[n++]=(u16)(op | ((trial&1U) ? 0x1000U : 0U));
      for (k=0;k<6U;++k) n+=rand_operand(w+n);
      w[n++]=0x000F;
      load(w,n);
      for (k=0;k<24U;++k) dreg(k)=(u16)(rnd()%40U);
      PLC_Main_Parser();
      if (PLC_Runtime_LastError()) ++faulted; else ++completed;
      /* the instruction under test sits at step 2: it ran if no fault, or the fault came later */
      if (!PLC_Runtime_LastError() || PLC_Runtime_ErrorStep()!=2U) ++slot_ok[op];
      assert(PLC_Runtime_LastError()==0 || !PLC_BIT_TEST(M8000));
    }
  }
  for (unsigned trial=0; trial<20000U; ++trial) {                       /* fully random words */
    u16 w[48]; unsigned n=0;
    while (n<44U) { if ((rnd()&3U)==0U) { w[n++]=(u16)(rnd()&0x1FFU); } else n+=rand_operand(w+n); }
    w[n++]=0x000F; load(w,n); PLC_Main_Parser();
  }
  unsigned implemented=0;
  for (unsigned op=0; op<0x200U; ++op) {
    void (*f)(void)=FUNC_EXT_0[op];
    if (f==ERR || f==NOP || (op>=2U && op<=0x0DU) || op==0x0F || op==0x1C) continue;
    if (listed(f,rejected,COUNT(rejected)) || listed(f,known_answer_only,COUNT(known_answer_only))) continue;
    ++implemented; if (slot_ok[op]) ++covered;
  }
  printf("fuzz: %u scans completed, %u latched a fault; instruction body reached in %u of %u fuzzed slots\n",
         completed,faulted,covered,implemented);
  if (getenv("FUZZ_REPORT")) {
    for (unsigned op=0; op<0x200U; ++op) {
      void (*f)(void)=FUNC_EXT_0[op];
      if (f==ERR || f==NOP || (op>=2U && op<=0x0DU) || op==0x0F || op==0x1C) continue;
      if (listed(f,rejected,COUNT(rejected)) || listed(f,known_answer_only,COUNT(known_answer_only))) continue;
      if (!slot_ok[op]) printf("not reached: 0x%03X\n",op);
    }
  }
  fflush(stdout);
  assert(covered==implemented);                 /* deterministic seed: every other body is reached */
}

/** Map the host memory that stands in for the device and run all tests. */
int main(void) {
  map(0x20001000,0x1000); map(PLC_RAM_BASE,PLC_RAM_SIZE); map(ROM_ADDR,0x4000);
  t_index_registers(); t_smoke(); t_faults(); t_operands(); t_move_compare(); t_arith(); t_rotate_shift();
  t_data(); t_float(); t_time(); t_flow(); t_contacts(); t_special_relays(); t_timers_counters(); t_fuzz();
  puts("PASS: experimental instruction layer known-answer tests and per-slot fuzzing (ASan/UBSan).");
  return 0;
}
