/**
 * @file    plc_flash.h
 * @brief   Flash layout of the PLC and the block-wise flash programming interface.
 *
 * STM32F103C8: 1 KB erase pages, grouped into 2 KB PLC blocks.
 *
 *     0x08000000 .. 0x0800BFFF   firmware (48 KB)
 *     0x0800C000 .. 0x0800F7FF   PLC program area, seven 2 KB blocks (14 KB)
 *     0x0800F800 .. 0x0800FFFF   retained-data block (2 KB), never used by the linker
 *     0x08010000                 end of the 64 KB flash
 *
 * The program area begins with the parameter header (PLC_ROM in
 * plc_serial_protocol.c) and the program words follow at offset 0x5C.
 */
#ifndef PLC_FLASH_H
#define PLC_FLASH_H
#include "platform.h"

/* STM32F103C8: 1 KB erase pages, grouped into 2 KB PLC blocks. */
#define PLC_FLASH_PAGE_SIZE 1024UL          /**< Erase granularity of the flash. */
#define PLC_FLASH_BLOCK_SIZE 2048UL         /**< PLC block: two pages, erased and written together. */
#define PLC_FLASH_END 0x08010000UL          /**< First address after the flash. */
#define PLC_PROGRAM_BASE 0x0800C000UL       /**< Start of the PLC program area. */
#define PLC_RETAIN_BASE 0x0800F800UL        /**< Start of the retained-data block (end of the program area). */
#define PLC_PROGRAM_BLOCKS ((PLC_RETAIN_BASE - PLC_PROGRAM_BASE) / PLC_FLASH_BLOCK_SIZE)  /**< Program blocks (7). */

/**
 * Erase one PLC block and program it with 1024 halfwords.
 * @param address  block-aligned address inside the program area or the retained block.
 * @param data     new block contents (PLC_FLASH_BLOCK_SIZE bytes); must not be NULL.
 * @return true on success; false for a bad address, NULL data, a flash error or a time-out.
 */
bool plc_flash_write_block(uint32_t address, const uint16_t *data);

/**
 * Erase one PLC block (it reads as 0xFFFF afterwards).
 * @param address  block-aligned address inside the program area or the retained block.
 * @return true on success; false for a bad address, a flash error or a time-out.
 */
bool plc_flash_erase_block(uint32_t address);
#endif
