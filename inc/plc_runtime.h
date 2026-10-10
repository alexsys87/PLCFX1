#ifndef PLC_RUNTIME_H
#define PLC_RUNTIME_H
#include "platform.h"

typedef enum {
  PLC_RUNTIME_OK = 0,
  PLC_RUNTIME_OPCODE = 1,
  PLC_RUNTIME_OPERAND = 2,
  PLC_RUNTIME_BOUNDS = 3,
  PLC_RUNTIME_STACK = 4,
  PLC_RUNTIME_NO_END = 5
} plc_runtime_error;

/* Init resets runtime-private state, not the PLC register image or RUN request. */
void PLC_Runtime_Init(void);
void PLC_Runtime_Tick1ms(void);
/* ISR-safe mailbox: no PLC image writes. The newest pending request wins. */
void PLC_Runtime_RequestRun(u8 run);
u8 PLC_Runtime_RunRequested(void);
void PLC_Runtime_ClearFault(void);
plc_runtime_error PLC_Runtime_LastError(void);
u16 PLC_Runtime_ErrorStep(void);
void PLC_Main_Parser(void);
void PLC_ProInstructParse(void); /* Legacy entry point. */
u32 PLC_BIT_TEST(u32 linear_bit);
void PLC_BIT_ON(u32 linear_bit);
void PLC_BIT_OFF(u32 linear_bit);
extern volatile u8 PLC_NEW_PROG;
extern u8 PLC_PRG_STOP;
extern u8 PLC_PULS;
#endif
