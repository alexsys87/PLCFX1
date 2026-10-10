#ifndef _MAIN_H_
#define _MAIN_H_

#include "platform.h"
#include "plc_io.h"

void InitALL(void);
void PLC_ProInstructParse(void);
void RTC_Init(void);
void RTC_Get(void);
void PLC_IO_Refresh(void);
void PLC_IO_config(void);
void PLC_IO_SampleInputs(void);
void TX_Process(void);       /* Transmit PLC serial data. */
void PLC_DATA_KEEP(void);
void PLC_DATA_RECOVER(void); /* Restore retained data after power loss. */
void PLC_DATA_INIT(void);    /* Initialize special registers D8000-D8126. */

#endif /* _MAIN_H_ */
