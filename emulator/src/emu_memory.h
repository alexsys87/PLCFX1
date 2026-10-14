/**
 * @file    emu_memory.h
 * @brief   Fixed-address memory of the emulated STM32F103C8 and the flash image file.
 *
 * The firmware addresses its PLC register image (0x20002000) and its program flash
 * (0x0800C000) by absolute address. The emulator reserves the same virtual addresses in
 * its own process, so the firmware sources run unchanged:
 *
 *     0x08000000 .. 0x0800FFFF   flash (program area and retained block are used)
 *     0x20000000 .. 0x2000FFFF   SRAM (pulse bitmap at 0x20001C00, PLC image at 0x20002000)
 *
 * The program area and the retained block (0x0800C000 .. 0x0800FFFF) are kept in an image
 * file, so a downloaded program survives a restart of the emulator like it survives a
 * power cycle of the board.
 */
#ifndef EMU_MEMORY_H
#define EMU_MEMORY_H

#include "platform.h"

/**
 * Reserve and commit the fixed-address regions.
 * @return true on success; on failure the reason is logged (another module already
 *         occupies the address range, typically because of ASLR).
 */
bool emu_memory_map(void);

/**
 * Load the program flash from @p path. A missing file gives the factory state (parameter
 * header and an END-only program); a damaged file is logged, renamed to *.bad and replaced
 * by the factory state.
 */
void emu_flash_load(const char *path);

/** Write the program flash to the image file (atomically through a temporary file). */
bool emu_flash_save(void);

/** Restore the factory flash contents (erase everything, write the parameter header). */
void emu_flash_factory_reset(void);

/** Make the next flash writes/erases fail (fault injection for testing the NAK path). */
void emu_flash_set_fail(bool fail);
bool emu_flash_get_fail(void);

/** Number of flash operations that failed since the last call (and reset the counter). */
unsigned emu_flash_take_failures(void);

/** Total number of block writes and erases since start. */
unsigned long emu_flash_write_count(void);

#endif /* EMU_MEMORY_H */
