/************************************************************
Library name:    PLC_Dialogue
Description:  PLC macro processing
Author:    Chuanren Ji
Date:    July 22, 2015
Version:    1.1
************************************************************/
#ifndef __PLC_Dialogue_H__
#define __PLC_Dialogue_H__
#include "stm32f10x.h"

#ifdef __cplusplus
extern "C"  {
#endif

extern const unsigned char PLC_BIT_OR[];
extern const unsigned char PLC_BIT_AND[];

#ifdef __cplusplus
}
#endif

#define Addr_ML         0x0000  // M0000 - M1535
#define Addr_MH         0x00C0  // M1536 - M3071
#define Addr_Y          0x0180  // Y000  - Y777
#define Addr_YH         0x01A0  // Y400  - Y777
#define Addr_M8         0x01C0  // M8000 - M8255
#define Addr_CO         0x01E0  // C000  - C255  Overflow coil
#define Addr_TO         0x0200  // T000  - T511  Overflow coil
#define Addr_X          0x0240  // X000  - X777
#define Addr_XH         0x0260  // X400  - X777
#define Addr_S          0x0280  // S0000 - S999
#define Addr_SH         0x02FD  // S1000 - S4095
#define Addr_ZZZZ       0x0480  // ZZZZ coil; purpose not documented in the original source.
#define Addr_CE         0x04E0  // C000 - C255 enable coil; original annotation incomplete.
#define Addr_TE         0x0500  // T000  - T511  Enable coil

#define Addr_CVal       0x0A00  // C000  - C199  Counter value 16 bit
#define Addr_CHVal      0x0C00  // C200  - C255  Counter value 32 bit

#define Addr_D8000      0x0E00  // D8000 - D8255

#define Addr_TVal       0x1000  // T000  - T255  Timer value
#define Addr_TMax       0x1200  // T000  - T255  Timer max value
#define Addr_D0000      0x2000  // In request
#define Addr_D1000      0x27D0  // In request

#define AddrB_S         0x0000  // S000  - S999
#define AddrB_X         0x0080  // X000  - X377
#define AddrB_Y         0x00A0  // Y000  - Y377
#define AddrB_T3        0x00E0  // T000 - T255 coil; original annotation incomplete.

#define AddrB_TO        0x0600  // T000  - T511  Overflow coil
#define AddrB_CO        0x0E00  // C000  - C255  Overflow coil
#define AddrB_TE        0x1600  // T000  - T511  Enable coil
#define AddrB_T1        0x2300  // T000 - T255 coil; original annotation incomplete.
#define AddrB_T2        0x2600  // T000 - T255 coil; original annotation incomplete.
#define AddrB_CE        0x2700  // C000 - C255 enable coil; original annotation incomplete.
#define AddrB_Tx        0x3800  // T000 - T255 coil; purpose not documented in the original source.

#define ROM_ADDR        0x0800C000
#define START_ADDR      0x005C
#define PLC_START_ADDR  (u16*)(ROM_ADDR + START_ADDR)

#define MAXSTEP         8000
#define MAXTIMER        256
#define MAXDXXX         2000

#define SRAM_ADDR       0x2000
#define STEP_ADDR       SRAM_ADDR - ( MAXSTEP / 8 + 16 ) & 0xFF00
#define RAM_ADDR        SRAM_BASE + SRAM_ADDR    //PLC_RAM start address
#define RAM_ACC_ADDR    RAM_ADDR - 4
#define RAM_STEP_ADDR   RAM_ADDR - ( MAXSTEP / 8 + 16 ) & 0xFF00
#define RAM_T_ADDR      RAM_ADDR + Addr_TVal  //PLC T start address
#define RAM_C_ADDR      RAM_ADDR + Addr_CVal  //PLC C start address
#define RAM_D0000_ADDR  RAM_ADDR + Addr_D0000 //PLC D start address
#define RAM_D1000_ADDR  RAM_ADDR + Addr_D1000 //PLC D1000 start address
#define RAM_D8000_ADDR  RAM_ADDR + Addr_D8000 //PLC D8000 start address

#define ROTATE_LEFT(x, s, n)        ((x) << (n)) | ((x) >> ((s) - (n)))      //Rotate left: x is the data, s is its bit width, and n is the rotation count
#define ROTATE_RIGHT(x, s, n)       ((x) >> (n)) | ((x) << ((s) - (n)))      //Rotate right: x is the data, s is its bit width, and n is the rotation count

#define PLC_D_C_T_addr(x)  ((x) % (0x4000))         //Used for V and Z

#define PLC_v_z_addr(x)    ((x) / (0x4000))         //Used for V and Z

#define PLC_Bu(x)          (*( u8*)(u32)(x))              //Byte access  R/W RAM  R ROM
#define PLC_Hu(x)          (*(u16*)(u32)(x))            //Halfword access  R/W RAM  R ROM
#define PLC_Wu(x)          (*(u32*)(u32)(x))            //Halfword access  R/W RAM  R ROM

#define PLC_Hs(x)          (*(s16*)(u32)(x))            //Halfword access  R/W RAM  R ROM
#define PLC_Ws(x)          (*(s32*)(u32)(x))            //Word access  R/W RAM  R ROM

#define PLC_Du(x)          (*(int64_t*)(u32)(x))        //Word access  R/W RAM  R ROM

#define PLC_Wf(x)          (*(float*)(u32)(x))        //Word access  R/W RAM  R ROM

#define valueF             FLOAT.data                   //Used for floating-point output

#define PLC_8BIT(x)        PLC_Bu(RAM_ADDR + x)
#define PLC_16BIT(x)       PLC_Hu(RAM_ADDR + x)
#define PLC_32BIT(x)       PLC_Wu(RAM_ADDR + x)

#define PLC_ROM_8BIT(x)    PLC_Bu(ROM_ADDR + x)
#define PLC_ROM_16BIT(x)   PLC_Hu(ROM_ADDR + x)

#define PLC_BIT_TEST1(x)    (PLC_Bu(RAM_ADDR + ((x)/8)) & PLC_BIT_OR[(x)%8])
#define PLC_BIT_ON1(x)      (PLC_Bu(RAM_ADDR + ((x)/8)) |= PLC_BIT_OR[(x)%8])
#define PLC_BIT_OFF1(x)     (PLC_Bu(RAM_ADDR + ((x)/8)) &= PLC_BIT_AND[(x)%8])

#define BB_SRAM(RAM,BIT)   (*(volatile uint32_t*)(SRAM_BB_BASE + 32 * ((uint32_t)((void*)(RAM))) + 4 * ((uint32_t)(BIT))))

#define PLC_PL_BIT_ON1(x)      (BB_SRAM( (STEP_ADDR + ( (x) >> 3 )), ((x) & 7)) = 1)
#define PLC_PL_BIT_OFF1(x)     (BB_SRAM( (STEP_ADDR + ( (x) >> 3 )), ((x) & 7)) = 0)
#define PLC_PL_BIT_TEST1(x)    (BB_SRAM( (STEP_ADDR + ( (x) >> 3 )), ((x) & 7)) )

#define BB_REG(REG,BIT)   (*(volatile uint32_t*)(PERIPH_BB_BASE + 32 * ((uint32_t)((void*)(REG))) + 4 * ((uint32_t)(BIT))))

/*
 *Floating-point union
 */
typedef union                                
{
  float data;
  u16   d16[2];
  u32   d32;
} float_union;
/*
 *32-bit integer union
 */
typedef union
{
  s32 d32;
  s16 d16[2];
} s32_union;
/*
 *64-bit integer union
 */
typedef union
{
  int64_t data;
  u16 data1[4];
} u64_union;

typedef union {
  struct {
    u8 M[384];
    u8 Y[64];
    u8 M8[32];
    u8 CO[32];
    u8 TO[64];
    u8 X[64];
    u8 S[512];
    u8 TE[64];
  } data;
  u16 w[12100];
} PLC;

#endif

#ifndef __at
#define __at(_addr) __attribute__ ((at(_addr)))
#endif
