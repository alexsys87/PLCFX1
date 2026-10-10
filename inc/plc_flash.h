#ifndef PLC_FLASH_H
#define PLC_FLASH_H
#include "platform.h"

/* STM32F103C8: 1 KB erase pages, grouped into 2 KB PLC blocks. */
#define PLC_FLASH_PAGE_SIZE 1024UL
#define PLC_FLASH_BLOCK_SIZE 2048UL
#define PLC_FLASH_END 0x08010000UL
#define PLC_PROGRAM_BASE 0x0800C000UL
#define PLC_RETAIN_BASE 0x0800F800UL
#define PLC_PROGRAM_BLOCKS ((PLC_RETAIN_BASE - PLC_PROGRAM_BASE) / PLC_FLASH_BLOCK_SIZE)

bool plc_flash_write_block(uint32_t address, const uint16_t *data);
bool plc_flash_erase_block(uint32_t address);
#endif
