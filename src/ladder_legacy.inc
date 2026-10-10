/* EXPERIMENTAL: restored executable PR2 legacy handlers, not validated FX support.
 * Provenance: 4023823a1b9c0edc083288664dda0b04f28a961c src/ladder.c.
 * See docs/experimental-instructions.md for port changes and known defects. */
/********************************************************/
// Original target required 64K RAM/128K Flash; this C8 port is experimental.
// This code has been tested successfully on STM32F103RDT6 and VET6
// Edited: 20150909
// editor by Xiao Xiao Sheng
// Online store: shop182385147.taobao.com
/********************************************************/

#include "platform.h"
#include <stdio.h>
#include "plc_runtime.h"
#include "plc_memory.h"
#include <string.h>
#include "PLC_Dialogue.h"
#include "math.h"          //Mathematical function library 

#include "PLC_CONF.H"

#include "ladder_legacy_guard.inc"

//extern unsigned char Y0P,Y1P;                //
//extern unsigned short Plus_CMP0,Plus_CMP1;   //Pulse flag bits
extern u16 X_DIY;                             //Filter time
extern volatile u16 PLC_RUN_TIME;                     //Scan time
extern volatile u8 PLC_UPDATE_PROG;
extern void RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec);//Time-setting routine
extern void PLC_IO_Refresh(void);            //I/O output refresh routine

static u8 PLC_ACC_BIT; // @ (RAM_ACC_ADDR);
static u8 PLC_MPS_BIT; //Reserved for program execution (operation stack and branch stack)

static const u16 *PLC_PC;                  //PLC program pointer
static const u16 *PLC_Err;                   //PLC error step
static u32 mov_d_addr;                       //K-group M/Y/S/X instruction buffer
static const u16 *PLC_P_Addr[128];           //Pointers for subroutine calls
static const u16 *p_save[64];               //Save the previous execution position on a subroutine call
u16 process[64];                             //Save the previous subroutine value on a subroutine call

#define USE_COMPARE_MOVE
#define USE_TIME
#define USE_SPECIAL
#define USE_ARIPH
#define USE_DOUBLE
#define USE_MULDIV
#define USE_FLOAT
#define USE_ROTATE

const unsigned char PLC_BIT_OR[] ={0x01,0x02,0x04,0x08,0X10,0X20,0X40,0X80};
const unsigned char PLC_BIT_AND[]={0xFE,0XFD,0XFB,0XF7,0XEF,0XDF,0XBF,0X7F};

u32 CMD_ADDR, CMD_REG, CMD_PULS, CMD_PULS_TEST;
u16 CMD_CMD;

u8  PLC_PULS;
u8  PLC_PRG_STOP = 0;
volatile u8 PLC_NEW_PROG;                        //Reprogramming buffer register

u8  Flag_bit = 0xff, bitT_put, bitT_get; //K-value flag; reduces function overhead and CPU load     
u32 value;                               //Used for addition and subtraction; reduces function overhead and CPU load     
u32 Transfer = 0;                        //Used for multi-point and block transfers; reduces function overhead and CPU load  

float_union FLOAT;
u64_union   u64d; //,u64d2;

#ifdef __ICCARM__
#define WEAK
#else
#define WEAK
#endif
void WEAK ERR(void);
void WEAK NOP(void);

void WEAK FUNC_0(void); // 0x0xxx 0x1xxx
void WEAK LD(void);     // 0x2XXX
void WEAK LDI(void);    // 0x3XXX
void WEAK AND(void);    // 0x4XXX
void WEAK ANI(void);    // 0x5XXX
void WEAK OR(void);     // 0x6XXX
void WEAK ORI(void);    // 0x7XXX
void WEAK OUT(void);    // 0xCXXX
void WEAK BSET(void);   // 0xDXXX
void WEAK RST(void);    // 0xEXXX
void WEAK FUNC_F(void); // 0xFxxx

void WEAK STL(void);    // 0xFXXX
void WEAK RET(void);    // 0xF7XX
void WEAK FUNC_FF(void);// 0xFFxx

void WEAK PLS(void);
void WEAK PLF(void);
void WEAK MC(void);
void WEAK MCR(void);
void WEAK RST_TC(void);
void WEAK RST_D(void);
void WEAK END(void);

void WEAK CJ(void);
void WEAK CALL(void);
void WEAK SRET(void);
void WEAK IRET(void);
void WEAK EI(void);
void WEAK DI(void);
void WEAK WDT(void);
void WEAK FOR(void);
void WEAK NEXT(void);

void WEAK CMP(void);
void WEAK DCMP(void);
void WEAK ZCP(void);
void WEAK DZCP(void);
void WEAK MOV(void);
void WEAK DMOV(void);
void WEAK SMOV(void);
void WEAK CML(void);
void WEAK DCML(void);
void WEAK BMOV(void);
void WEAK FMOV(void);
void WEAK DFMOV(void);
void WEAK XCH(void);
void WEAK DXCH(void);
void WEAK BCD(void);
void WEAK DBCD(void);
void WEAK BIN(void);
void WEAK DBIN(void);

void WEAK ADD(void); 
void WEAK DADD(void);
void WEAK SUB(void); 
void WEAK DSUB(void);
void WEAK MUL(void); 
void WEAK DMUL(void);
void WEAK DIV(void); 
void WEAK DDIV(void);
void WEAK INC(void); 
void WEAK DINC(void);
void WEAK DEC(void); 
void WEAK DDEC(void);
void WEAK WAND(void);
void WEAK DWAND(void);
void WEAK WOR(void); 
void WEAK DWOR(void);
void WEAK WXOR(void);
void WEAK DWXOR(void);
void WEAK NEG(void); 
void WEAK DNEG(void);
void WEAK ROR(void); 
void WEAK DROR(void);
void WEAK ROL(void); 
void WEAK DROL(void);
void WEAK RCR(void); 
void WEAK DRCR(void);
void WEAK RCL(void); 
void WEAK DRCL(void);
void WEAK SFTR(void);
void WEAK SFTL(void);
void WEAK WSFR(void);
void WEAK WSFL(void);
void WEAK SFWR(void);
void WEAK SFWL(void);
void WEAK ZRST(void);
void WEAK DECO(void);
void WEAK ENCO(void);
void WEAK SUM(void); 
void WEAK DSUM(void);
void WEAK BON(void); 
void WEAK DBON(void);
void WEAK MEAN(void);
void WEAK DMEAN(void);
void WEAK ANS(void); 
void WEAK ANR(void); 
void WEAK SQR(void); 
void WEAK DSQR(void);
void WEAK FLT(void); 
void WEAK DFLT(void);
void WEAK REF(void); 
void WEAK REFF(void);
void WEAK MTR(void); 
void WEAK DHSCS(void);
void WEAK DHSCR(void);
void WEAK DHSZ(void);
void WEAK SPD(void); 
void WEAK DSPD(void);
void WEAK PLSY(void);
void WEAK PWM(void); 
void WEAK PLSR(void);
void WEAK IST(void); 
void WEAK SER(void); 
void WEAK DSER(void);
void WEAK ABSD(void);
void WEAK DABSD(void);
void WEAK INCD(void);
void WEAK TTMR(void);
void WEAK STMR(void);
void WEAK ALT(void); 
void WEAK RAMP(void);
void WEAK ROTS(void);
void WEAK SORT(void);
void WEAK TKY(void); 
void WEAK HKY(void); 
void WEAK DSW(void); 
void WEAK SEGD(void);
void WEAK SEGL(void);
void WEAK ARWS(void);
void WEAK ASC(void); 
void WEAK PR(void);
void WEAK FROM(void);
void WEAK TO(void);
void WEAK RS(void);
void WEAK PRUN(void);
void WEAK ASCI(void);
void WEAK HEX(void); 
void WEAK CCD(void); 
void WEAK VRRD(void);
void WEAK VRSC(void);
void WEAK RS2(void); 
void WEAK PID(void); 
void WEAK ZPUSH(void);
void WEAK ZPOP(void);
void WEAK DECMP(void);
void WEAK DEZCP(void);
void WEAK DEMOV(void);
void WEAK DESTR(void);
void WEAK DEVAL(void);
void WEAK EBCD(void);
void WEAK EBIN(void);

void WEAK DEADD(void);
void WEAK DESUB(void);
void WEAK DEMUL(void);
void WEAK DEDIV(void);
void WEAK DEXP(void); 
void WEAK DLOGE(void);
void WEAK DLOG10(void);
void WEAK DESQR(void);
void WEAK DENEG(void);
void WEAK INT(void);
void WEAK DINT(void); 
void WEAK DSIN(void); 
void WEAK DCOS(void); 
void WEAK DTAN(void); 
void WEAK DASIN(void);
void WEAK DACOS(void);
void WEAK DATAN(void);
void WEAK DRAD(void); 
void WEAK DDEG(void); 
void WEAK WSUM(void); 
void WEAK DWSUM(void);
void WEAK WTOB(void); 
void WEAK BTOW(void); 
void WEAK UNI(void);
void WEAK DIS(void);
void WEAK SWAP(void); 
void WEAK DSWAP(void);
void WEAK SORT2(void);
void WEAK DSORT2(void);
void WEAK TCMP(void); 
void WEAK TZCP(void); 
void WEAK TADD(void); 
void WEAK TSUB(void); 
void WEAK HTOS(void); 
void WEAK STOH(void); 
void WEAK TRD(void);
void WEAK TWR(void);
void WEAK HOUR(void); 
void WEAK GRY(void);
void WEAK DGRY(void); 
void WEAK GBIN(void); 
void WEAK DGBIN(void);
void WEAK RD3A(void); 
void WEAK WR3A(void); 
void WEAK LDP(void);
void WEAK LDF(void);
void WEAK ANDP(void); 
void WEAK ANDF(void); 
void WEAK ORP(void);
void WEAK ORF(void);
void WEAK LD_EQU(void);
void WEAK DLD_EQU(void);
void WEAK LD_GTR(void);
void WEAK DLD_GTR(void);
void WEAK LD_LSS(void);
void WEAK DLD_LSS(void);
void WEAK LD_NEQ(void);
void WEAK DLD_NEQ(void);
void WEAK LD_LEQ(void);
void WEAK DLD_LEQ(void);
void WEAK LD_GEQ(void);
void WEAK DLD_GEQ(void);
void WEAK AND_EQU(void);
void WEAK DAND_EQU(void);
void WEAK AND_GTR(void);
void WEAK DAND_GTR(void);
void WEAK AND_LSS(void);
void WEAK DAND_LSS(void);
void WEAK AND_NEQ(void);
void WEAK DAND_NEQ(void);
void WEAK AND_LEQ(void);
void WEAK DAND_LEQ(void);
void WEAK AND_GEQ(void);
void WEAK DAND_GEQ(void);
void WEAK OR_EQU(void);
void WEAK DOR_EQU(void);
void WEAK OR_GTR(void);
void WEAK DOR_GTR(void);
void WEAK OR_LSS(void);
void WEAK DOR_LSS(void);
void WEAK OR_NEQ(void);
void WEAK DOR_NEQ(void);
void WEAK OR_LEQ(void);
void WEAK DOR_LEQ(void);
void WEAK OR_GEQ(void);
void WEAK DOR_GEQ(void);

//Table :
void (* const FUNC_BASIC[])(void) = {
  FUNC_0,   FUNC_0,   LD,       LDI,      AND,      ANI,      OR,       ORI,
  ERR,      ERR,      ERR,      NOP,      OUT,      BSET,     RST,      FUNC_F
};

void (* const FUNC_EXT_F[])(void) = {
  STL,      STL,      STL,      STL,      ERR,      ERR,      ERR,      RET,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      FUNC_FF
};

void (* const FUNC_EXT_0[])(void) = {
// 0x0000
  NOP,      ERR,      OUT,      BSET,     RST,      OUT,      BSET,     RST,
  PLS,      PLF,      MC,       MCR,      RST_TC,   RST_D,    ERR,      END,
// 0x0010
  CJ,       ERR,      CALL,     ERR,      SRET,     ERR,      IRET,     ERR,
  EI,       ERR,      DI,       ERR,      END,      ERR,      WDT,      ERR,
  FOR,      ERR,      NEXT,     ERR,
// 0x0024
                                          CMP,      DCMP,     ZCP,      DZCP,
  MOV,      DMOV,     SMOV,     ERR,      CML,      DCML,     BMOV,     ERR,
  FMOV,     DFMOV,    XCH,      DXCH,     BCD,      DBCD,     BIN,      DBIN,
// 0x0038
  ADD,      DADD,     SUB,      DSUB,     MUL,      DMUL,     DIV,      DDIV,
  INC,      DINC,     DEC,      DDEC,     WAND,     DWAND,    WOR,      DWOR,
  WXOR,     DWXOR,    NEG,      DNEG,
// 0x004C
                                          ROR,      DROR,     ROL,      DROL,
  RCR,      DRCR,     RCL,      DRCL,     SFTR,     ERR,      SFTL,     ERR,
  WSFR,     ERR,      WSFL,     ERR,      SFWR,     ERR,      SFWL,     ERR,
// 0x0060
  ZRST,     ERR,      DECO,     ERR,      ENCO,     ERR,      SUM,      DSUM,
  BON,      DBON,     MEAN,     DMEAN,    ANS,      ERR,      ANR,      ERR,
  SQR,      DSQR,     FLT,      DFLT,
// 0x0074
                                          REF,      ERR,      REFF,     ERR,
  MTR,      ERR,      DHSCS,    ERR,      DHSCR,    ERR,      DHSZ,     ERR,
  SPD,      DSPD,     PLSY,     ERR,      PWM,      ERR,      PLSR,     ERR,
// 0x0088
  IST,      ERR,      SER,      DSER,     ABSD,     DABSD,    INCD,     ERR,
  TTMR,     ERR,      STMR,     ERR,      ALT,      ERR,      RAMP,     ERR,
  ROTS,     ERR,      SORT,     ERR,
// 0x009C
                                          TKY,      ERR,      HKY,      ERR,
  DSW,      ERR,      SEGD,     ERR,      SEGL,     ERR,      ARWS,     ERR,
  ASC,      ERR,      PR,       ERR,      FROM,     ERR,      TO,       ERR,
// 0x00B0
  RS,       ERR,      PRUN,     ERR,      ASCI,     ERR,      HEX,      ERR,
  CCD,      ERR,      VRRD,     ERR,      VRSC,     ERR,      RS2,      ERR,
  PID,      ERR,      ZPUSH,    ERR,
// 0x00C4
                                          ZPOP,     ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
// 0x00D8
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
// 0x00EC
                                          ERR,      DECMP,    ERR,      DEZCP,
  ERR,      DEMOV,    ERR,      DESTR,    ERR,      DEVAL,    ERR,      EBCD,
  ERR,      EBIN,     ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
// 0x0100
  ERR,      DEADD,    ERR,      DESUB,    ERR,      DEMUL,    ERR,      DEDIV,
  ERR,      DEXP,     ERR,      DLOGE,    ERR,      DLOG10,   ERR,      DESQR,
  ERR,      DENEG,    INT,      DINT,
// 0x0114
                                          ERR,      DSIN,     ERR,      DCOS,
  ERR,      DTAN,     ERR,      DASIN,    ERR,      DACOS,    ERR,      DATAN,
  ERR,      DRAD,     ERR,      DDEG,     ERR,      ERR,      ERR,      ERR,
// 0x0128
  ERR,      ERR,      ERR,      ERR,      WSUM,     DWSUM,    WTOB,     BTOW,
  UNI,      DIS,      SWAP,     DSWAP,    SORT2,    DSORT2,   ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
// 0x013C
                                          ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
// 0x0150
  TCMP,     ERR,      TZCP,     ERR,      TADD,     ERR,      TSUB,     ERR,
  HTOS,     ERR,      STOH,     ERR,      TRD,      ERR,      TWR,      ERR,
  HOUR,     ERR,      ERR,      ERR, 
// 0x0164
                                          GRY,      DGRY,     GBIN,     DGBIN,
  RD3A,     ERR,      WR3A,     ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
// 0x0178
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
// 0x018C
                                          ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
// 0x01A0
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,
// 0x01B4
                                          ERR,      ERR,      ERR,      ERR,
  ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,      ERR,
// 0x01C0
  ERR,      ERR,      LD,       LDI,      AND,      ANI,      OR,       ORI,
  ERR,      ERR,      LDP,      LDF,      ANDP,     ANDF,     ORP,      ORF,
// 0x01D0
  LD_EQU,   DLD_EQU,  LD_GTR,   DLD_GTR,  LD_LSS,   DLD_LSS,  ERR,      ERR,
  LD_NEQ,   DLD_NEQ,  LD_LEQ,   DLD_LEQ,  LD_GEQ,   DLD_GEQ,  ERR,      ERR,
// 0x01E0
  AND_EQU,  DAND_EQU, AND_GTR,  DAND_GTR, AND_LSS,  DAND_LSS, ERR,      ERR,
  AND_NEQ,  DAND_NEQ, AND_LEQ,  DAND_LEQ, AND_GEQ,  DAND_GEQ, ERR,      ERR,
// 0x01F0
  OR_EQU,   DOR_EQU,  OR_GTR,   DOR_GTR,  OR_LSS,   DOR_LSS,  ERR,      ERR,
  OR_NEQ,   DOR_NEQ,  OR_LEQ,   DOR_LEQ,  OR_GEQ,   DOR_GEQ,  ERR,      ERR
};
     
/***************************************************FOR**************************************************/
struct {
  const u16 *Addr[7];  //FOR address records
  u16 cycle[7];        //Current loop iteration count
  u16 count[7];        //Target loop iteration count
  u8  point;           //FOR stack position  
} FOR_CMD;

/***************************************************STL**************************************************/
static u32 PLC_STL_Addr;    //STL instruction address number
static u8  PLC_STL_Status;    //Current STL instruction state: 0 = no STL state in the program; 1 = STL active; 2 = STL stopped 
static u8  PLC_STL_CMD;      //STL flag
static u16 PLC_STL_Count;     //Number of counted coils
static u16 PLC_STL_Coil[256]; //Coil buffer register
/********************************************************************************************************/

u32 PLC_LD_BIT(u32 addr) {
  return PLC_BIT_TEST(addr);
}

u32 PLC_BIT_TEST(u32 addr) {
  if (addr >= PLC_RAM_SIZE * 8U) { legacy_fault(PLC_RUNTIME_BOUNDS); return 0; }
  return (PLC_8BIT(addr / 8U) >> (addr & 7U)) & 1U;
}
void PLC_BIT_ON(u32 addr) { legacy_bit_put(addr, 1); }
void PLC_BIT_OFF(u32 addr) { legacy_bit_put(addr, 0); }

u32 PLC_PL_BIT_TEST(void) {
  return legacy_pulse_test(CMD_ADDR);
}

void PLC_PL_BIT_ON(void) {
  legacy_pulse_put(CMD_ADDR, 1);
}

void PLC_PL_BIT_OFF(void) {
  legacy_pulse_put(CMD_ADDR, 0);
}

u8 PLC_LDP_TEST(void) {
  if ( PLC_PL_BIT_TEST() == 0 ) {
    if ( PLC_ACC_BIT & 0x01 ) {
      PLC_PL_BIT_ON();
      return 1;
    } else
      return 0;
  } else {
    if ( !( PLC_ACC_BIT & 0x01 ) )
      PLC_PL_BIT_OFF();
    return 0;
  } 
}

/*********************************************
* Function: PLC code error handler
* Err_id = 01: Instruction error (unrecognized instruction)
* Err_id = 02: Instruction error (not supporting command)
* Err_id = 10: Data error (data type not recognized)
* Err_id = 11: Data error (data read address is exceeded)
* Err_id = 12: Data error (index Z address unknown)
* Err_id = 13: Data error (indexed Z address exceeded)
* Err_id = 20: CJ instruction address error
* D8061, M8061 = PC hardware error
* D8063, M8063 = Link, communication error
* D8064, M8064 = Parameter error
* D8065, M8065 = Syntax error
* D8066, M8066 = Circuit error
* D8067, M8067 = Operation error
* D8068, M8068 = Operation error latch
***********************************************/
void PLC_PROG_ERROR(u16 err,u16 err_id) {
  (void)err; (void)err_id;
  legacy_fault(PLC_RUNTIME_LEGACY_UNSUPPORTED);
}

//=======================================================================================================
//=======================================================================================================
// GET_ADDR - 2 word
//=======================================================================================================
//=======================================================================================================
u32 get_addr(void) {  
u32 temp, temp1;

  temp1 = legacy_word();
  PLC_PC++;
  temp = ( ( ( legacy_word() & 0xFF ) << 8 ) | ( temp1 & 0xFF ) ) << 1;

  if ( ( temp1 & 0xFF00 ) == 0x8400 )
    Flag_bit = 0;
  else if ( ( temp1 & 0xFF00 ) == 0x8600 )
      switch ( legacy_word() >> 8 ) {
        case 0x80: temp += Addr_D8000; break;
        case 0x82: temp += Addr_TVal;  break;
        case 0x84: temp += Addr_CVal;  break;
        case 0x86: temp += Addr_D0000; break;
        case 0x88: temp += Addr_D1000; break;
      }
  PLC_PC++;
  return temp;
}
//=======================================================================================================
//=======================================================================================================
//   1  80  80  K
//   2  82  80  H
//   3  84  80  X, Y, M, S, M8, C, T
//   4  84  82  K1 4bit
//   5  84  84  K2 8bit
//   6  84  86  K3 12bit
//   7  84  88  K4 16bit
//   8  84  8A  K5 20bit
//   9  84  8C  K6 24bit
//  10  84  8E  K7 28bit
//  11  84  90  K8 32bit
//  12  86  80  D8xxx
//  13  86  82  Txxx
//  14  86  84  Cxxx
//  15  86  86  Dxxx  D <1000
//  16  86  88  Dxxxx D >1000
//  17  88  80  Pxxx
//  18  8A  80  ASCII CHAR
//  19  90  80  KxxxxV
//  20  94  82  K1(X,Y,M,S)V
//  21  94  84  K2(X,Y,M,S)V
//  22  94  86  K3(X,Y,M,S)V
//  23  94  88  K4(X,Y,M,S)V
//  24  94  8A  K5(X,Y,M,S)V
//  25  94  8C  K6(X,Y,M,S)V
//  26  94  8E  K7(X,Y,M,S)V
//  27  94  90  K8(X,Y,M,S)V
//  28  A4  82  K1(X,Y,M,S)Z
//  29  A4  84  K2(X,Y,M,S)Z
//  30  A4  86  K3(X,Y,M,S)Z
//  31  A4  88  K4(X,Y,M,S)Z
//  32  A4  8A  K5(X,Y,M,S)Z
//  33  A4  8C  K6(X,Y,M,S)Z
//  34  A4  8E  K7(X,Y,M,S)Z
//  35  A4  90  K8(X,Y,M,S)Z
//  36  A6  80  D8xxxZ
//  37  A6  82  TxxxZ
//  38  A6  84  CxxxZ
//  39  A6  86  DxxxZ  D <1000
//  40  A6  88  DxxxxZ D >1000
//=======================================================================================================
//=======================================================================================================
 
void MOV_TO_K_H(u8 i, u32 data, u32 addr) { //Calculate the address for MOV to K-group X/Y/S/M devices
u32 LL_BIT;                         //Register holding the number of bits to shift left
u32 tempAddr;
int64_t tempData,tempData1,tempData2;  //Shift 32-bit data

  mov_d_addr |= addr << 8; 
  mov_d_addr += Transfer;                      //Transfer holds an offset for block and multi-point transfers; otherwise it is 0
  LL_BIT = mov_d_addr % 0x20;                     //Number of bits to shift
  tempAddr = (mov_d_addr / 0x20) * 4;              //Starting address  
  switch(i) {            //    Select the bit width, prepare the data, shift it into position, and form the inverted mask for later use
    case 0x82: tempData1 = data & 0x0000000F; tempData1 <<= LL_BIT; tempData2 = ~(0x0000000F << LL_BIT); break;//Transfer K1-width data
    case 0x84: tempData1 = data & 0x000000FF; tempData1 <<= LL_BIT; tempData2 = ~(0x000000FF << LL_BIT); break;//Transfer K2-width data
    case 0x86: tempData1 = data & 0x00000FFF; tempData1 <<= LL_BIT; tempData2 = ~(0x00000FFF << LL_BIT); break;//Transfer K3-width data
    case 0x88: tempData1 = data & 0x0000FFFF; tempData1 <<= LL_BIT; tempData2 = ~(0x0000FFFF << LL_BIT); break;//Transfer K4-width data
    case 0x8A: tempData1 = data & 0x000FFFFF; tempData1 <<= LL_BIT; tempData2 = ~(0x000FFFFF << LL_BIT); break;//Transfer K5-width data
    case 0x8C: tempData1 = data & 0x00FFFFFF; tempData1 <<= LL_BIT; tempData2 = ~(0x00FFFFFF << LL_BIT); break;//Transfer K6-width data
    case 0x8E: tempData1 = data & 0x0FFFFFFF; tempData1 <<= LL_BIT; tempData2 = ~(0x0FFFFFFF << LL_BIT); break;//Transfer K7-width data
    case 0x90: tempData1 = data & 0XFFFFFFFF; tempData1 <<= LL_BIT; tempData2 = ~(0XFFFFFFFF << LL_BIT); break;//Transfer K8-width data
    default: PLC_PC += 3; break;  //Unsupported instruction encountered
  }                                          
  tempData  = PLC_Du(RAM_ADDR + tempAddr);
  tempData &= tempData2;             //Clear the destination bits 
  tempData |= tempData1;        //Insert the data at the required position   
  PLC_Du(RAM_ADDR + tempAddr) = tempData;  //Transfer the data to the destination
}

signed int MOV_K(void) {               //Calculate the K-group X/Y/S/M value
u32 LL_BIT, tempAddr;                   //Register holding the number of bits to shift left
uint64_t tempData;               //Shift 64-bit data

//  mov_d_addr |= Addr << 8;                    
  mov_d_addr += Transfer;                         //Transfer holds an offset for block and multi-point transfers; otherwise it is 0
  LL_BIT = mov_d_addr & 0x1F;                        //Number of bits to shift
  tempAddr = (mov_d_addr >> 5) << 2;                 //Starting address                 
  tempData = PLC_Du(RAM_ADDR + tempAddr),
  tempData >>= LL_BIT;                    
  return (signed int)tempData;
}

//=======================================================================================================
// -----------------------------------------------------------
//=======================================================================================================
u32 addr_value_prog(void) {  
u32 temp; 
u32 type_of, temp1, temp2;

  temp1 = legacy_word();
  PLC_PC++;
  temp2 = legacy_word();

  type_of  = (temp1 & 0xFF00) | (temp2 >> 8);

  mov_d_addr = ( (temp2 & 0x00FF) << 8 ) | (temp1 & 0xFF);
  temp2 = mov_d_addr << 1;

  if      ( type_of == 0x8680 ) {temp = RAM_D8000_ADDR + temp2;            } //Calculate the D address D8000
  else if ( type_of == 0x8682 ) {temp = RAM_T_ADDR + temp2;                } //Calculate the T address 
  else if ( type_of == 0x8684 ) {temp = RAM_C_ADDR + temp2;                } //Calculate the C address
  else if ( type_of == 0x8686 ) {temp = RAM_D0000_ADDR + temp2;            } //Calculate the D address 
  else if ( type_of == 0x8688 ) {temp = RAM_D1000_ADDR + temp2;            } //D1000 and above

  else if ( type_of == 0x8482 ) {temp = MOV_K() & 0x0000000F; Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0    
  else if ( type_of == 0x8484 ) {temp = MOV_K() & 0x000000FF; Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0    
  else if ( type_of == 0x8486 ) {temp = MOV_K() & 0x00000FFF; Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0    
  else if ( type_of == 0x8488 ) {temp = MOV_K() & 0x0000FFFF; Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0    
  else if ( type_of == 0x848A ) {temp = MOV_K() & 0x000FFFFF; Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0    
  else if ( type_of == 0x848C ) {temp = MOV_K() & 0x00FFFFFF; Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0    
  else if ( type_of == 0x848E ) {temp = MOV_K() & 0x0FFFFFFF; Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0    
  else if ( type_of == 0x8490 ) {temp = MOV_K();              Flag_bit = 0;} //Transfer grouped bit-device values such as K4M0  
  PLC_PC++;
  return temp;
}

//// -- V0 = 00xxxxxx V1 = 01xxxxxx V2 = 10xxxxxx V3 = 11xxxxxx

u16 V0_V3(u16 temp) {

  if      ( temp == 0 ) return D8029;
  else if ( temp == 1 ) return D8183;
  else if ( temp == 2 ) return D8185;
  else if ( temp == 3 ) return D8187;
  else return 0;
}

u16 V4_V7(u16 temp) {

  if      ( temp == 0 ) return D8189;
  else if ( temp == 1 ) return D8191;
  else if ( temp == 2 ) return D8193;
  else if ( temp == 3 ) return D8195;
  else return 0;
}

u16 Z0_Z3(u16 temp) {

  if      ( temp == 0 ) return D8028;
  else if ( temp == 1 ) return D8182;
  else if ( temp == 2 ) return D8184;
  else if ( temp == 3 ) return D8186;
  else return 0;
}

u16 Z4_Z7(u16 temp) {

  if      ( temp == 0 ) return D8188;
  else if ( temp == 1 ) return D8190;
  else if ( temp == 2 ) return D8192;
  else if ( temp == 3 ) return D8194;
  else return 0;
}

static u32 legacy_read_register_pair(volatile u16 *high_word,
                                     volatile u16 *low_word) {
  u32 high, low;
  /* Sequence volatile reads; widen before shifting on all C targets. */
  high = *high_word;
  low = *low_word;
  return (high << 16) | low;
}

u32 DZ0_Z3(u16 temp) {

  if      ( temp == 0 ) return legacy_read_register_pair(&D8029, &D8028);
  else if ( temp == 1 ) return legacy_read_register_pair(&D8183, &D8182);
  else if ( temp == 2 ) return legacy_read_register_pair(&D8184, &D8184);
  else if ( temp == 3 ) return legacy_read_register_pair(&D8185, &D8186);
  else return 0;
}

u32 DZ4_Z7(u16 temp) {

  if      ( temp == 0 ) return legacy_read_register_pair(&D8189, &D8188);
  else if ( temp == 1 ) return legacy_read_register_pair(&D8191, &D8190);
  else if ( temp == 2 ) return legacy_read_register_pair(&D8193, &D8192);
  else if ( temp == 3 ) return legacy_read_register_pair(&D8193, &D8194);
  else return 0;
}

//=======================================================================================================
s16 get_value() {  
s32 temp = 0; 
u32 type_of, temp1, temp2, temp3;

  temp1 = legacy_word();
  PLC_PC++;
  temp2 = legacy_word();

  type_of  = (temp1 & 0xFF00) | (temp2 >> 8);

  mov_d_addr = ( (temp2 & 0x00FF) << 8 ) | (temp1 & 0xFF);
  temp2 = mov_d_addr;

//  mov_d_addr = temp1 & 0xFF;
//  temp2 = ((temp2 & 0xFFU) << 8) | mov_d_addr;

  temp3 = PLC_v_z_addr(temp2);

  switch(type_of) {
    case  0x8080: temp = temp2; break;  //Calculate the K value
    case  0x8280: temp = temp2; break;  //Calculate the H value

    case  0x8482: temp = MOV_K()&0x0000000F; bitT_get = 1; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8484: temp = MOV_K()&0x000000FF; bitT_get = 1; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8486: temp = MOV_K()&0x00000FFF; bitT_get = 1; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8488: temp = MOV_K()&0x0000FFFF; bitT_get = 1; break; //Transfer grouped bit-device values such as K4M0      

    case  0x8680: temp = PLC_Hs(RAM_D8000_ADDR + (( temp2 + Transfer) << 1) ); break; //Calculate the D address D8000
    case  0x8682: temp = PLC_Hs(RAM_T_ADDR     + (( temp2 + Transfer) << 1) ); break; //Calculate the T address 
    case  0x8684: temp = PLC_Hs(RAM_C_ADDR     + (( temp2 + Transfer) << 1) ); break; //Calculate the C address
    case  0x8686: temp = PLC_Hs(RAM_D0000_ADDR + (( temp2 + Transfer) << 1) ); break; //Calculate the D address 
    case  0x8688: temp = PLC_Hs(RAM_D1000_ADDR + (( temp2 + Transfer) << 1) ); break; //D1000 and above
    /************************************************K register: V indexing*******************************************************************/
    case  0x9080: temp = temp2 + D8029; break; //Calculate the K address with V0 indexing
    case  0x9081: temp = temp2 + D8183; break; //Calculate the K address with V1 indexing
    case  0x9082: temp = temp2 + D8185; break; //Calculate the K address with V2 indexing
    case  0x9083: temp = temp2 + D8187; break; //Calculate the K address with V3 indexing
    case  0x9084: temp = temp2 + D8189; break; //Calculate the K address with V4 indexing
    case  0x9085: temp = temp2 + D8191; break; //Calculate the K address with V5 indexing
    case  0x9086: temp = temp2 + D8193; break; //Calculate the K address with V6 indexing
    case  0x9087: temp = temp2 + D8195; break; //Calculate the K address with V7 indexing

    case  0x9280: temp = temp2 + D8029; break; //Calculate the H address with V0 indexing
    case  0x9281: temp = temp2 + D8183; break; //Calculate the H address with V1 indexing
    case  0x9282: temp = temp2 + D8185; break; //Calculate the H address with V2 indexing
    case  0x9283: temp = temp2 + D8187; break; //Calculate the H address with V3 indexing
    case  0x9284: temp = temp2 + D8189; break; //Calculate the H address with V4 indexing
    case  0x9285: temp = temp2 + D8191; break; //Calculate the H address with V5 indexing
    case  0x9286: temp = temp2 + D8193; break; //Calculate the H address with V6 indexing
    case  0x9287: temp = temp2 + D8195; break; //Calculate the H address with V7 indexing   
    /************************************************K1M0 register: V indexing*******************************************************************/
    case  0x9482: mov_d_addr += V0_V3(temp3); temp = MOV_K()&0x0000000F;break;//K1M0 with V0-V3 indexing  
    case  0x9483: mov_d_addr += V4_V7(temp3); temp = MOV_K()&0x0000000F;break;//K1M0 with V4-V7 indexing  
    /************************************************K2M0 register: V indexing*******************************************************************/
    case  0x9484: mov_d_addr += V0_V3(temp3); temp = MOV_K()&0x000000FF;break;//K2M0 with V0-V3 indexing  
    case  0x9485: mov_d_addr += V4_V7(temp3); temp = MOV_K()&0x000000FF;break;//K2M0 with V4-V7 indexing    
    /***********************************3************K3M0 register: V indexing*******************************************************************/
    case  0x9486: mov_d_addr += V0_V3(temp3); temp = MOV_K()&0x00000FFF;break;//K3M0 with V0-V3 indexing  
    case  0x9487: mov_d_addr += V4_V7(temp3); temp = MOV_K()&0x00000FFF;break;//K3M0 with V4-V7 indexing    
    /************************************************K4M0 register: V indexing*******************************************************************/
    case  0x9488: mov_d_addr += V0_V3(temp3); temp = MOV_K()&0x0000FFFF;break;//K4M0 with V0-V3 indexing  
    case  0x9489: mov_d_addr += V4_V7(temp3); temp = MOV_K()&0x0000FFFF;break;//K4M0 with V4-V7 indexing            

    case  0x9682: temp = PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2);break;//Calculate the T address with V0-V3 indexing
    case  0x9683: temp = PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2);break;//Calculate the T address with V4-V7 indexing
    case  0x9684: temp = PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2);break;//Calculate the C address with V0-V3 indexing
    case  0x9685: temp = PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2);break;//Calculate the C address with V4-V7 indexing

    case  0x9686: temp = PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2);break;//Calculate the D address with V0-V3 indexing
    case  0x9687: temp = PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2);break;//Calculate the D address with V4-V7 indexing
    case  0x9688: temp = PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2);break;//D1000 and above with V0-V3 indexing
    case  0x9689: temp = PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2);break;//D1000 and above with V4-V7 indexing
  
    case  0xA080: temp = temp2 + D8028; break; //Calculate the K address with Z0 indexing
    case  0xA081: temp = temp2 + D8182; break; //Calculate the K address with Z1 indexing
    case  0xA082: temp = temp2 + D8184; break; //Calculate the K address with Z2 indexing
    case  0xA083: temp = temp2 + D8186; break; //Calculate the K address with Z3 indexing
    case  0xA084: temp = temp2 + D8188; break; //Calculate the K address with Z4 indexing
    case  0xA085: temp = temp2 + D8190; break; //Calculate the K address with Z5 indexing
    case  0xA086: temp = temp2 + D8192; break; //Calculate the K address with Z6 indexing
    case  0xA087: temp = temp2 + D8194; break; //Calculate the K address with Z7 indexing   

    case  0xA280: temp = temp2 + D8028; break; //Calculate the H address with Z0 indexing
    case  0xA281: temp = temp2 + D8182; break; //Calculate the H address with Z1 indexing
    case  0xA282: temp = temp2 + D8184; break; //Calculate the H address with Z2 indexing
    case  0xA283: temp = temp2 + D8186; break; //Calculate the H address with Z3 indexing
    case  0xA284: temp = temp2 + D8188; break; //Calculate the H address with Z4 indexing
    case  0xA285: temp = temp2 + D8190; break; //Calculate the H address with Z5 indexing
    case  0xA286: temp = temp2 + D8192; break; //Calculate the H address with Z6 indexing
    case  0xA287: temp = temp2 + D8194; break; //Calculate the H address with Z7 indexing   
    /************************************************K1M0 register: Z indexing*******************************************************************/
    case  0xA482: mov_d_addr += Z0_Z3(temp3); temp = MOV_K()&0x0000000F;break;//K1M0 with V0-V3 indexing  
    case  0xA483: mov_d_addr += Z4_Z7(temp3); temp = MOV_K()&0x0000000F;break;//K1M0 with V4-V7 indexing  
    /************************************************K2M0 register: Z indexing*******************************************************************/
    case  0xA484: mov_d_addr += Z0_Z3(temp3); temp = MOV_K()&0x000000FF;break;//K2M0 with V0-V3 indexing  
    case  0xA485: mov_d_addr += Z4_Z7(temp3); temp = MOV_K()&0x000000FF;break;//K2M0 with V4-V7 indexing    
    /************************************************K3M0 register: Z indexing*******************************************************************/
    case  0xA486: mov_d_addr += Z0_Z3(temp3); temp = MOV_K()&0x00000FFF;break;//K3M0 with V0-V3 indexing  
    case  0xA487: mov_d_addr += Z4_Z7(temp3); temp = MOV_K()&0x00000FFF;break;//K3M0 with V4-V7 indexing    
    /************************************************K4M0 register: Z indexing*******************************************************************/
    case  0xA488: mov_d_addr += Z0_Z3(temp3); temp = MOV_K()&0x0000FFFF;break;//K4M0 with V0-V3 indexing
    case  0xA489: mov_d_addr += Z4_Z7(temp3); temp = MOV_K()&0x0000FFFF;break;//K4M0 with V4-V7 indexing    

    case  0xA682: temp = PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2);break;//Calculate the T address with Z0-Z3 indexing
    case  0xA683: temp = PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2);break;//Calculate the T address with Z4-Z7 indexing
    case  0xA684: temp = PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2);break;//Calculate the C address with Z0-Z3 indexing
    case  0xA685: temp = PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2);break;//Calculate the C address with Z4-Z7 indexing

    case  0xA686: temp = PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2);break;//Calculate the D address with Z0-Z3 indexing
    case  0xA687: temp = PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2);break;//Calculate the D address with Z4-Z7 indexing
    case  0xA688: temp = PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2);break;//D1000 and above with Z0-Z3 indexing
    case  0xA689: temp = PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2);break;//D1000 and above with Z4-Z7 indexing
  }  
  PLC_PC++;
  return temp;
}
//=======================================================================================================
#ifdef USE_DOUBLE
s32 get_valueD(void) {   
u32 type_of, temp1, temp2, temp3;
u32 temp = 0;
s32_union tempD = {0};

  temp1 = legacy_word();
  PLC_PC++;
  temp2 = legacy_word();

  type_of  = (temp1 & 0xFF00) | (temp2 >> 8);

  mov_d_addr = ( (temp2 & 0x00FF) << 8 ) | (temp1 & 0xFF);
  temp2 = mov_d_addr;

//  mov_d_addr = temp1 & 0xFF;
//  temp2 = ((temp2 & 0xFFU) << 8) | mov_d_addr;

  temp3 = PLC_v_z_addr(temp2);

  switch ( type_of ) {    
    case  0x8080: tempD.d16[0] = temp2; PLC_PC++; tempD.d16[1] = get_value(); PLC_PC -= 2; break;//Calculate the K value
    case  0x8280: tempD.d16[0] = temp2; PLC_PC++; tempD.d16[1] = get_value(); PLC_PC -= 2; break;//Calculate the H value

    case  0x8482: tempD.d32 = MOV_K()&0x0000000F; bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8484: tempD.d32 = MOV_K()&0x000000FF; bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8486: tempD.d32 = MOV_K()&0x00000FFF; bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8488: tempD.d32 = MOV_K()&0x0000FFFF; bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0    
    case  0x848A: tempD.d32 = MOV_K()&0x000FFFFF; bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0    
    case  0x848C: tempD.d32 = MOV_K()&0x00FFFFFF; bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0    
    case  0x848E: tempD.d32 = MOV_K()&0x0FFFFFFF; bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8490: tempD.d32 = MOV_K();            bitT_get = 1; PLC_PC++; break; //Transfer grouped bit-device values such as K4M0   
      
    case  0x8680: tempD.d32 = PLC_Ws(RAM_D8000_ADDR + (temp2 << 1)); PLC_PC++; break;                 //Calculate the D address D8000
    case  0x8682: tempD.d32 = PLC_Ws(RAM_T_ADDR     + (temp2 << 1)); PLC_PC++; break;                 //Calculate the T address 
    case  0x8684: tempD.d32 = PLC_Ws(RAM_C_ADDR     + (temp2 << 1)); PLC_PC++; break;                 //Calculate the C address
    case  0x8686: tempD.d32 = PLC_Ws(RAM_D0000_ADDR + (temp2 << 1)); PLC_PC++; break;                 //Calculate the D address 
    case  0x8688: tempD.d32 = PLC_Ws(RAM_D1000_ADDR + (temp2 << 1)); PLC_PC++; break;                 //D1000 and above

    case  0xA080: tempD.d16[0] = temp2 + D8028; PLC_PC++; tempD.d16[1] = get_value() + D8029; PLC_PC -= 2; break; //Calculate the K address with Z0 indexing
    case  0xA081: tempD.d16[0] = temp2 + D8182; PLC_PC++; tempD.d16[1] = get_value() + D8183; PLC_PC -= 2; break; //Calculate the K address with Z1 indexing
    case  0xA082: tempD.d16[0] = temp2 + D8184; PLC_PC++; tempD.d16[1] = get_value() + D8185; PLC_PC -= 2; break; //Calculate the K address with Z2 indexing
    case  0xA083: tempD.d16[0] = temp2 + D8186; PLC_PC++; tempD.d16[1] = get_value() + D8187; PLC_PC -= 2; break; //Calculate the K address with Z3 indexing
    case  0xA084: tempD.d16[0] = temp2 + D8188; PLC_PC++; tempD.d16[1] = get_value() + D8189; PLC_PC -= 2; break; //Calculate the K address with Z4 indexing
    case  0xA085: tempD.d16[0] = temp2 + D8190; PLC_PC++; tempD.d16[1] = get_value() + D8191; PLC_PC -= 2; break; //Calculate the K address with Z5 indexing
    case  0xA086: tempD.d16[0] = temp2 + D8192; PLC_PC++; tempD.d16[1] = get_value() + D8193; PLC_PC -= 2; break; //Calculate the K address with Z6 indexing
    case  0xA087: tempD.d16[0] = temp2 + D8194; PLC_PC++; tempD.d16[1] = get_value() + D8195; PLC_PC -= 2; break; //Calculate the K address with Z7 indexing 

    case  0xA280: tempD.d16[0] = temp2 + D8028; PLC_PC++; tempD.d16[1] = get_value() + D8029; PLC_PC -= 2; break; //Calculate the H address with Z0 indexing
    case  0xA281: tempD.d16[0] = temp2 + D8182; PLC_PC++; tempD.d16[1] = get_value() + D8183; PLC_PC -= 2; break; //Calculate the H address with Z1 indexing
    case  0xA282: tempD.d16[0] = temp2 + D8184; PLC_PC++; tempD.d16[1] = get_value() + D8185; PLC_PC -= 2; break; //Calculate the H address with Z2 indexing
    case  0xA283: tempD.d16[0] = temp2 + D8186; PLC_PC++; tempD.d16[1] = get_value() + D8187; PLC_PC -= 2; break; //Calculate the H address with Z3 indexing
    case  0xA284: tempD.d16[0] = temp2 + D8188; PLC_PC++; tempD.d16[1] = get_value() + D8189; PLC_PC -= 2; break; //Calculate the H address with Z4 indexing
    case  0xA285: tempD.d16[0] = temp2 + D8190; PLC_PC++; tempD.d16[1] = get_value() + D8191; PLC_PC -= 2; break; //Calculate the H address with Z5 indexing
    case  0xA286: tempD.d16[0] = temp2 + D8192; PLC_PC++; tempD.d16[1] = get_value() + D8193; PLC_PC -= 2; break; //Calculate the H address with Z6 indexing
    case  0xA287: tempD.d16[0] = temp2 + D8194; PLC_PC++; tempD.d16[1] = get_value() + D8195; PLC_PC -= 2; break; //Calculate the H address with Z7 indexing         
    /************************************************K1M0 register: Z indexing*******************************************************************/
    case  0xA482: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K()&0x0000000F; PLC_PC++; break;//K1M0 with V0-V3 indexing  
    case  0xA483: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K()&0x0000000F; PLC_PC++; break;//K1M0 with V4-V7 indexing  
    /************************************************K2M0 register: Z indexing*****************************************************************/
    case  0xA484: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K()&0x000000FF; PLC_PC++; break;//K2M0 with V0-V3 indexing  
    case  0xA485: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K()&0x000000FF; PLC_PC++; break;//K2M0 with V4-V7 indexing    
    /************************************************K3M0 register: Z indexing*******************************************************************/
    case  0xA486: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K()&0x00000FFF; PLC_PC++; break;//K3M0 with V0-V3 indexing  
    case  0xA487: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K()&0x00000FFF; PLC_PC++; break;//K3M0 with V4-V7 indexing    
    /************************************************K4M0 register: Z indexing*******************************************************************/
    case  0xA488: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K()&0x0000FFFF; PLC_PC++; break;//K4M0 with V0-V3 indexing
    case  0xA489: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K()&0x0000FFFF; PLC_PC++; break;//K4M0 with V4-V7 indexing    
    /************************************************K5M0 register: Z indexing*******************************************************************/
    case  0xA48A: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K()&0x000FFFFF; PLC_PC++; break;//K1M0 with V0-V3 indexing  
    case  0xA48B: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K()&0x000FFFFF; PLC_PC++; break;//K1M0 with V4-V7 indexing  
    /************************************************K6M0 register: Z indexing*******************************************************************/
    case  0xA48C: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K()&0x00FFFFFF; PLC_PC++; break;//K2M0 with V0-V3 indexing  
    case  0xA48D: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K()&0x00FFFFFF; PLC_PC++; break;//K2M0 with V4-V7 indexing    
    /************************************************K7M0 register: Z indexing*******************************************************************/
    case  0xA48E: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K()&0x0FFFFFFF; PLC_PC++; break;//K3M0 with V0-V3 indexing  
    case  0xA48F: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K()&0x0FFFFFFF; PLC_PC++; break;//K3M0 with V4-V7 indexing    
    /************************************************K8M0 register: Z indexing*******************************************************************/
    case  0xA490: mov_d_addr += DZ0_Z3(temp3); tempD.d32 = MOV_K(); PLC_PC++; break;//K4M0 with V0-V3 indexing
    case  0xA491: mov_d_addr += DZ4_Z7(temp3); tempD.d32 = MOV_K(); PLC_PC++; break;//K4M0 with V4-V7 indexing  
    /************************************************T register: Z indexing*******************************************************************/
    case  0xA682: temp = (PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2; if (temp >= 510)   {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_T_ADDR + (temp << 1)); PLC_PC++;} break;//Calculate the T address with Z0-Z3 indexing
    case  0xA683: temp = (PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2; if (temp >= 510)   {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_T_ADDR + (temp << 1)); PLC_PC++;} break;//Calculate the T address with Z4-Z7 indexing
    /************************************************C register: Z indexing*******************************************************************/
    case  0xA684: temp = (PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2; if (temp >= 510)   {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_C_ADDR + (temp << 1)); PLC_PC++;} break;//Calculate the C address with Z0-Z3 indexing
    case  0xA685: temp = (PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2; if (temp >= 510)   {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_C_ADDR + (temp << 1)); PLC_PC++;} break;//Calculate the C address with Z4-Z7 indexing
    /************************************************D register: Z indexing*******************************************************************/
    case  0xA686: temp = (PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2; if (temp >= 15998) {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_D0000_ADDR + (temp << 1)); PLC_PC++;} break;//Calculate the D address with Z0-Z3 indexing
    case  0xA687: temp = (PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2; if (temp >= 15998) {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_D0000_ADDR + (temp << 1)); PLC_PC++;} break;//Calculate the D address with Z4-Z7 indexing
    case  0xA688: temp = (PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2; if (temp >= 13998) {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_D1000_ADDR + (temp << 1)); PLC_PC++;} break;//D1000 and above with Z0-Z3 indexing
    case  0xA689: temp = (PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2; if (temp >= 13998) {PLC_PROG_ERROR(M8067,6706); tempD.d32=0;} else {tempD.d32 = PLC_Ws(RAM_D1000_ADDR + (temp << 1)); PLC_PC++;} break;//D1000 and above with Z4-Z7 indexing      
  }
  PLC_PC += 2;
  return tempD.d32;
}
#endif
//=======================================================================================================
void put_value(void) {  
u32 type_of, temp1, temp2, temp3, temp4;

  temp1 = legacy_word();
  PLC_PC++;
  temp2 = legacy_word();

  type_of  = (temp1 & 0xFF00) | (temp2 >> 8);

  mov_d_addr = temp1 & 0xFF;
  temp2 = ((temp2 & 0xFFU) << 8) | mov_d_addr;

  value &= 0xFFFF;

  temp3 = PLC_v_z_addr(temp2);
  temp4 = PLC_D_C_T_addr(temp2) >> 8;

  switch(type_of) {
    case  0x8482: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8484: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8486: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case  0x8488: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    
    case  0x8680: PLC_Hs(RAM_D8000_ADDR + (( temp2 + Transfer) << 1) ) = (u16)value; PLC_PC++; break; //Calculate the D address D8000
    case  0x8682: PLC_Hs(RAM_T_ADDR     + (( temp2 + Transfer) << 1) ) = (u16)value; PLC_PC++; break; //Calculate the T address 
    case  0x8684: PLC_Hs(RAM_C_ADDR     + (( temp2 + Transfer) << 1) ) = (u16)value; PLC_PC++; break; //Calculate the C address
    case  0x8686: PLC_Hs(RAM_D0000_ADDR + (( temp2 + Transfer) << 1) ) = (u16)value; PLC_PC++; break; //Calculate the D address 
    case  0x8688: PLC_Hs(RAM_D1000_ADDR + (( temp2 + Transfer) << 1) ) = (u16)value; PLC_PC++; break; //D1000 and above
    /************************************************K1M0 register: V indexing*******************************************************************/
    case  0x9482: mov_d_addr += V0_V3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K1M0 with V0-V3 indexing  
    case  0x9483: mov_d_addr += V4_V7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K1M0 with V4-V7 indexing  
    /************************************************K2M0 register: V indexing*******************************************************************/
    case  0x9484: mov_d_addr += V0_V3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K2M0 with V0-V3 indexing  
    case  0x9485: mov_d_addr += V4_V7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K2M0 with V4-V7 indexing    
    /************************************************K3M0 register: V indexing**************************************************************/
    case  0x9486: mov_d_addr += V0_V3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K3M0 with V0-V3 indexing  
    case  0x9487: mov_d_addr += V4_V7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K3M0 with V4-V7 indexing    
    /************************************************K4M0 register: V indexing**************************************************************/
    case  0x9488: mov_d_addr += V0_V3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K4M0 with V0-V3 indexing  
    case  0x9489: mov_d_addr += V4_V7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K4M0 with V4-V7 indexing            

    case  0x9682: PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the T address with V0-V3 indexing
    case  0x9683: PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the T address with V4-V7 indexing
    case  0x9684: PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the C address with V0-V3 indexing
    case  0x9685: PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the C address with V4-V7 indexing

    case  0x9686: PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the D address with V0-V3 indexing
    case  0x9687: PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the D address with V4-V7 indexing
    case  0x9688: PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + V0_V3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//D1000 and above with V0-V3 indexing
    case  0x9689: PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + V4_V7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//D1000 and above with V4-V7 indexing
    /************************************************K1M0 register: Z indexing*******************************************************************/
    case  0xA482: mov_d_addr += Z0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K1M0 with V0-V3 indexing  
    case  0xA483: mov_d_addr += Z4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K1M0 with V4-V7 indexing  
    /************************************************K2M0 register: Z indexing**************************************************************/
    case  0xA484: mov_d_addr += Z0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K2M0 with V0-V3 indexing  
    case  0xA485: mov_d_addr += Z4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K2M0 with V4-V7 indexing    
    /************************************************K3M0 register: Z indexing**************************************************************/
    case  0xA486: mov_d_addr += Z0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K3M0 with V0-V3 indexing  
    case  0xA487: mov_d_addr += Z4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K3M0 with V4-V7 indexing    
    /************************************************K4M0 register: Z indexing**************************************************************/
    case  0xA488: mov_d_addr += Z0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K4M0 with V0-V3 indexing
    case  0xA489: mov_d_addr += Z4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++; break;//K4M0 with V4-V7 indexing      

    case  0xA682: PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the C address with Z0-Z3 indexing
    case  0xA683: PLC_Hs(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the C address with Z4-Z7 indexing
    case  0xA684: PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the C address with Z0-Z3 indexing
    case  0xA685: PLC_Hs(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the C address with Z4-Z7 indexing

    case  0xA686: PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the D address with Z0-Z3 indexing
    case  0xA687: PLC_Hs(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//Calculate the D address with Z4-Z7 indexing
    case  0xA688: PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + Z0_Z3(temp3) ) * 2) = (u16)value; PLC_PC++; break;//D1000 and above with Z0-Z3 indexing
    case  0xA689: PLC_Hs(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + Z4_Z7(temp3) ) * 2) = (u16)value; PLC_PC++; break;//D1000 and above with Z4-Z7 indexing
    }
}
//=======================================================================================================
#ifdef USE_DOUBLE
void put_valueD(void) {  
u32 type_of, temp1, temp2, temp3, temp4;

  temp1 = legacy_word();
  PLC_PC++;
  temp2 = legacy_word();

  type_of  = (temp1 & 0xFF00) | (temp2 >> 8);

  mov_d_addr = temp1 & 0xFF;
  temp2 = ((temp2 & 0xFFU) << 8) | mov_d_addr;

  temp3 = PLC_v_z_addr(temp2);
  temp4 = PLC_D_C_T_addr(temp2) >> 8;

  switch(type_of) {
    case 0x8482: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case 0x8484: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case 0x8486: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case 0x8488: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case 0x848A: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case 0x848C: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case 0x848E: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0    
    case 0x8490: MOV_TO_K_H(type_of, value, legacy_word()); PLC_PC++; bitT_put = 1; break; //Transfer grouped bit-device values such as K4M0     
    
    case 0x8680: PLC_Ws(RAM_D8000_ADDR + (( temp2 + Transfer ) << 1) ) = value, PLC_PC++; break; //Calculate the D address D8000
    case 0x8682: PLC_Ws(RAM_T_ADDR     + (( temp2 + Transfer ) << 1) ) = value, PLC_PC++; break; //Calculate the T address 
    case 0x8684: PLC_Ws(RAM_C_ADDR     + (( temp2 + Transfer ) << 1) ) = value, PLC_PC++; break; //Calculate the C address
    case 0x8686: PLC_Ws(RAM_D0000_ADDR + (( temp2 + Transfer ) << 1) ) = value, PLC_PC++; break; //Calculate the D address 
    case 0x8688: PLC_Ws(RAM_D1000_ADDR + (( temp2 + Transfer ) << 1) ) = value, PLC_PC++; break; //D1000 and above
    /************************************************K1M0 register: Z indexing*******************************************************************/
    case 0xA482: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K1M0 with V0-V3 indexing  
    case 0xA483: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K1M0 with V4-V7 indexing  
    /************************************************K2M0 register: Z indexing*******************************************************************/
    case 0xA484: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K2M0 with V0-V3 indexing  
    case 0xA485: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K2M0 with V4-V7 indexing    
    /************************************************K3M0 register: Z indexing*******************************************************************/
    case 0xA486: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K3M0 with V0-V3 indexing  
    case 0xA487: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K3M0 with V4-V7 indexing    
    /************************************************K4M0 register: Z indexing*******************************************************************/
    case 0xA488: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K4M0 with V0-V3 indexing
    case 0xA489: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K4M0 with V4-V7 indexing    
    /************************************************K5M0 register: Z indexing*******************************************************************/
    case 0xA48A: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K5M0 with V0-V3 indexing  
    case 0xA48B: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K5M0 with V4-V7 indexing    
    /************************************************K6M0 register: Z indexing*******************************************************************/
    case 0xA48C: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K6M0 with V0-V3 indexing  
    case 0xA48D: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K6M0 with V4-V7 indexing    
    /************************************************K7M0 register: Z indexing*******************************************************************/
    case 0xA48E: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K7M0 with V0-V3 indexing  
    case 0xA48F: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K7M0 with V4-V7 indexing    
    /************************************************K8M0 register: Z indexing*******************************************************************/
    case 0xA490: mov_d_addr += DZ0_Z3(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//k8M0 with V0-V4 indexing  
    case 0xA491: mov_d_addr += DZ4_Z7(temp3); MOV_TO_K_H(type_of, value, temp4); PLC_PC++;break;//K8M0 with V4-V7 indexing      
    /************************************************T register: Z indexing*******************************************************************/
    case 0xA682: PLC_Ws(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2) = value; PLC_PC++;break;//Calculate the T address with Z0-Z3 indexing
    case 0xA683: PLC_Ws(RAM_T_ADDR     + ( PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2) = value; PLC_PC++;break;//Calculate the T address with Z4-Z7 indexing
    case 0xA684: PLC_Ws(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2) = value; PLC_PC++;break;//Calculate the C address with Z0-Z3 indexing
    case 0xA685: PLC_Ws(RAM_C_ADDR     + ( PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2) = value; PLC_PC++;break;//Calculate the C address with Z4-Z7 indexing

    case 0xA686: PLC_Ws(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2) = value; PLC_PC++;break;//Calculate the D address with Z0-Z3 indexing
    case 0xA687: PLC_Ws(RAM_D0000_ADDR + ( PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2) = value; PLC_PC++;break;//Calculate the D address with Z4-Z7 indexing
    case 0xA688: PLC_Ws(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + DZ0_Z3(temp3) ) * 2) = value; PLC_PC++;break;//D1000 and above with Z0-Z3 indexing
    case 0xA689: PLC_Ws(RAM_D1000_ADDR + ( PLC_D_C_T_addr(temp2) + DZ4_Z7(temp3) ) * 2) = value; PLC_PC++;break;//D1000 and above with Z4-Z7 indexing
  }
  PLC_PC += 2;
}
#endif
//=======================================================================================================
#ifdef USE_FLOAT
float get_valueF(void) {  
u32 type_of, temp1, temp2, temp3; // Data1, Data2, Data3, Data4;
float_union tempF = {0};

  temp1 = legacy_word();
  PLC_PC++;
  temp3 = legacy_word();
  PLC_PC++;

  type_of = (temp1 & 0xFF00) | (temp3 >> 8);

  temp1  = (temp3 << 8) | (temp1 & 0xFF);

  temp3 = legacy_word();
  PLC_PC++;
  temp2 = legacy_word();
  PLC_PC++;

  temp2  = (temp2 << 8) | (temp3 & 0xFF);

  if      (type_of == 0x8080) {tempF.d16[0] = temp1; tempF.d16[1] = temp2;} //Calculate the K value
  else if (type_of == 0x8280) {tempF.d16[0] = temp1; tempF.d16[1] = temp2;} //Calculate the H value            
  else if (type_of == 0x8680) {tempF.data = PLC_Wf(RAM_D8000_ADDR + (temp1 << 1));} //Calculate the D address D8000
  else if (type_of == 0x8682) {tempF.data = PLC_Wf(RAM_T_ADDR     + (temp1 << 1));} //Calculate the T address 
  else if (type_of == 0x8684) {tempF.data = PLC_Wf(RAM_C_ADDR     + (temp1 << 1));} //Calculate the C address
  else if (type_of == 0x8686) {tempF.data = PLC_Wf(RAM_D0000_ADDR + (temp1 << 1));} //Calculate the D address 
  else if (type_of == 0x8688) {tempF.data = PLC_Wf(RAM_D1000_ADDR + (temp1 << 1));} //D1000 and above
  return tempF.data;
}

void put_valueF(void) { 
u32 temp = 0;

  temp = get_addr();
  PLC_16BIT(temp)     = FLOAT.d16[0];
  PLC_16BIT(temp + 2) = FLOAT.d16[1];
//  PLC_PC += 2;
}
#endif
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
void timer_enable(u32 timer_number) {
u16 current, preset;

  current = PLC_16BIT(Addr_TVal + timer_number * 2);
  preset = PLC_16BIT(Addr_TMax + timer_number * 2);
  if ( current < preset ) {
    if ( PLC_BIT_TEST((Addr_TO * 8U) + timer_number) )
      PLC_BIT_ON((0x0580U * 8U) + timer_number);             //Reset coil
    else {
      PLC_BIT_OFF((Addr_TO * 8U) + timer_number);      //OFF when the value is less than the set value
      if ( PLC_BIT_TEST((0x0580U * 8U) + timer_number) ) {
        PLC_16BIT(Addr_TVal + timer_number * 2) = 0;
        PLC_BIT_OFF((0x0580U * 8U) + timer_number);
      }
    }
  } else
    PLC_BIT_ON((Addr_TO * 8U) + timer_number);         //ON when the value reaches the set value
}

void timer_disble(u32 timer_number) {     

  PLC_16BIT(Addr_TVal + timer_number * 2) = 0;
  PLC_16BIT(Addr_TMax + timer_number * 2) = 0;
  PLC_BIT_OFF((Addr_TO * 8U) + timer_number);
  PLC_BIT_OFF((Addr_TE * 8U) + timer_number);
  PLC_BIT_OFF((0x0580U * 8U) + timer_number);
}
//=======================================================================================================
void FUNC_TIMER(void) {
u32 temp, tempV, tempN;

  tempN = CMD_REG & 0x00FF;
  CMD_REG = Addr_TE * 8 + tempN;
  temp = legacy_word();
  switch ( temp >> 8) { 
    case 0x80: //// K
      if ( PLC_ACC_BIT & 0x01 ) {
        tempV = temp & 0x00FF;
        PLC_PC++;
        tempV += (legacy_word() & 0x00FF) << 8;

        PLC_16BIT(Addr_TMax + tempN * 2) = tempV;
        timer_enable(tempN);
        OUT();
      } else {
        PLC_PC++;
        timer_disble(tempN);
        OUT();   //disable T coil    1600 / 8 = 2C0
        CMD_REG = Addr_TO * 8 + tempN;
        OUT();   //reset T over coil  600 / 8 = C0
      }

      break;
    case 0x86: //// D
      if ( PLC_ACC_BIT & 0x01 ) {
        tempV = (temp & 0x00FF) >> 1;
        PLC_PC++;
        tempV += (legacy_word() & 0x00FF) << 7;
        if ( (legacy_word() >> 8) == 0x88 )
          tempV += 1000; 
        PLC_16BIT(Addr_TMax + tempN * 2) = PLC_16BIT(Addr_D0000 + tempV * 2);
        timer_enable(tempN);
        OUT();
      } else {
        PLC_PC++;
        timer_disble(tempN);
        OUT();   //disable T coil    1600 / 8 = 2C0
        CMD_REG = Addr_TO * 8 + tempN;
        OUT();   //reset T over coil  600 / 8 = C0
      }

      break;
  }  
  PLC_PC++; /* Consume the last preset word. */
}
//=======================================================================================================
//=======================================================================================================
void FUNC_COUNTER() {
u32 temp, tempV;
u32 C;

  temp = CMD_REG & 0x00FF;
  switch (legacy_word() >> 8) {
    case 0x80:
      if ( PLC_ACC_BIT & 0x01 ) {

        tempV = legacy_word() & 0x00FF;
        PLC_PC++;
        tempV += (legacy_word() & 0x00FF) << 8;
        if ( temp >= 200 ) {                     //Check whether this is a counter register numbered C200 or higher
          PLC_PC++;
          tempV += (legacy_word() & 0x00FF) << 16;
          PLC_PC++;
          tempV += (legacy_word() & 0x00FF) << 24;
          C = RAM_ADDR + Addr_CHVal + ((temp - 200) << 2); 
          if ( PLC_Ws(C) < tempV ) {            //Compare the current counter value with the target value
            if ( !PLC_BIT_TEST(Addr_CE * 8 + temp) )
              PLC_Ws(C) += 1;
          }
          if ( PLC_Ws(C) < tempV )              //Compare the overflow value
            PLC_BIT_OFF(Addr_CO * 8 + temp);     
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);
        } else {
          if ( PLC_16BIT(Addr_CVal + temp * 2) < tempV ) {
            if ( !PLC_BIT_TEST(Addr_CE * 8 + temp) )
              PLC_16BIT(Addr_CVal + temp * 2)++;
          }
          if ( PLC_16BIT(Addr_CVal + temp * 2) <= tempV )
            PLC_BIT_OFF(Addr_CO * 8 + temp);     
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);
        }
        CMD_REG = Addr_CE * 8 + (u8)temp;
        OUT();
      } else {

        tempV = legacy_word() & 0x00FF;
        PLC_PC++;
        tempV += (legacy_word() & 0x00FF) << 8;
        if ( temp >= 200 ) {                //Check whether this is a counter register numbered C200 or higher
          PLC_PC++;
          tempV += (legacy_word() & 0x00FF) << 16;
          PLC_PC++;
          tempV += (legacy_word() & 0x00FF) << 24;
          C = RAM_ADDR + Addr_CHVal + ((temp - 200) << 2); 
          if ( PLC_Ws(C) < tempV )              //Compare the current counter value with the target value
            PLC_BIT_OFF(Addr_CO * 8 + temp);     
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);        
        } else {
          if ( PLC_16BIT(Addr_CVal + temp * 2) <= tempV )
            PLC_BIT_OFF(Addr_CO * 8 + temp);     
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);       
        }
        CMD_REG = Addr_CE * 8 + (u8)temp;
        OUT();
      }

      break;  //Assign a K-group value
    case 0x86:
      tempV = (legacy_word() & 0x00FF) >> 1;
      PLC_PC++;
      switch (legacy_word() >> 8) { 
        case 0x86: tempV += (legacy_word() & 0x00FF) << 7;        break;
        case 0x88: tempV += 1000 + (legacy_word() & 0x00FF) << 7; break; 
      }
      if ( PLC_ACC_BIT & 0x01 ) {

        tempV = PLC_16BIT(Addr_D0000 + tempV * 2);
        if ( temp >= 200 ) {              //Check whether this is a counter register numbered C200 or higher
          tempV += PLC_16BIT(Addr_D0000 + tempV * 2 + 2) << 16;
          C = RAM_ADDR + Addr_CHVal + ((temp - 200) << 2); 
          if ( PLC_Ws(C) < tempV ) {     //Compare the current counter value with the target value
            if ( !PLC_BIT_TEST(Addr_CE * 8 + temp) )
              PLC_Ws(C) += 1;
          }
          if ( PLC_16BIT(Addr_CVal + temp * 2) < tempV )
            PLC_BIT_OFF(Addr_CO * 8 + temp);     
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);
          PLC_PC += 2;                  
        } else {
          if ( PLC_16BIT(Addr_CVal + temp * 2) < tempV ) {
            if ( !PLC_BIT_TEST(Addr_CE * 8 + temp) )
              PLC_16BIT(Addr_CVal + temp * 2)++;
          }
          if ( PLC_16BIT(Addr_CVal + temp * 2) <= tempV )
            PLC_BIT_OFF(Addr_CO * 8 + temp);     
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);
        }
        CMD_REG = Addr_CE * 8 + (u8)temp;
        OUT();
      } else {

        tempV = PLC_16BIT(Addr_D0000 + tempV * 2);
        if ( temp >= 200 ) {              //Check whether this is a counter register numbered C200 or higher
          tempV += PLC_16BIT(Addr_D0000 + tempV * 2 + 2) << 16;
          C = RAM_ADDR + Addr_CHVal + ((temp - 200) << 2);
          if ( PLC_Ws(C) < tempV )      //Compare the overflow value
            PLC_BIT_OFF(Addr_CO * 8 + temp);
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);
          PLC_PC += 2;
        } else {
          if ( PLC_16BIT(Addr_CVal + temp * 2) <= tempV )
            PLC_BIT_OFF(Addr_CO * 8 + temp);
          else
            PLC_BIT_ON(Addr_CO * 8 + temp);   //  E00 / 8 = 1C0
        }  
        CMD_REG = Addr_CE * 8 + (u8)temp;
        OUT();          // 2700 / 8 = 4E0
      }
      
      break;  //Assign a D-register value
  }  
  PLC_PC++; /* Consume the last preset word. */
}
//=======================================================================================================
//=======================================================================================================
void RST_Y(void) {
  PLC_16BIT(Addr_Y) = 0;
  PLC_16BIT(Addr_Y + 2) = 0;
}
//=======================================================================================================
//=======================================================================================================
void RST_T_D_C_M_data(void) {
u32 backup_addr;

  for ( backup_addr = Addr_D0000; backup_addr < Addr_D0000 + MAXDXXX;      backup_addr+=2 ) //D0000-D8000  0X1000--0X2F40
    PLC_16BIT(backup_addr) = 0;
  for ( backup_addr = Addr_CHVal; backup_addr < Addr_CHVal + MAXTIMER * 2; backup_addr+=2 ) //C0-C256
    PLC_16BIT(backup_addr) = 0;
  for ( backup_addr = Addr_TVal;  backup_addr < Addr_TVal + MAXTIMER * 2;  backup_addr+=2 ) //T0-T256
    PLC_16BIT(backup_addr) = 0;
  for ( backup_addr = Addr_ML;    backup_addr < Addr_ML + 0x0180;          backup_addr+=2 ) //M0-M3096
    PLC_16BIT(backup_addr) = 0;
}
//=======================================================================================================
//=======================================================================================================
//
//=======================================================================================================
//=======================================================================================================
void ERR(void) {
  PLC_PROG_ERROR(M8065,02);
}

void NOP(void) {
}

void END(void) {
  PLC_PRG_STOP = 1;
}

void FUNC_0(void) {
  u16 op = CMD_CMD & 0x01FFU;
  if ((CMD_CMD & 0xFF00U) == 0x0600U) FUNC_TIMER();
  else if ((CMD_CMD & 0xFF00U) == 0x0E00U) FUNC_COUNTER();
  else {
    CMD_PULS = (CMD_CMD & 0x1000U) != 0U;
    if ((op >= 2U && op <= 13U) || op == 0x10U || op == 0x12U ||
        (op >= 0x1C2U && op <= 0x1CFU)) {
      CMD_REG = legacy_word();
      if ((op >= 2U && op <= 9U) || (op >= 0x1C2U && op <= 0x1CFU))
        CMD_REG &= 0x3FFFU; /* Preserve address bit 12 as in minimal. */
    }
    CMD_PULS_TEST = CMD_PULS ? PLC_LDP_TEST() : (PLC_ACC_BIT & 1U);
    FUNC_EXT_0[op]();
    if ((op >= 2U && op <= 13U) || op == 0x10U || op == 0x12U ||
        op == 0x14U || (op >= 0x1C2U && op <= 0x1CFU)) PLC_PC++;
  }
}

void FUNC_F(void) {
  FUNC_EXT_F[(CMD_CMD & 0x0F00) >> 8]();
}

void FUNC_FF(void) {
  switch( CMD_CMD & 0x00FF ) {
    case 0xF8: //ANB 
      PLC_ACC_BIT = (PLC_ACC_BIT >> 1) & ( ( PLC_ACC_BIT & 0x01 ) | 0xFE );
      break;
    case 0xF9: //ORB
      PLC_ACC_BIT = (PLC_ACC_BIT >> 1) | (PLC_ACC_BIT & 0x01);
      break;
    case 0xFA: //MPS
      PLC_MPS_BIT = (PLC_MPS_BIT << 1) | (PLC_ACC_BIT & 0x01);
      break;
    case 0xFB: //MRD
      PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (PLC_MPS_BIT & 0x01);
      break;
    case 0xFC: //MPP
      PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (PLC_MPS_BIT & 0x01);
      PLC_MPS_BIT >>= 1;
      break;
    case 0xFD: //INV  
      PLC_ACC_BIT = (PLC_ACC_BIT & 0xFE) | (~PLC_ACC_BIT & 0x01);                 
      break;
    case 0xFF: //POP: original empty case, explicitly incomplete.
      PLC_Legacy_Incomplete();
      break;
    default:
      PLC_PROG_ERROR(M8064,02);
      break;
  }
}
//=======================================================================================================
//=======================================================================================================
// BASIC Command
//=======================================================================================================
//=======================================================================================================
// LD, LDI, AND, ANI, OR, ORI, OUT, SET, RST, PLS, PLF
//=======================================================================================================
//=======================================================================================================
void LD(void) {
  PLC_ACC_BIT <<= 1;
  if ( PLC_STL_Status ) {
    if ( PLC_BIT_TEST(CMD_REG) && PLC_BIT_TEST(PLC_STL_Addr) )
      PLC_ACC_BIT |= 0x01;
  } else {
    if ( PLC_BIT_TEST(CMD_REG) )
      PLC_ACC_BIT |= 0x01;
  }
}

void LDI(void) {
  PLC_ACC_BIT <<= 1;
  if ( PLC_STL_Status ) {
    if ( !(PLC_BIT_TEST(CMD_REG) ) && PLC_BIT_TEST(PLC_STL_Addr) )
      PLC_ACC_BIT |= 0x01;
  } else {
    if ( !PLC_BIT_TEST(CMD_REG) )
      PLC_ACC_BIT |= 0x01;
  }
}

void AND(void) {
  if ( PLC_BIT_TEST(CMD_REG) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}

void ANI(void) {
  if ( !(PLC_BIT_TEST(CMD_REG) ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}

void OR(void) {
  if ( PLC_BIT_TEST(CMD_REG) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}

void ORI(void) {
  if ( !(PLC_BIT_TEST(CMD_REG) ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}

void OUT(void) {
  if ( PLC_STL_CMD ) {
    if ( PLC_STL_Status ) {
      if ( CMD_REG < 0x000A ) { // S000-S009
        if ( PLC_ACC_BIT & 0x01 ) {
          PLC_BIT_OFF(PLC_STL_Addr);
          PLC_BIT_ON(CMD_REG);
        }
      } else {
        if ( PLC_BIT_TEST(PLC_STL_Addr) ) {             
          if ( PLC_ACC_BIT & 0x01 ) { 
            PLC_BIT_ON(CMD_REG);
            if (PLC_STL_Count >= 256U) legacy_fault(PLC_RUNTIME_STACK);
            else PLC_STL_Coil[PLC_STL_Count++] = CMD_REG;
          } else 
            PLC_BIT_OFF(CMD_REG);
        }
      }
    } else {
      if ( CMD_REG < 0x000A ) { // S000-S009
        if ( PLC_ACC_BIT & 0x01 ) {
          PLC_BIT_ON(CMD_REG);
        }
      } else {
        if ( PLC_ACC_BIT & 0x01 )
          PLC_BIT_ON(CMD_REG);
        else 
          PLC_BIT_OFF(CMD_REG);
      }
    }
  } else {
    if ( PLC_ACC_BIT & 0x01 )
      PLC_BIT_ON(CMD_REG);
    else
      PLC_BIT_OFF(CMD_REG);
  }
}

void BSET(void) { 
u16 temp;

  if ( PLC_ACC_BIT & 0x01 ) {
    if ( PLC_STL_Status ) {
      for ( temp = 0; temp < PLC_STL_Count; temp++ )
        PLC_BIT_OFF(PLC_STL_Coil[temp]);
      
      PLC_BIT_OFF(PLC_STL_Addr);
      PLC_STL_Count = 0;
      PLC_BIT_ON(CMD_REG);
    } else
      PLC_BIT_ON(CMD_REG);
  }
}

void RST(void) { 
  if ( PLC_ACC_BIT & 0x01 )
    PLC_BIT_OFF(CMD_REG);
}
//=======================================================================================================
void PLS(void) {
  u8 current = PLC_ACC_BIT & 1U, old = (u8)PLC_PL_BIT_TEST();
  if (current) PLC_PL_BIT_ON(); else PLC_PL_BIT_OFF();
  if (current && !old) PLC_BIT_ON(CMD_REG); else PLC_BIT_OFF(CMD_REG);
}

void PLF(void) {
  u8 current = PLC_ACC_BIT & 1U, old = (u8)PLC_PL_BIT_TEST();
  if (current) PLC_PL_BIT_ON(); else PLC_PL_BIT_OFF();
  if (old && !current) PLC_BIT_ON(CMD_REG); else PLC_BIT_OFF(CMD_REG);
}

void MC(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void MCR(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void RST_TC(void) {
u32 addr;

  addr = CMD_REG & 0xFF;
  switch (CMD_REG >> 8) {
    case 0x86:

      if ( PLC_ACC_BIT & 0x01 ) {
        PLC_BIT_OFF((Addr_TO * 8U) + addr);
        PLC_BIT_OFF((0x0560U * 8U) + addr);
        PLC_16BIT(Addr_TVal + addr * 2) = 0;
      
        PLC_BIT_ON((0x0540U * 8U) + addr);
      } else
        PLC_BIT_OFF((0x0540U * 8U) + addr);
      break;
    case 0x8E:

      if ( PLC_ACC_BIT & 0x01 ) {
        if ( addr >= 200 ) {
          PLC_16BIT(Addr_CVal + addr * 2 + 2) = 0;
          PLC_16BIT(Addr_CVal + addr * 2) = 0;
          PLC_BIT_OFF((Addr_CO * 8U) + addr);             //Pointing to the overflow coil and clearing the coil
        } else {
          PLC_16BIT(Addr_CVal + addr * 2) = 0;
          PLC_BIT_OFF((Addr_CO * 8U) + addr);             //Pointing to the overflow coil and clearing the coil
        }
      }
      CMD_REG = 0X3700 + addr;
      OUT();
      break;
  }
}

void RST_D(void) {  
u32 temp = 0;
u32 addr;

  if ( PLC_ACC_BIT & 0x01 ) {   
    addr = CMD_REG & 0x00FF;
    PLC_PC++;
    addr = ( addr + ( ( legacy_word() & 0x00FF ) << 8) ) * 2;
    temp = legacy_word() >> 8;
//    PLC_PC++;
    if ( temp == 0x86 ) {
      PLC_16BIT(Addr_D8000 + addr) = 0;
    } else if ( addr == 0x88 ) {
      PLC_16BIT(Addr_D0000 + addr) = 0;
    } else if ( addr == 0x80 ) {
      PLC_16BIT(Addr_D1000 + addr) = 0;
    } else
      PLC_PROG_ERROR(M8065,6501);
  } else
    PLC_PC++;
}
//=======================================================================================================
void STL(void) {    // step mode                      
  PLC_STL_CMD = 1;          // Global program enable step mark
  PLC_STL_Status = 1;       // Start step mode                
  PLC_STL_Addr = CMD_REG;  // Record step address            
  PLC_ACC_BIT <<= 1;
  if ( PLC_BIT_TEST(PLC_STL_Addr) )
    PLC_ACC_BIT |= 0x01;
}

void RET(void) {  
  PLC_STL_Status = 0;       //Exit step mode to let the program enter the ladder diagram
}
//=======================================================================================================
void LDP(void) {
  PLC_ACC_BIT <<= 1;
  if ( PLC_PL_BIT_TEST() ) { 
    if ( !( PLC_LD_BIT(CMD_REG) ) )
      PLC_PL_BIT_OFF();         
  } else {
    if ( PLC_STL_Status ) {
      if ( PLC_LD_BIT(CMD_REG) && PLC_BIT_TEST(PLC_STL_Addr) ) {
        PLC_ACC_BIT |= 0x01;
        PLC_PL_BIT_ON();
      }
    } else {
      if ( PLC_LD_BIT(CMD_REG) ) {
        PLC_ACC_BIT |= 0x01;
        PLC_PL_BIT_ON();
      }
    }
  } 
}

void LDF(void) {
  PLC_ACC_BIT <<= 1;
  if ( PLC_PL_BIT_TEST() ) {
    if ( PLC_STL_Status ) {
      if ( ( !( PLC_LD_BIT(CMD_REG) ) ) && PLC_BIT_TEST(PLC_STL_Addr) ) {
        PLC_ACC_BIT |= 0x01;
        PLC_PL_BIT_ON();
      }
    } else { 
      if ( !( PLC_LD_BIT(CMD_REG) ) ) {
        PLC_ACC_BIT |= 0x01;
        PLC_PL_BIT_OFF();
      }
    }             
  } else {  
    if ( PLC_LD_BIT(CMD_REG) )
      PLC_PL_BIT_ON();
  }  
}

void ANDP(void) {
u8 logic;

  logic = 0;
  if ( PLC_PL_BIT_TEST() ) {
    if ( !( PLC_LD_BIT(CMD_REG) ) )
      PLC_PL_BIT_OFF();
  } else {
    if ( PLC_LD_BIT(CMD_REG) ) {
      logic = 1;
      PLC_PL_BIT_ON();
    }
  }
  if ( ( PLC_ACC_BIT & 0x01 ) && ( logic == 1 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}

void ANDF(void) {
u8 logic;

  logic = 0;
  if ( PLC_PL_BIT_TEST() ) {
    if ( !( PLC_LD_BIT(CMD_REG) ) ) {
      logic = 1;
      PLC_PL_BIT_OFF();
    }
  } else {
    if ( PLC_LD_BIT(CMD_REG) )
      PLC_PL_BIT_ON();
  }  
  if ( ( PLC_ACC_BIT & 0x01 ) && ( logic == 1 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
} 

void ORP(void) {
u8 logic;

  logic = 0;
  if ( PLC_PL_BIT_TEST() ) { 
    if ( !( PLC_LD_BIT(CMD_REG) ) )
      PLC_PL_BIT_OFF();
  } else {
    if ( PLC_LD_BIT(CMD_REG) ) {
      logic = 1;
      PLC_PL_BIT_ON();
    }
  }  
  if ( ( PLC_ACC_BIT & 0x01 ) || ( logic == 1 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}

void ORF(void) {
u8 logic;

  logic = 0;
  if ( PLC_PL_BIT_TEST() ) {
    if ( !( PLC_LD_BIT(CMD_REG) ) ) {
      logic = 1;
      PLC_PL_BIT_OFF();
    }
  } else {  
    if ( PLC_LD_BIT(CMD_REG) )
      PLC_PL_BIT_ON();
  }  
  if ( ( PLC_ACC_BIT & 0x01 ) || ( logic == 1 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}
//=======================================================================================================
//=======================================================================================================
// EXECUTING Command
//=======================================================================================================
//=======================================================================================================
// CJ, CALL, SRET, FEND (WDT, IRET, EI, DI, FOR, NEXT)
//=======================================================================================================
//=======================================================================================================
void CJ(void) { 
u32 temp = 0;

  if ( CMD_PULS_TEST ) {
    if ( ( legacy_word() & 0xFF00 ) == 0x8800 ) {
      temp = legacy_word() & 0xFF;
      PLC_PC++;
      if ( ( legacy_word() & 0xFF00 ) == 0x8000 )
        PLC_PC = PLC_P_Addr[temp >> 1];
    }
  } else
    PLC_PC++;
}

void CALL(void) { 
u32 temp, temp1;

  if ( CMD_PULS_TEST ) {
    if ( ( legacy_word() & 0xFF00 ) == 0x8800 ) {
      temp = legacy_word() & 0xFF;
      PLC_PC++;
      if ( ( legacy_word() & 0xFF00 ) == 0x8000 ) {
        for ( temp1 = 62; temp1 > 0; temp1-- ) {
          process[temp1 + 1] = process[temp1];
          p_save[temp1 + 1] = p_save[temp1]; 
        }
        process[0] = PLC_ACC_BIT;
        p_save[0] = PLC_PC;

        PLC_PC = PLC_P_Addr[temp >> 1];
      }
    }
  } else
    PLC_PC++;
}

void SRET(void) { 
u32 temp = 0;

  PLC_ACC_BIT = process[0];
  PLC_PC = p_save[0];
  for ( temp = 62; temp > 0; temp-- ) {
    process[temp] = process[temp + 1];
    p_save[temp] = p_save[temp + 1]; 
  }      
}

void IRET(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void EI(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void DI(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void WDT(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void FOR(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void NEXT(void) { 
  PLC_PROG_ERROR(M8065,02);
}
//=======================================================================================================//
//=======================================================================================================//
// COMPARE and MOVE Command                                                                              //
//=======================================================================================================//
//=======================================================================================================//
// CMP, ZCP, MOV, SMOV, CML, BMOV, FMOV, XCH, BCD, BIN                                                   //
// 10 CMP  0x0024  7 0 0 0
// 11 ZCP  0x0026  7 0 0 0
// 12 MOV  0x0028  5 0 0 0
// 13 SMOV 0x002A 11 - - 0
// 14 CML  0x002C  5 - - 0
// 15 BMOV 0x002E  7 0 0 0
// 16 FMOV 0x0030  7 - - 0
// 17 XCH  0x0032  5 - - 0
// 18 BCD  0x0034  5 0 0 0
// 19 BIN  0x0036  5 0 0 0
//=======================================================================================================//
//=======================================================================================================//
#ifdef USE_COMPARE_MOVE

void CMP(void) {
signed int temp1, temp2, temp3;

  if ( CMD_PULS_TEST ) {
    Flag_bit = 0XFF;

    temp1 = get_value();
    temp2 = get_value();
    temp3 = get_addr();

    PLC_BIT_OFF(temp3);
    PLC_BIT_OFF(temp3 + 1);
    PLC_BIT_OFF(temp3 + 2);

    if ( temp1 > temp2 )       PLC_BIT_ON(temp3); 
    else if ( temp1 == temp2 ) PLC_BIT_ON(temp3 + 1);
    else if ( temp1 < temp2 )  PLC_BIT_ON(temp3 + 2);    
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DCMP(void) {
signed int temp3;
int temp1,temp2;

  if ( CMD_PULS_TEST ) {
    Flag_bit = 0XFF;

    temp1 = get_valueD();
    temp2 = get_valueD();
    temp3 = get_addr();

    PLC_BIT_OFF(temp3);
    PLC_BIT_OFF(temp3 + 1);
    PLC_BIT_OFF(temp3 + 2);

    if ( temp1 > temp2 )       PLC_BIT_ON(temp3); 
    else if ( temp1 == temp2 ) PLC_BIT_ON(temp3 + 1);
    else if ( temp1 < temp2 )  PLC_BIT_ON(temp3 + 2); 
  } else
    PLC_PC += 10;
}
#else
void DCMP(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void ZCP(void) {
signed int temp1,temp2,temp3,temp4;

  if ( CMD_PULS_TEST ) {
    Flag_bit = 0XFF;

    temp1 = get_value();
    temp2 = get_value();
    temp3 = get_value();
    temp4 = get_addr();

    PLC_BIT_OFF(temp4);
    PLC_BIT_OFF(temp4 + 1);
    PLC_BIT_OFF(temp4 + 2);

    if ( temp1 > temp3 )
      PLC_BIT_ON(temp4); 
    else if ( ( temp1 <= temp3 ) && ( temp3 <= temp2 ) )
      PLC_BIT_ON(temp4 + 1);
    else if ( temp2 < temp3 )
      PLC_BIT_ON(temp4 + 2); 
  } else
    PLC_PC += 8;
}

#ifdef USE_DOUBLE
void DZCP(void) {
s32 temp1,temp2,temp3,temp4;

  if ( CMD_PULS_TEST ) {
    Flag_bit = 0XFF;

    temp1 = get_valueD();
    temp2 = get_valueD();
    temp3 = get_valueD();
    temp4 = get_addr();

    PLC_BIT_OFF(temp4);
    PLC_BIT_OFF(temp4 + 1);
    PLC_BIT_OFF(temp4 + 2);

    if ( temp1 > temp3 )
      PLC_BIT_ON(temp4); 
    else if ( ( temp1 <= temp3 ) && ( temp3 <= temp2 ) )
      PLC_BIT_ON(temp4 + 1);
    else if ( temp2 < temp3 )
      PLC_BIT_ON(temp4 + 2); 
  } else
    PLC_PC += 14;
}
#else
void DZCP(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void MOV(void) {
  if ( CMD_PULS_TEST ) {
    value = get_value();
    put_value();  
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DMOV(void) {
  if ( CMD_PULS_TEST ) {
    value = get_valueD();
    put_valueD(); 
  } else   
    PLC_PC += 8;
}
#else
void DMOV(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

u16 bcd[4]={0x1,0x10,0x100,0x1000};
void SMOV(void) {
u32 temp1,temp2,temp3,temp4,temp5,temp6;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value();
    temp2 = get_value();
    temp3 = get_value();
    temp4 = get_addr();
    temp5 = get_value();
    temp1 %= bcd[temp2];
    for ( temp6 = 0; temp6 < temp3; temp6++ ) { 
      temp2--;
      temp5--;
      PLC_16BIT(temp4) |= (temp1 / bcd[temp2]) * bcd[temp5];
      if ( ( temp2 == 1 ) && ( temp5 == 1 ) ) {
        PLC_16BIT(temp4) |= temp1 % 0x10 * bcd[temp5];
        temp6 = temp3 + 1;
      } else
        temp1 %= bcd[temp2];
    }
  } else
    PLC_PC += 10;
}

void CML(void) {

  if ( CMD_PULS_TEST ) {
    value = ~get_value();
    put_value();
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DCML(void) {

  if ( CMD_PULS_TEST ) {
    value = ~get_valueD();
    put_valueD();
  } else
    PLC_PC += 8;
}
#else
void DCML(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void BMOV(void) {    
signed int temp,i;

  if ( CMD_PULS_TEST ) {
    value = get_value();
    put_value();
    temp = get_value();
    PLC_PC -= 2; // Adjust the program counter by two words
    for ( i = 1; i < temp; i++ ) {
      if ( bitT_get == 1 )
        Transfer= i * 16;
      else
        Transfer = i * 2;
      PLC_PC -= 4;
      value = get_value();
      if ( bitT_put == 1 )
        Transfer = i * 16;
      else
        Transfer = i * 2;
      put_value();
    }
    PLC_PC += 2; // Adjust the program counter by two words
    Transfer = 0;
  } else
    PLC_PC += 6;
}

void FMOV(void) {
signed int temp,i;

  if ( CMD_PULS_TEST ) {
    value = get_value();
    put_value();
    temp = get_value();
    PLC_PC -= 2; // Adjust the program counter by two words
    for ( i = 1; i < temp; i++ ) {
      if ( bitT_put == 1 )
        Transfer = i * 16;
      else
        Transfer = i * 2;
      PLC_PC -= 2;
      put_value();
    }
    PLC_PC += 2; // Adjust the program counter by two words
    Transfer = 0;
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DFMOV(void) {
signed int temp,i;

  if ( CMD_PULS_TEST ) {
    value = get_valueD();
    put_valueD();
    temp = get_valueD();
    PLC_PC -= 4; // Adjust the program counter by two words
    for( i = 1; i < temp; i++ ) {
      if ( bitT_put == 1)
        Transfer = i * 32;
      else
        Transfer = i * 4;
      PLC_PC -= 4;
      put_valueD();
    }
    PLC_PC += 2; // Adjust the program counter by two words
    Transfer = 0;
  } else
    PLC_PC += 12;
}
#else
void DFMOV(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void XCH(void) {    
signed int temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value();
    temp2 = get_value();
    PLC_PC -= 4;
    value = temp2;
    put_value();
    value = temp1;
    put_value();
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DXCH(void) {    
signed int temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD();
    temp2 = get_valueD();
    PLC_PC -= 8;
    value = temp2;
    put_valueD();
    value = temp1;
    put_valueD();
  } else
    PLC_PC += 8;
}
#else
void DXCH(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void BCD(void) {
signed int temp1, temp2;

  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    temp1 = get_value();

    value = temp1 % 10;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 4;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 8;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 12;
    put_value();
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DBCD(void) {
signed int temp1, temp2;

  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    temp1 = get_valueD();

    value = temp1 % 10;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 4;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 8;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 12;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 16;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 20;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 24;
    temp1 /= 10;
    temp2 = temp1 % 10;
    value += temp2 << 28;
    put_valueD();
  } else
    PLC_PC += 8;
}
#else
void DBCD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void BIN(void) {
signed int temp1, temp2;

  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    temp1 = get_value();

    value = temp1 & 0x0F;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 10;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 100;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 1000;
    put_value();
  } else
    PLC_PC += 4;
}
 
#ifdef USE_DOUBLE
void DBIN(void) {
signed int temp1, temp2;

  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    temp1 = get_valueD();

    value = temp1 & 0x0F;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 10;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 100;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 1000;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 10000;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 100000;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 1000000;
    temp1 >>= 4;
    temp2 = temp1 & 0x0F;
    value += temp2 * 10000000;
    put_valueD();
  } else
    PLC_PC += 8;
}
#else
void DBIN(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

#endif // USE_COMPARE_MOVE
//=======================================================================================================
//=======================================================================================================
// ARIFMETIC and LOGIC Command
//=======================================================================================================
//=======================================================================================================
//
// 20 ADD  0x0038  7 0 0 0
// 21 SUB  0x003A  7 0 0 0
// 22 MUL  0x003C  7 0 0 0
// 23 DIV  0x003E  7 0 0 0
// 24 INC  0x0040  3 0 0 0
// 25 DEC  0x0042  3 0 0 0
// 26 WAND 0x0044  7 0 0 0
// 27 WOR  0x0046  7 0 0 0
// 28 WXOR 0x0048  7 0 0 0
// 29 NEG  0x004A  3 - - 0
//=======================================================================================================
//=======================================================================================================
#ifdef USE_ARIPH

void ADD(void) { 
  if ( CMD_PULS_TEST ) {
    value = get_value() + get_value();
    put_value();
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DADD(void) {
  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    value = get_valueD() + get_valueD();
    put_valueD();
  } else
    PLC_PC += 12;
}
#else
void DADD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void SUB(void) {
  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    value = get_value() - get_value();
    put_value();
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DSUB(void) {
  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    value = get_valueD() - get_valueD();
    put_valueD();
  } else
    PLC_PC += 12;
}
#else
void DSUB(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

#ifdef USE_MULDIV
void MUL(void) {
signed int temp1,temp2;u32 temp3;

  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    temp1 = get_value();
    temp2 = get_value();
    temp3 = addr_value_prog(); 
    PLC_Ws(temp3) = temp1 * temp2;
  } else
    PLC_PC += 6;
}
 
#ifdef USE_DOUBLE
void DMUL(void) {   
signed int temp;

  if ( CMD_PULS_TEST ) {
    u64d.data = (int64_t)(get_valueD() * get_valueD());
    temp = get_addr();
//    PLC_PC += 2;               
    PLC_16BIT(temp) = u64d.data1[0];
    PLC_16BIT(temp + 2) = u64d.data1[1]; 
    PLC_16BIT(temp + 4) = u64d.data1[2]; 
    PLC_16BIT(temp + 6) = u64d.data1[3];  
  } else
    PLC_PC += 12;
}
#else
void DMUL(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void DIV(void) {
signed int temp1,temp2,temp3;

  if ( CMD_PULS_TEST ) {
    PLC_Err = PLC_PC;
    temp1 = get_value();
    temp2 = get_value();
    temp3 = get_addr();
    PLC_16BIT(temp3) = temp1 / temp2;
    PLC_16BIT(temp3 + 2) = temp1 % temp2;
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DDIV(void) {
signed int temp1,temp2,temp3;
s32_union tempD = {0};

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD();             
    temp2 = get_valueD();    

    temp3 = get_addr();
//    PLC_PC += 2;                         

    tempD.d32 = temp1 / temp2;
    PLC_16BIT(temp3) = tempD.d16[0];
    PLC_16BIT(temp3 + 2) = tempD.d16[1]; 

    tempD.d32 = temp1 % temp2;
    PLC_16BIT(temp3 + 4) = tempD.d16[0]; 
    PLC_16BIT(temp3 + 6) = tempD.d16[1];  
  } else
    PLC_PC += 12;
}
#else
void DDIV(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

#endif // USE_MULDIV

void INC(void) {
  if ( CMD_PULS_TEST ) {
    value = get_value() + 1;
    PLC_PC -= 2;
    put_value();
  } else
    PLC_PC += 2;
}

#ifdef USE_DOUBLE
void DINC(void) {
  if ( CMD_PULS_TEST ) {
    value = (u32)get_valueD() + 1;
    PLC_PC -= 4;
    put_valueD();
  } else
    PLC_PC += 4;
}
#else
void DINC(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void DEC(void) {
  if ( CMD_PULS_TEST ) {
    value = get_value() - 1;
    PLC_PC -= 2;
    put_value();
  } else
    PLC_PC += 2;
}

#ifdef USE_DOUBLE
void DDEC(void) {
  if ( CMD_PULS_TEST ) {
    value = get_valueD() - 1;
    PLC_PC -= 4;
    put_valueD();
  } else
    PLC_PC += 4;
}
#else
void DDEC(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void WAND(void) {
signed int temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value();
    temp2 = get_value();
    value = temp1 & temp2;
    put_value();
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DWAND(void) {
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD();
    temp2 = get_valueD();;
    value = temp1 & temp2;
    put_valueD();
  } else
    PLC_PC += 12;
}
#else
void DWAND(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void WOR(void) {
signed int temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value();
    temp2 = get_value();
    value = temp1 | temp2;
    put_value();
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DWOR(void) {
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD();
    temp2 = get_valueD();;
    value = temp1 | temp2;
    put_valueD();
  } else
    PLC_PC += 12;
}
#else
void DWOR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void WXOR(void) {
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value();
    temp2 = get_value();
    value = temp1 ^ temp2;
    put_value();
  } else
    PLC_PC += 6;
}

#ifdef USE_DOUBLE
void DWXOR(void) {
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD();
    temp2 = get_valueD();
    value = temp1 ^ temp2;
    put_valueD();
  } else
    PLC_PC+=12;
}
#else
void DWXOR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void NEG(void) {
  if ( CMD_PULS_TEST ) {
    value = 0 - get_value();
    PLC_PC -= 2;
    put_value();
  } else
    PLC_PC += 2;
}

#ifdef USE_DOUBLE
void DNEG(void) {
  if ( CMD_PULS_TEST ) {
    value = 0 - get_valueD();
    PLC_PC -= 4;
    put_valueD();     
  } else
    PLC_PC += 4;
}
#else
void DNEG(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

#endif // USE_ARIPH
//=======================================================================================================
//=======================================================================================================
// ROTATE
//=======================================================================================================
//=======================================================================================================
//
// 30 ROR  0x004C  5 - - 0
// 31 ROL  0x004E  5 - - 0
// 32 RCR  0x0050  5 - - 0
// 33 RCL  0x0052  5 - - 0
// 34 SFTR 0x0054  7 0 0 0
// 35 SFTL 0x0056  7 0 0 0
// 36 WSFR 0x0058  9 - - 0
// 37 WSFL 0x005A  9 - - 0
// 38 SFWR 0x005C  7 0 0 0
// 39 SFRD 0x005E  7 0 0 0
//=======================================================================================================
//=======================================================================================================
#ifdef USE_ROTATE

void RCR(void) {    
u16 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value();
    temp2 = get_value();
    value = temp1 >> temp2;
    PLC_PC -= 2;
    put_value();
    PLC_PC += 1;
  } else
    PLC_PC += 2;
}

#ifdef USE_DOUBLE
void DRCR(void) {    
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD(); 
    temp2 = get_valueD(); 
    value = temp1 >> temp2;
    PLC_PC -= 4;
    put_valueD();  
    PLC_PC += 2;
  } else
    PLC_PC += 4;
}
#else
void DRCR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void RCL(void) {    
u16 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value();
    temp2 = get_value();
    value = temp1 << temp2;
    PLC_PC -= 2;
    put_value();
    PLC_PC += 1;
  } else
    PLC_PC += 2;
}

#ifdef USE_DOUBLE
void DRCL(void) {    
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD();
    temp2 = get_valueD();
    value = temp1 << temp2;
    PLC_PC -= 4;
    put_valueD();
    PLC_PC += 2;
  } else
    PLC_PC += 4;
}
#else
void DRCL(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void ROR(void) {    
u16 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value(); 
    temp2 = get_value(); 
    value = ROTATE_RIGHT(temp1,16,temp2);
    PLC_PC -= 2;
    put_value();  
    PLC_PC += 1;
  } else
    PLC_PC += 2;
}

#ifdef USE_DOUBLE
void DROR(void) {    
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD(); 
    temp2 = get_valueD(); 
    value = ROTATE_RIGHT(temp1,32,temp2);
    PLC_PC -= 4;
    put_valueD();  
    PLC_PC += 2;
  } else
    PLC_PC += 4;
}
#else
void DROR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void ROL(void) {    
u16 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_value(); 
    temp2 = get_value(); 
    value = ROTATE_LEFT(temp1,16,temp2);
    PLC_PC -= 2;
    put_value();
    PLC_PC += 1;
  } else
    PLC_PC += 2;
}

#ifdef USE_DOUBLE
void DROL(void) {    
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_valueD(); 
    temp2 = get_valueD();  
    value = ROTATE_LEFT(temp1,32,temp2);
    PLC_PC -= 4;
    put_valueD();  
    PLC_PC += 2;
  } else
    PLC_PC += 4;
}
#else
void DROL(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void SFTR(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void SFTL(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void WSFR(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void WSFL(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void SFWR(void) { 
  PLC_PROG_ERROR(M8065,02);
}

void SFRD(void) { 
  PLC_PROG_ERROR(M8065,02);
}
#endif // USE_ROTATE
//=======================================================================================================
//=======================================================================================================
// DATA 1
//=======================================================================================================
//=======================================================================================================
//
// 40 ZRST 0x0060  5 0 0 0
// 41 DECO 0x0062  7 0 0 0
// 42 ENCO 0x0064  7 0 0 0
// 43 SUM  0x0066  5 - - 0
// 44 BON  0x0068  7 - - 0
// 45 MEAN 0x006A  7 - - 0
// 46 ANS  0x006C  7 - - 0
// 47 ANR  0x006E  1 - - 0
// 48 SQR  0x0070  5 - - 0
// 49 FLT  0x0072  5 - - 0
//=======================================================================================================
//=======================================================================================================
void ZRST(void) { 
u32 temp1,temp2;

  if ( CMD_PULS_TEST ) {
    temp1 = get_addr();
    temp2 = get_addr();
    if ( Flag_bit == 0x00 ) {
      for ( ; temp1 <= temp2; temp1++ )
        PLC_BIT_OFF(temp1);
    } else {     
      for ( ; temp1 <= temp2; temp1 += 2 )
        PLC_16BIT(temp1) = 0;
    }
  } else
    PLC_PC += 4;
}

void DECO(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ENCO(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void SUM(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void BON(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void MEAN(void) {
u32 temp, temp1, temp2;
uint64_t data;

  if ( CMD_PULS_TEST ) {
    temp1 = addr_value_prog();             
    PLC_PC += 2;
    temp2 = get_value();
    if ( Flag_bit == 0xFF ) {
      for ( temp = 0; temp < temp2; temp++ ) {
        data += PLC_Hs(temp1 + temp * 2);
      }
      PLC_PC -= 4;
    } else {  
      data = (u16)temp1;
      PLC_PC -= 4;
      for ( temp = 1; temp < temp2; temp++ ) { 
        PLC_PC -= 2;
        Transfer = temp * 16;
        data += addr_value_prog();
      }
      Flag_bit = 0xFF;
    }
    value = data / temp2;
    put_value();
    PLC_PC += 2;        
  } else
    PLC_PC += 6;
}

void ANS(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ANR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

#ifdef USE_FLOAT

#ifdef USE_MULDIV
void SQR(void) {
  if ( CMD_PULS_TEST ) {
    value = (u16)sqrt((double)get_value());
    put_value();  
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DSQR(void) {
  if ( CMD_PULS_TEST ) {
    value = (u32)sqrt((double)get_valueD());
    put_value();
  } else
    PLC_PC += 8;
}
#else
void DSQR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

#else
void SQR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void DSQR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void FLT(void) {    
  if ( CMD_PULS_TEST ) {
    valueF = (float)get_value();      
    put_valueF();
    PLC_PC -= 2; 
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DFLT(void) {
  if ( CMD_PULS_TEST ) {
    valueF = (float)get_valueD();          
    put_valueF();
  } else
    PLC_PC += 8;
}
#else
void DFLT(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

#endif
//=======================================================================================================
//=======================================================================================================
// 
//=======================================================================================================
//=======================================================================================================
//
// 50 REF  0x0074  5 0 0 0
// 51 REFF 0x0076  3 - - 0
// 52 MTR  0x0078  9 0 0 0
// 53 HSCS 0x007A 13 0 0 0
// 54 HSCR 0x007C 13 0 0 0
// 55 HSZ  0x007E 17 - - 0
// 56 SPD  0x0080  7 0 0 0
// 57 PLSY 0x0082  7 0 0 0
// 58 PWM  0x0084  7 0 0 0
// 59 PLSR 0x0086  7 0 0 0
//=======================================================================================================
//=======================================================================================================
void REF(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void REFF(void) {
  if ( CMD_PULS_TEST ) {
    X_DIY = get_value();
  } else {
    X_DIY = 10;
    PLC_PC += 2;
  }
}

void MTR(void) {
u16 X,Y,M_Y,K_H,temp=0;
u16 i,t;   

  if ( PLC_ACC_BIT & 0x01 ) {
    X = get_addr();
    Y = get_addr();
    M_Y = get_addr();
    K_H = get_value();
    for ( i = 0; i < K_H; i++ ) {
      temp = i * 8;
      PLC_BIT_ON(Y + i);
      for ( t = 0; t <= 7; t++ )
        (PLC_BIT_TEST(X + t)) ? PLC_BIT_ON(M_Y + temp + t) : PLC_BIT_OFF(M_Y + temp + t);
    }
  } else
    PLC_PC += 8;
}

#ifdef USE_DOUBLE
void DHSCS(void) {
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp2 == temp1 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}

void DHSCR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void DHSZ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void SPD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void PLSY(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void PWM(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void PLSR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
//=======================================================================================================
//=======================================================================================================
//
//=======================================================================================================
//=======================================================================================================
//
// 60 IST  0x0088  7 0 0 0
// 61 SER  0x008A  7 - - 0
// 62 ABSD 0x008C  9 0 0 0
// 63 INCD 0x008E  9 0 0 0
// 64 TTMR 0x0090  5 0 0 0
// 65 STMR 0x0092  7 - - 0
// 66 ALT  0x0094  3 0 0 0
// 67 RAMP 0x0096  9 0 0 0
// 68 ROTC 0x0098  9 - - 0
// 69 SORT 0x009A 17 - - 0
// 70 TKY  0x009C  7 - - 0
// 71 HKY  0x009E  9 - - 0
// 72 DSW  0x00A0  9 0 0 0
// 73 SEGD 0x00A2  5 - - 0
// 74 SEGL 0x00A4  7 0 0 0
// 75 ARWS 0x00A6  9 - - 0
// 76 ASC  0x00A8 11 - - 0
// 77 PR   0x00AA  5 - - 0
// 78 FROM 0x00AC  9 - 0 0
// 79 TO   0x00AE  9 - 0 0
// 80 RS   0x00B0  9 0 0 0
// 81 PRUN 0x00B2  5 0 0 0
// 82 ASCI 0x00B4  7 0 0 0
// 83 HEX  0x00B6  7 0 0 0
// 84 CCD  0x00B8  7 0 0 0
// 85 VRRD 0x00BA  5 0 0 0
// 86 VRSC 0x00BC  5 0 0 0
// 88 PID  0x00C0  9 0 0 0
//=======================================================================================================
//=======================================================================================================
void IST(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void SER(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ABSD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void INCD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void TTMR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void STMR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ALT(void) {    
u32 temp = 0;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_addr(); 
    if ( PLC_BIT_TEST(temp) )
      PLC_BIT_OFF(temp);
    else
      PLC_BIT_ON(temp);
  } else
    PLC_PC++;
}

void RAMP(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ROTC(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void SORT(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void TKY(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void HKY(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void DSW(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void SEGD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void SEGL(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ARWS(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ASC(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void PR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void FROM(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void TO(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void RS(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void PRUN(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void ASCI(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void HEX(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void CCD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void VRRD(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void VRSC(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}

void PID(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
//=======================================================================================================
//=======================================================================================================
// FLOAT Command
//=======================================================================================================
//=======================================================================================================
//
// 110 ECMP 0x00EC 13 - - 0
// 111 EZCP 0x00EE 17 - - 0
// 118 EBCD 0x00FC  9 - - 0
// 119 EBIN 0x00FE  9 - - 0
// 120 EADD 0x0100 13 - - 0
// 121 ESUB 0x0102 13 - - 0
// 122 EMUL 0x0104 13 - - 0
// 123 EDIV 0x0108 13 - - 0
// 127 ESQR 0x010E  9 - - 0
// 129 INT  0x0112  5 - - 0
// 130 SIN  0x0114  9 - - 0
// 131 COS  0x0116  9 - - 0
// 132 TAN  0x0118  9 - - 0
//=======================================================================================================
//=======================================================================================================
#ifdef USE_FLOAT
void DECMP(void) {
signed int temp3;
float temp1,temp2;

  if ( PLC_ACC_BIT & 0x01 ) {  
    Flag_bit = 0XFF;

    temp1 = get_valueF();
    temp2 = get_valueF();
    temp3 = get_addr();

    PLC_BIT_OFF(temp3);
    PLC_BIT_OFF(temp3 + 1);
    PLC_BIT_OFF(temp3 + 2);

    if      ( temp1 > temp2 )  PLC_BIT_ON(temp3); 
    else if ( temp1 == temp2 ) PLC_BIT_ON(temp3 + 1);
    else if ( temp1 < temp2 )  PLC_BIT_ON(temp3 + 2); 
  } else
    PLC_PC += 9;
}

void DEZCP(void) {
float temp1,temp2,temp3;
u32 temp4;

  if ( PLC_ACC_BIT & 0x01 ) {
    Flag_bit = 0XFF;

    temp1 = get_valueF();
    temp2 = get_valueF();
    temp3 = get_valueF();
    temp4 = get_addr();

    PLC_BIT_OFF(temp4);
    PLC_BIT_OFF(temp4 + 1);
    PLC_BIT_OFF(temp4 + 2);

    if      ( temp1 > temp3 )                            PLC_BIT_ON(temp4); 
    else if ( ( temp1 <= temp3 ) && ( temp3 <= temp2 ) ) PLC_BIT_ON(temp4 + 1);
    else if ( temp2 < temp3 )                            PLC_BIT_ON(temp4 + 2); 
  } else
    PLC_PC += 13;
}

void DEMOV(void) {
  if ( PLC_ACC_BIT &0x01 ) {
    valueF = get_valueF();
    put_valueF(); 
  } else   
    PLC_PC += 6;
}

void DEADD(void) {  
  if ( PLC_ACC_BIT & 0x01 ) {
    valueF = get_valueF() + get_valueF();             
    put_valueF();
  } else
    PLC_PC += 12;
}

void DESUB(void) {
float temp1,temp2;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp1 = get_valueF();              
    temp2 = get_valueF(); 
    valueF = temp1 - temp2;          
    put_valueF();
  } else
    PLC_PC += 12;
}

void DEMUL(void) {
  if ( PLC_ACC_BIT & 0x01 ) {
    valueF = get_valueF() * get_valueF();          
    put_valueF();
  } else
    PLC_PC+=12;
}

void DEDIV(void) {
float temp1,temp2;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp1 = get_valueF();              
    temp2 = get_valueF(); 
    valueF = temp1 / temp2;          
    put_valueF();
  } else
    PLC_PC += 12;
}

void DESQR(void) {
  if ( PLC_ACC_BIT & 0x01 ) {
    valueF = (float)sqrt((double)get_valueF());
    put_valueF();
  } else
    PLC_PC += 8;
}

void INT(void) {
  if ( PLC_ACC_BIT & 0x01 ) {
    value = (u16)get_valueF();
    PLC_PC -= 2; 
    put_value();
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DINT(void) {
  if ( PLC_ACC_BIT & 0x01 ) {
    value = (u32)get_valueF();
    put_valueD();
  } else
    PLC_PC += 8;
}
#endif

void DTAN(void) {
  if ( PLC_ACC_BIT & 0x01 ) {
    valueF = (float)tan((double)get_valueF());
    put_valueF();
  } else
    PLC_PC += 8;
}

void DCOS(void) {
  if ( PLC_ACC_BIT & 0x01 ) {
    valueF = (float)cos((double)get_valueF());
    put_valueF();
  } else
    PLC_PC += 8;
}

void DSIN(void) {
  if ( PLC_ACC_BIT & 0x01 ) {
    valueF = (float)sin((double)get_valueF());
    put_valueF();
  } else
    PLC_PC += 8;
}

#endif
//=======================================================================================================
//=======================================================================================================
//
//=======================================================================================================
//=======================================================================================================
//
// 147 SWAP 0x0136  3 - - 0
// 155 ABS  0x0146 13 0 0 -
// 156 ZRN  0x0148  9 0 0 -
// 157 PLSV 0x014A  9 0 0 -
// 158 DRVI 0x014C  9 0 0 -
// 159 DRVA 0x014E  9 0 0 -
//=======================================================================================================
//=======================================================================================================
void SWAP(void) {    
u32 temp = 0;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_value();    
    value = (temp >> 8) | (temp << 8);
    PLC_PC -= 2;
    put_value();     
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DSWAP(void) {    
u32 temp = 0;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_valueD();
    value = (temp >> 16) | (temp << 16);
    PLC_PC -= 4;
    put_valueD();
  }
}
#else
void DSWAP(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif
//=======================================================================================================
//=======================================================================================================
// TIME Command
//=======================================================================================================
//=======================================================================================================
//
// 160 TCMP 0x0150 11 0 0 0
// 161 TZCP 0x0152  9 0 0 0
// 162 TADD 0x0154  7 0 0 0
// 163 TSUB 0x0156  7 0 0 0
// 166 TRD  0x015C  3 0 0 0
// 167 TWR  0x015E  3 0 0 0
// 169 HOUR 0x0162  7 0 0 -
//=======================================================================================================
//=======================================================================================================
#ifdef USE_TIME
void TCMP(void) {   
u32 h,m,s,temp,h2,m2,s2;

  if ( PLC_ACC_BIT & 0x01 ) {  
    Flag_bit = 0XFF;

    h = get_value();
    m = get_value();
    s = get_value();

    temp = get_addr();
    h2 = PLC_16BIT(temp);
    m2 = PLC_16BIT(temp + 2);
    s2 = PLC_16BIT(temp + 4);

    temp = get_addr();
    PLC_BIT_OFF(temp);
    PLC_BIT_OFF(temp + 1);
    PLC_BIT_OFF(temp + 2);  

    if ( ( h >= h2 ) && ( m >= m2 ) && ( s > s2 ) )
      PLC_BIT_ON(temp);
    else if ( ( h == h2 ) && ( m == m2 ) && ( s == s2 ) )
      PLC_BIT_ON(temp + 1);
    else if ( ( h <= h2 ) && ( m <= m2 ) && ( s < s2 ) )
      PLC_BIT_ON(temp + 2);
  } else
    PLC_PC += 5;
}

void TZCP(void) {
u32 h,m,s, h1,m1,s1, h2,m2,s2;
u32 temp1,temp;

  if ( PLC_ACC_BIT & 0x01 ) {  
    Flag_bit = 0XFF;

    temp = get_addr();
    h = PLC_16BIT(temp);
    m = PLC_16BIT(temp + 2);
    s = PLC_16BIT(temp + 4);

    temp = get_addr();
    h1 = PLC_16BIT(temp);
    m1 = PLC_16BIT(temp + 2);
    s1 = PLC_16BIT(temp + 4);

    temp = get_addr();
    h2 = PLC_16BIT(temp);
    m2 = PLC_16BIT(temp + 2);
    s2 = PLC_16BIT(temp + 4);

    temp1 = get_addr();
    PLC_BIT_OFF(temp1);
    PLC_BIT_OFF(temp1 + 1);
    PLC_BIT_OFF(temp1 + 2);

    if ( ( h >= h2 ) && ( m >= m2 ) && ( s > s2 ) ) {
      PLC_BIT_ON(temp1);
    }
    else if ( ( ( h <= h2 ) && ( m <= m2 ) && ( s <= s2 ) ) && ( ( h1 >= h2 ) && ( m1 >= m2 ) && ( s1 >= s2 ) ) ) {
      PLC_BIT_ON(temp1 + 1);
    }
    else if ( ( h1 <= h2 ) && ( m1 <= m2 ) && ( s1 < s2 ) ) {
      PLC_BIT_ON(temp1 + 2);
    }
  } else
    PLC_PC += 4;
}

void TADD(void) {  
u32 temp,temp1,temp2;
u16 left, right;

  if ( PLC_ACC_BIT & 0x01 ) { 
    temp1 = get_addr();              
    temp2 = get_addr();  
    temp  = get_addr();
    left = PLC_16BIT(temp1);
    right = PLC_16BIT(temp2);
    PLC_16BIT(temp) = left + right;
    left = PLC_16BIT(temp1 + 2);
    right = PLC_16BIT(temp2 + 2);
    PLC_16BIT(temp + 2) = left + right;
    left = PLC_16BIT(temp1 + 4);
    right = PLC_16BIT(temp2 + 4);
    PLC_16BIT(temp + 4) = left + right;
  } else
    PLC_PC += 3;
}

void TSUB(void) {
u32 temp,temp1,temp2;
u16 left, right;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp1 = get_addr();
    temp2 = get_addr();
    temp  = get_addr();
    left = PLC_16BIT(temp1);
    right = PLC_16BIT(temp2);
    PLC_16BIT(temp) = left - right;
    left = PLC_16BIT(temp1 + 2);
    right = PLC_16BIT(temp2 + 2);
    PLC_16BIT(temp + 2) = left - right;
    left = PLC_16BIT(temp1 + 4);
    right = PLC_16BIT(temp2 + 4);
    PLC_16BIT(temp + 4) = left - right;
  } else
    PLC_PC += 3;
}

void TRD(void) {
u32 temp = 0;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_addr();
    PLC_16BIT(temp)      = D8018;
    PLC_16BIT(temp + 2)  = D8017;
    PLC_16BIT(temp + 4)  = D8016;
    PLC_16BIT(temp + 6)  = D8015;
    PLC_16BIT(temp + 8)  = D8014;
    PLC_16BIT(temp + 10) = D8013;
    PLC_16BIT(temp + 12) = D8019;
  } else
    PLC_PC++;
}

void TWR(void) {
u32 temp = 0;
u16 year, month, day, hour, minute, second;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_addr();
    year = PLC_16BIT(temp);
    month = PLC_16BIT(temp + 2);
    day = PLC_16BIT(temp + 4);
    hour = PLC_16BIT(temp + 6);
    minute = PLC_16BIT(temp + 8);
    second = PLC_16BIT(temp + 10);
    RTC_Set(year, month, day, hour, minute, second);
  } else
    PLC_PC++;
}

void HOUR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif
//=======================================================================================================
//=======================================================================================================
//
//=======================================================================================================
//=======================================================================================================

// 170 GRY  0x0164  5 - - 0
// 171 GBIN 0x0166  5 - - 0
// 176 RD3A 0x0170  7 - 0 -
// 177 WR3A 0x0172  7 - 0 -

unsigned int b2g(unsigned int lStart,unsigned int lEnd,unsigned int lValue) {
unsigned int Start = lStart, End = lEnd, Temp = 0, Counter = 0;
unsigned char Type = 1;

  while(Start<End) {
    Temp = (End + Start - 1) >> 1;
    if ( lValue <= Temp ) {
      if ( !Type )
        Counter += ( (End - Start + 1) >> 1 );
      End = Temp;
      Type = 1;
    } else {
      if ( Type )
        Counter += ( (End - Start + 1) >> 1 );
      Start = ++Temp;
      Type = 0;
    }
  }
  return Counter;
}

/**
 * Convert a Gray-code value to binary within the specified range.
 * @param lStart Lower bound of the conversion range.
 * @param lEnd Upper bound of the conversion range.
 * @param lValue Gray-code value to convert.
 * @return Decoded binary value.
 * @see b2g() Binary-to-Gray conversion over a specified range.
 * @see BtoG() Binary-to-Gray conversion wrapper.
 * @see GtoB() Gray-to-binary conversion wrapper.
 * @usage a=b2g(0,15,6); // Convert binary 6 to Gray code; a is 5.
 * @note Conversion range: [lStart,lEnd]. The wrappers use lStart = 0
 *       and lEnd = 2^N - 1. Other original notes were unreadable.
 *       The original example claimed a result of 4; the code yields 5.
 *       An original 105%-140% comparison could not be recovered.
 */

unsigned int g2b(unsigned int lStart,unsigned int lEnd,unsigned int lValue) {
unsigned int Start = lStart, End = lEnd, Counter = 0, Temp = 0;
unsigned char Type = 1;

  while ( Start < End ) {
    Temp = Counter + ( (End - Start + 1) >>1 );
    if ( Type^( lValue < Temp ) ) { 
      if ( Type )
        Counter = Temp;
      Start = (Start + End + 1) >> 1;
      Type = 0;
    } else {
      if ( !Type )
        Counter=Temp;
      End = ( Start + End - 1 ) >> 1;
      Type = 1;
    }
  }
  return Start;
}

//=======================================================================================================
// Function:  int BtoG(unsigned int lValue)
//=======================================================================================================
signed int BtoG(unsigned int lValue) {
register unsigned int lV = lValue, lMax = 1;

  while ( lV > 0 ) {
    lV >>= 1;
    lMax <<= 1;
  }
  if ( lMax == 0 )
    return -1;
  return b2g(0,--lMax,lValue);
}

//=======================================================================================================
// Function:  int GtoB(unsigned int lValue)
//=======================================================================================================
signed int GtoB(unsigned int lValue) {
register unsigned int lV = lValue, lMax = 1;

  while ( lV > 0 ) {
    lV >>= 1;
    lMax <<= 1;
  }
  if ( lMax == 0 )
    return -1;
  return g2b(0,--lMax,lValue);
}

//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
//=======================================================================================================
void GRY(void) {    
signed int temp;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_value();
    value = (u16)BtoG((unsigned int)temp);
    put_value();
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DGRY(void) {    
signed int temp;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_value();
    value = BtoG(temp);
    put_valueD();
  } else
    PLC_PC += 8;
}
#endif

void GBIN(void) {    
signed int temp;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_value();
    value = (u16)GtoB((unsigned int)temp);
    put_value();
  } else
    PLC_PC += 4;
}

#ifdef USE_DOUBLE
void DGBIN(void) {    
signed int temp;

  if ( PLC_ACC_BIT & 0x01 ) {
    temp = get_value();
    value = GtoB(temp);
    put_valueD();
  } else
    PLC_PC += 8;
}
#endif

//=======================================================================================================
//=======================================================================================================
// Special command
//=======================================================================================================
//=======================================================================================================
//
// 224 LD=   0x01D0  5 0 0 0
// 225 LD>   0x01D2  5 0 0 0
// 226 LD<   0x01D4  5 0 0 0
// 228 LD<>  0x01D8  5 0 0 0
// 229 LD?   0x01DA  5 0 0 0
// 230 LD?   0x01DC  5 0 0 0
// 232 AND=  0x01E0  5 0 0 0
// 233 AND>  0x01E2  5 0 0 0
// 234 AND<  0x01E4  5 0 0 0
// 236 AND<> 0x01E8  5 0 0 0
// 237 AND?  0x01EA  5 0 0 0
// 238 AND?  0x01EC  5 0 0 0
// 240 OR=   0x01F0  5 0 0 0
// 241 OR>   0x01F2  5 0 0 0
// 242 OR<   0x01F4  5 0 0 0
// 244 OR<>  0x01F8  5 0 0 0
// 245 OR?   0x01FA  5 0 0 0
// 246 OR?   0x01FC  5 0 0 0
//=======================================================================================================
//=======================================================================================================
#ifdef USE_SPECIAL

void LD_EQU(void) {
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {
    if ( ( temp1 == temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 == temp2 )
      PLC_ACC_BIT |= 1;
  }
}

#ifdef USE_DOUBLE
void DLD_EQU(void) {
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1 ) {
    if ( ( temp1 == temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else {
    if ( temp1 == temp2 )
      PLC_ACC_BIT |= 1;
  }
}
#else
void DLD_EQU(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void OR_EQU() { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 == temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}

#ifdef USE_DOUBLE
void DOR_EQU() { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 == temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}
#else
void DOR_EQU(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void AND_EQU(void) {
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 == temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}

#ifdef USE_DOUBLE
void DAND_EQU(void) {
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 == temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}
#else
void DAND_EQU(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void LD_GTR(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {  
    if ( ( temp1 > temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 > temp2 )
      PLC_ACC_BIT |= 1;
  }
}

#ifdef USE_DOUBLE
void DLD_GTR(void) {
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1 ) {
    if ( ( temp1 > temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 > temp2 )
      PLC_ACC_BIT |= 1;
  }
}
#else
void DLD_GTR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void OR_GTR() { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 > temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}

#ifdef USE_DOUBLE
void DOR_GTR() { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 >= temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}
#else
void DOR_GTR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void AND_GTR(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 > temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}

#ifdef USE_DOUBLE
void DAND_GTR(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 > temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}
#else
void DAND_GTR(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void LD_LSS(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {  
    if ( ( temp1 < temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 < temp2 )
      PLC_ACC_BIT |= 1;
  }
}

#ifdef USE_DOUBLE
void DLD_LSS(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {  
    if ( ( temp1 < temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 < temp2 )
      PLC_ACC_BIT |= 1;
  }
}
#else
void DLD_LSS(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void OR_LSS() { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 < temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}

#ifdef USE_DOUBLE
void DOR_LSS() { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( (temp1 < temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}
#else
void DOR_LSS(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void AND_LSS(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 < temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}

#ifdef USE_DOUBLE
void DAND_LSS(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 < temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}
#else
void DAND_LSS(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void LD_LEQ(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {  
    if ( ( temp1 <= temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 <= temp2 )
      PLC_ACC_BIT |= 1;
  }
}  

#ifdef USE_DOUBLE
void DLD_LEQ(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {  
    if ( ( temp1 <= temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 <= temp2 )
      PLC_ACC_BIT |= 1;
  }
}  
#else
void DLD_LEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void OR_LEQ() { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 <= temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}

#ifdef USE_DOUBLE
void DOR_LEQ() { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 <= temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}
#else
void DOR_LEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void AND_LEQ(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 <= temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE;
}

#ifdef USE_DOUBLE
void DAND_LEQ(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 <= temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}
#else
void DAND_LEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void LD_GEQ(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {  
    if ( ( temp1 >= temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 >= temp2 )
      PLC_ACC_BIT |= 1;
  }
}

#ifdef USE_DOUBLE
void DLD_GEQ(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  PLC_ACC_BIT <<= 1;
  if ( PLC_STL_Status == 1) {
    if ( ( temp1 >= temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 >= temp2 )
      PLC_ACC_BIT |= 1;
  }
}
#else
void DLD_GEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void OR_GEQ() { 
signed short int temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 >= temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}

#ifdef USE_DOUBLE
void DOR_GEQ() { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 >= temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}
#else
void DOR_GEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void AND_GEQ(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 >= temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}

#ifdef USE_DOUBLE
void DAND_GEQ(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 >= temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}
#else
void DAND_GEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void LD_NEQ(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  PLC_ACC_BIT <<= 1;
  if ( PLC_STL_Status == 1) {
    if ( ( temp1 != temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 != temp2 )
      PLC_ACC_BIT |= 1;
  }
}

#ifdef USE_DOUBLE
void DLD_NEQ(void) { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  PLC_ACC_BIT <<= 1;    
  if ( PLC_STL_Status == 1) {  
    if ( ( temp1 != temp2 ) && ( PLC_BIT_TEST(PLC_STL_Addr) ) )
      PLC_ACC_BIT |= 1;
  } else { 
    if ( temp1 != temp2 )
      PLC_ACC_BIT |= 1;
  }
}
#else
void DLD_NEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void OR_NEQ() { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 != temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}

#ifdef USE_DOUBLE
void DOR_NEQ() { 
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 != temp2 ) || ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 0x01;
  else
    PLC_ACC_BIT &= 0xFE; 
}
#else
void DOR_NEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

void AND_NEQ(void) { 
s32 temp1,temp2;

  temp1 = get_value();
  temp2 = get_value();
  if ( ( temp1 != temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}

#ifdef USE_DOUBLE
void DAND_NEQ(void) {
s32 temp1,temp2;

  temp1 = get_valueD();
  temp2 = get_valueD();
  if ( ( temp1 != temp2 ) && ( PLC_ACC_BIT & 0x01 ) )
    PLC_ACC_BIT |= 1;
  else
    PLC_ACC_BIT &= 0xFE;
}
#else
void DAND_NEQ(void) {
  /* Original body was empty; semantics are not implemented. */
  PLC_Legacy_Incomplete();
}
#endif

#endif // USE_SPECIAL
//=======================================================================================================
//=======================================================================================================
// Instruction Parse 
//=======================================================================================================
//=======================================================================================================

/* Original PR2 declarations without any definition: explicit faults. */
void BTOW(void) { PLC_Legacy_Missing(); }
void DABSD(void) { PLC_Legacy_Missing(); }
void DACOS(void) { PLC_Legacy_Missing(); }
void DASIN(void) { PLC_Legacy_Missing(); }
void DATAN(void) { PLC_Legacy_Missing(); }
void DBON(void) { PLC_Legacy_Missing(); }
void DDEG(void) { PLC_Legacy_Missing(); }
void DENEG(void) { PLC_Legacy_Missing(); }
void DESTR(void) { PLC_Legacy_Missing(); }
void DEVAL(void) { PLC_Legacy_Missing(); }
void DEXP(void) { PLC_Legacy_Missing(); }
void DIS(void) { PLC_Legacy_Missing(); }
void DLOG10(void) { PLC_Legacy_Missing(); }
void DLOGE(void) { PLC_Legacy_Missing(); }
void DMEAN(void) { PLC_Legacy_Missing(); }
void DRAD(void) { PLC_Legacy_Missing(); }
void DSER(void) { PLC_Legacy_Missing(); }
void DSORT2(void) { PLC_Legacy_Missing(); }
void DSPD(void) { PLC_Legacy_Missing(); }
void DSUM(void) { PLC_Legacy_Missing(); }
void DWSUM(void) { PLC_Legacy_Missing(); }
void EBCD(void) { PLC_Legacy_Missing(); }
void EBIN(void) { PLC_Legacy_Missing(); }
void HTOS(void) { PLC_Legacy_Missing(); }
void RD3A(void) { PLC_Legacy_Missing(); }
void ROTS(void) { PLC_Legacy_Missing(); }
void RS2(void) { PLC_Legacy_Missing(); }
void SFWL(void) { PLC_Legacy_Missing(); }
void SORT2(void) { PLC_Legacy_Missing(); }
void STOH(void) { PLC_Legacy_Missing(); }
void UNI(void) { PLC_Legacy_Missing(); }
void WR3A(void) { PLC_Legacy_Missing(); }
void WSUM(void) { PLC_Legacy_Missing(); }
void WTOB(void) { PLC_Legacy_Missing(); }
void ZPOP(void) { PLC_Legacy_Missing(); }
void ZPUSH(void) { PLC_Legacy_Missing(); }

#include "ladder_legacy_adapter.inc"
