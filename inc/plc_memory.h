#ifndef PLC_MEMORY_H
#define PLC_MEMORY_H
#include "platform.h"

#define PLC_RAM_BASE 0x20002000UL
#define PLC_RAM_SIZE 0x3000UL
#define PLC_STEPS_BASE 0x20001C00UL
#define PLC_STEPS_SIZE 0x400UL

void plc_memory_init(void);
bool plc_ram_range(uint32_t offset, uint32_t length);
#endif
