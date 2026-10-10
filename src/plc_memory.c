#include "plc_memory.h"

/* Reserve the interpreter's fixed-address areas in both linkers. */
#ifdef __ICCARM__
#pragma location=".plc_ram"
__no_init uint8_t plc_ram[PLC_RAM_SIZE];
#pragma location=".plc_steps"
__no_init uint8_t plc_steps[PLC_STEPS_SIZE];
#else
uint8_t plc_ram[PLC_RAM_SIZE] __attribute__((section(".plc_ram"), used));
uint8_t plc_steps[PLC_STEPS_SIZE] __attribute__((section(".plc_steps"), used));
#endif

void plc_memory_init(void) {
    uint32_t i;
    for (i = 0; i < PLC_RAM_SIZE; i++) plc_ram[i] = 0U;
    for (i = 0; i < PLC_STEPS_SIZE; i++) plc_steps[i] = 0U;
}

bool plc_ram_range(uint32_t offset, uint32_t length) {
    return offset <= PLC_RAM_SIZE && length <= PLC_RAM_SIZE - offset;
}
