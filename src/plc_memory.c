/**
 * @file    plc_memory.c
 * @brief   Reservation of the fixed-address PLC RAM areas and range checking.
 */
#include "plc_memory.h"

/* Reserve the interpreter's fixed-address areas in both linkers. The IAR
 * variables are __no_init, so startup code does not clear them; the GCC build
 * keeps them in dedicated sections for the same purpose. */
#ifdef __ICCARM__
#pragma location=".plc_ram"
__no_init uint8_t plc_ram[PLC_RAM_SIZE];
#pragma location=".plc_steps"
__no_init uint8_t plc_steps[PLC_STEPS_SIZE];
#else
uint8_t plc_ram[PLC_RAM_SIZE] __attribute__((section(".plc_ram"), used));
uint8_t plc_steps[PLC_STEPS_SIZE] __attribute__((section(".plc_steps"), used));
#endif

/** Zero the whole register image and the pulse bitmap (called once at boot). */
void plc_memory_init(void) {
    uint32_t i;
    for (i = 0; i < PLC_RAM_SIZE; i++) plc_ram[i] = 0U;
    for (i = 0; i < PLC_STEPS_SIZE; i++) plc_steps[i] = 0U;
}

/**
 * Check that a byte range lies inside the register image.
 * Written as "length <= size - offset" so that offset + length cannot overflow.
 * @param offset  byte offset from the start of the image.
 * @param length  number of bytes.
 */
bool plc_ram_range(uint32_t offset, uint32_t length) {
    return offset <= PLC_RAM_SIZE && length <= PLC_RAM_SIZE - offset;
}
