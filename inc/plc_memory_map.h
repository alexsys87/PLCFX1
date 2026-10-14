/**
 * @file    plc_memory_map.h
 * @brief   Layout of the PLC register image in RAM and the macros that access it.
 *
 * The PLC image is one contiguous RAM block (RAM_ADDR, PLC_RAM_SIZE bytes, see
 * plc_memory.h) that holds every device of the programming model: relays, timers,
 * counters, data registers and the special devices. Programs and tools address
 * it with byte offsets (the Addr_* constants below) or with linear bit addresses
 * (byte offset * 8 + bit index). The program itself lives in flash starting at
 * ROM_ADDR.
 *
 * The experimental interpreter replaces the raw accessors PLC_Bu, PLC_Hu, PLC_Wu,
 * PLC_Hs, PLC_Ws, PLC_Du and PLC_Wf with bounds-checked versions (plc_exp_guard.h).
 *
 * Original library: "PLC_Dialogue" (PLC macro processing), author Chuanren Ji,
 * 22 July 2015, version 1.1.
 */
#ifndef PLC_MEMORY_MAP_H
#define PLC_MEMORY_MAP_H
#include "platform.h"
#include "plc_flash.h"

#ifdef __cplusplus
extern "C"  {
#endif

/**
 * Single-bit masks for bit index 0..7: PLC_BIT_OR[n] sets bit n, PLC_BIT_AND[n]
 * clears it. Defined by the minimal runtime (plc_runtime_minimal.c).
 */
extern const unsigned char PLC_BIT_OR[];
extern const unsigned char PLC_BIT_AND[];

#ifdef __cplusplus
}
#endif

/* ---- Byte offsets of the devices inside the PLC image --------------------------------- */

#define Addr_ML         0x0000  /* M0000 - M1535 */
#define Addr_MH         0x00C0  /* M1536 - M3071 */
#define Addr_Y          0x0180  /* Y000  - Y777 (octal numbering) */
#define Addr_YH         0x01A0  /* Y400  - Y777 */
#define Addr_M8         0x01C0  /* M8000 - M8255 special relays */
#define Addr_CO         0x01E0  /* C000  - C255  counter overflow (output) coils */
#define Addr_TO         0x0200  /* T000  - T511  timer overflow (output) coils */
#define Addr_X          0x0240  /* X000  - X777 (octal numbering) */
#define Addr_XH         0x0260  /* X400  - X777 */
#define Addr_S          0x0280  /* S0000 - S999 */
#define Addr_SH         0x02FD  /* S1000 - S4095 */
#define Addr_ZZZZ       0x0480  /* ZZZZ coil; purpose not documented in the original source. */
#define Addr_CE         0x04E0  /* C000 - C255 enable coil; original annotation incomplete. */
#define Addr_TE         0x0500  /* T000  - T511  enable coil */

#define Addr_CVal       0x0A00  /* C000  - C199  counter current value, 16 bit */
#define Addr_CHVal      0x0C00  /* C200  - C255  counter current value, 32 bit */

#define Addr_D8000      0x0E00  /* D8000 - D8255 special data registers */

#define Addr_TVal       0x1000  /* T000  - T255  timer current value */
#define Addr_TMax       0x1200  /* T000  - T255  timer preset (maximum) value */
#define Addr_D0000      0x2000  /* D0000 - general data registers (start of the D area) */
#define Addr_D1000      0x27D0  /* D1000 - second block of data registers (D0000 + 1000 words) */

/* Addr_* are BYTE offsets. A linear bit address is Addr_* * 8 + index.
 * Contradictory historical AddrB_* aliases were removed. */

/* ---- Program storage ------------------------------------------------------------------ */

#define ROM_ADDR        PLC_PROGRAM_BASE  /* flash address of the program area */
#define START_ADDR      0x005C            /* byte offset of the first program word (after the parameter header) */
#define PLC_START_ADDR  (u16*)(ROM_ADDR + START_ADDR)  /* pointer to the first program word */

#define MAXSTEP         6000 /* Program capacity in 16-bit words; fits the 14 KB program storage of the STM32F103C8. */
#define MAXTIMER        256  /* Number of timer slots. */
#define MAXDXXX         2000 /* Number of general data registers (D0000 - D1999) that E63 clears. */

/* ---- RAM addresses -------------------------------------------------------------------- */

#define SRAM_ADDR       0x2000 /* Offset of the PLC image from SRAM_BASE. */
#define STEP_ADDR       0x1C00UL /* Reserved pulse-state bitmap, below PLC RAM. */
#define RAM_ADDR        (SRAM_BASE + SRAM_ADDR)    /* Start address of the PLC image. */
#define RAM_ACC_ADDR    (RAM_ADDR - 4)             /* Four bytes below the PLC image (slot named ACC in the original; unused). */
#define RAM_STEP_ADDR   (SRAM_BASE + STEP_ADDR)    /* Start address of the pulse-state bitmap. */
#define RAM_T_ADDR      (RAM_ADDR + Addr_TVal)     /* Start address of the timer values. */
#define RAM_C_ADDR      (RAM_ADDR + Addr_CVal)     /* Start address of the counter values. */
#define RAM_D0000_ADDR  (RAM_ADDR + Addr_D0000)    /* Start address of D0000. */
#define RAM_D1000_ADDR  (RAM_ADDR + Addr_D1000)    /* Start address of D1000. */
#define RAM_D8000_ADDR  (RAM_ADDR + Addr_D8000)    /* Start address of D8000. */

/* ---- Helper macros (legacy; not used by the current sources) -------------------------- */

/* Rotate left/right: x is the data, s its bit width and n the rotation count.
 * The expansion is not parenthesised as a whole; wrap the call when mixing it with other operators. */
#define ROTATE_LEFT(x, s, n)        ((x) << (n)) | ((x) >> ((s) - (n)))
#define ROTATE_RIGHT(x, s, n)       ((x) >> (n)) | ((x) << ((s) - (n)))

/* Operand encoding with an index register: bits 0-13 select the device offset and
 * the upper bits select V or Z. */
#define PLC_D_C_T_addr(x)  ((x) % (0x4000))   /* offset part of the operand */

#define PLC_v_z_addr(x)    ((x) / (0x4000))   /* index register part of the operand */

/* ---- Raw memory access by absolute address (volatile) --------------------------------- */

#define PLC_Bu(x)          (*(volatile u8*)(u32)(x))     /* unsigned 8 bit;  RAM read/write, ROM read */
#define PLC_Hu(x)          (*(volatile u16*)(u32)(x))    /* unsigned 16 bit; RAM read/write, ROM read */
#define PLC_Wu(x)          (*(volatile u32*)(u32)(x))    /* unsigned 32 bit; RAM read/write, ROM read */

#define PLC_Hs(x)          (*(volatile s16*)(u32)(x))    /* signed 16 bit */
#define PLC_Ws(x)          (*(volatile s32*)(u32)(x))    /* signed 32 bit */

#define PLC_Du(x)          (*(volatile int64_t*)(u32)(x))  /* signed 64 bit */

#define PLC_Wf(x)          (*(volatile float*)(u32)(x))    /* single-precision float */

#define valueF             FLOAT.data                   /* Legacy alias used for floating-point output. */

/* ---- Access to the PLC image by byte offset ------------------------------------------- */

#define PLC_8BIT(x)        PLC_Bu(RAM_ADDR + (x))      /* byte at offset x of the PLC image */
#define PLC_16BIT(x)       PLC_Hu(RAM_ADDR + (x))      /* 16-bit word at offset x */
#define PLC_32BIT(x)       PLC_Wu(RAM_ADDR + (x))      /* 32-bit word at offset x */

#define PLC_ROM_8BIT(x)    PLC_Bu(ROM_ADDR + (x))      /* byte at offset x of the program flash area */
#define PLC_ROM_16BIT(x)   PLC_Hu(ROM_ADDR + (x))      /* 16-bit word at offset x of the program flash area */

/* Bit access by linear bit address x (byte offset * 8 + bit); legacy macro forms of
 * PLC_BIT_TEST/ON/OFF(). */
#define PLC_BIT_TEST1(x)    (PLC_Bu(RAM_ADDR + ((x)/8)) & PLC_BIT_OR[(x)%8])
#define PLC_BIT_ON1(x)      (PLC_Bu(RAM_ADDR + ((x)/8)) |= PLC_BIT_OR[(x)%8])
#define PLC_BIT_OFF1(x)     (PLC_Bu(RAM_ADDR + ((x)/8)) &= PLC_BIT_AND[(x)%8])

/* Cortex-M3 bit-band alias of bit BIT of the SRAM byte at offset RAM from SRAM_BASE. */
#define BB_SRAM(RAM,BIT)   (*(volatile uint32_t*)(SRAM_BB_BASE + 32 * ((uint32_t)((void*)(RAM))) + 4 * ((uint32_t)(BIT))))

/* Bit-band access to the pulse-state bitmap by step number x. */
#define PLC_PL_BIT_ON1(x)      (BB_SRAM( (STEP_ADDR + ( (x) >> 3 )), ((x) & 7)) = 1)
#define PLC_PL_BIT_OFF1(x)     (BB_SRAM( (STEP_ADDR + ( (x) >> 3 )), ((x) & 7)) = 0)
#define PLC_PL_BIT_TEST1(x)    (BB_SRAM( (STEP_ADDR + ( (x) >> 3 )), ((x) & 7)) )

/* Cortex-M3 bit-band alias of bit BIT of the peripheral register at offset REG from PERIPH_BASE. */
#define BB_REG(REG,BIT)   (*(volatile uint32_t*)(PERIPH_BB_BASE + 32 * ((uint32_t)((void*)(REG))) + 4 * ((uint32_t)(BIT))))

/* ---- Overlay types (legacy; not used by the current sources) -------------------------- */

/** Single-precision float viewed as two 16-bit words or one 32-bit word. */
typedef union
{
  float data;
  u16   d16[2];
  u32   d32;
} float_union;

/** Signed 32-bit integer viewed as two 16-bit halves. */
typedef union
{
  s32 d32;
  s16 d16[2];
} s32_union;

/** 64-bit integer viewed as four 16-bit words. */
typedef union
{
  int64_t data;
  u16 data1[4];
} u64_union;

/**
 * Overlay of the bit-device area of the image, in the order of the original
 * source. The offsets of TE and later members do not match the Addr_* constants
 * above (those are authoritative); the type is not used by the current sources.
 */
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

/* ARM Compiler 5 syntax for placing an object at an absolute address. Not used by the
 * current sources (IAR places objects with #pragma location); kept from the original. */
#ifndef __at
#define __at(_addr) __attribute__ ((at(_addr)))
#endif

#endif /* PLC_MEMORY_MAP_H */
