/**
 * @file    plc_memory.h
 * @brief   Fixed-address RAM areas reserved for the PLC.
 *
 * The interpreter reaches its register image and the pulse bitmap by absolute
 * address, so the linker must keep both areas free of ordinary variables.
 * plc_memory.c reserves them in the sections .plc_ram and .plc_steps, and the
 * linker script (ewarm/bluepill_plc.icf) places the sections at the addresses
 * below. The layout inside the image is described in plc_memory_map.h.
 */
#ifndef PLC_MEMORY_H
#define PLC_MEMORY_H
#include "platform.h"

#define PLC_RAM_BASE 0x20002000UL   /**< Start of the PLC register image. */
#define PLC_RAM_SIZE 0x3000UL       /**< Size of the PLC register image: 12 KB. */
#define PLC_STEPS_BASE 0x20001C00UL /**< Start of the per-instruction pulse (edge) bitmap. */
#define PLC_STEPS_SIZE 0x400UL      /**< Size of the pulse bitmap: 1 KB. */

/** Clear the PLC register image and the pulse bitmap. */
void plc_memory_init(void);

/** True if [offset, offset + length) lies completely inside the PLC register image (overflow-safe). */
bool plc_ram_range(uint32_t offset, uint32_t length);
#endif
