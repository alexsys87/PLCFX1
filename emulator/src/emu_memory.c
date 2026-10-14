/**
 * @file    emu_memory.c
 * @brief   Fixed-address memory, plc_memory.c replacement and flash driver emulation.
 *
 * Replaces three firmware modules for the host build:
 *   - src/plc_memory.c:  plc_memory_init() and plc_ram_range() on the mapped SRAM;
 *   - src/plc_flash.c:   plc_flash_write_block() / plc_flash_erase_block() on the mapped
 *                        flash, with the same argument rules as the real driver;
 *   - the linker:        the factory .plc_rom image at the start of the program area.
 */
#include "emu_memory.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "emu_log.h"
#include "plc_flash.h"
#include "plc_memory.h"
#include "plc_memory_map.h"

#define FLASH_REGION_BASE 0x08000000UL
#define FLASH_REGION_SIZE 0x10000UL
#define SRAM_REGION_SIZE  0x10000UL
#define IMAGE_SIZE (PLC_FLASH_END - PLC_PROGRAM_BASE) /* program area + retained block, 16 KB */

/** Factory parameter header and END-only program (defined in plc_serial_protocol.c). */
extern const u8 PLC_ROM[110];

static char image_path[MAX_PATH];
static bool fail_flash;
static unsigned failures;
static unsigned long flash_writes;

/** Describe who owns @p address, to explain a failed mapping in the log. */
static void log_region_owner(uintptr_t address) {
  MEMORY_BASIC_INFORMATION info;
  char module[MAX_PATH] = "";
  if (VirtualQuery((LPCVOID)address, &info, sizeof info) == 0) return;
  if (info.AllocationBase)
    GetModuleFileNameA((HMODULE)info.AllocationBase, module, sizeof module);
  LOG_ERROR(EMU_CAT_SYSTEM,
            "  address 0x%08lX: state 0x%lX, allocation base %p, region size 0x%lX%s%s",
            (unsigned long)address, (unsigned long)info.State, info.AllocationBase,
            (unsigned long)info.RegionSize, module[0] ? ", module " : "", module);
}

/** Reserve and commit one region at exactly @p address. */
static bool map_fixed(uintptr_t address, size_t size, const char *name) {
  void *result = VirtualAlloc((void *)address, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (result != (void *)address) {
    LOG_FATAL(EMU_CAT_SYSTEM, "Cannot map %s at 0x%08lX (size 0x%lX): Windows error %lu", name,
              (unsigned long)address, (unsigned long)size, GetLastError());
    log_region_owner(address);
    if (result) VirtualFree(result, 0, MEM_RELEASE);
    return false;
  }
  LOG_DEBUG(EMU_CAT_SYSTEM, "Mapped %s at 0x%08lX..0x%08lX", name, (unsigned long)address,
            (unsigned long)(address + size - 1U));
  return true;
}

bool emu_memory_map(void) {
  if (!map_fixed(FLASH_REGION_BASE, FLASH_REGION_SIZE, "flash")) return false;
  if (!map_fixed(SRAM_BASE, SRAM_REGION_SIZE, "SRAM")) return false;
  memset((void *)FLASH_REGION_BASE, 0xFF, FLASH_REGION_SIZE);
  memset((void *)SRAM_BASE, 0, SRAM_REGION_SIZE);
  return true;
}

/* ---- plc_memory.c replacement ------------------------------------------------------------ */

void plc_memory_init(void) {
  memset((void *)(uintptr_t)PLC_RAM_BASE, 0, PLC_RAM_SIZE);
  memset((void *)(uintptr_t)PLC_STEPS_BASE, 0, PLC_STEPS_SIZE);
}

bool plc_ram_range(uint32_t offset, uint32_t length) {
  return offset <= PLC_RAM_SIZE && length <= PLC_RAM_SIZE - offset;
}

/* ---- Flash image file ---------------------------------------------------------------------- */

void emu_flash_factory_reset(void) {
  memset((void *)(uintptr_t)PLC_PROGRAM_BASE, 0xFF, IMAGE_SIZE);
  memcpy((void *)(uintptr_t)PLC_PROGRAM_BASE, PLC_ROM, sizeof PLC_ROM);
}

void emu_flash_load(const char *path) {
  FILE *file;
  long size;
  size_t got;

  snprintf(image_path, sizeof image_path, "%s", path);
  file = fopen(path, "rb");
  if (!file) {
    LOG_INFO(EMU_CAT_FLASH, "Flash image %s not found: starting with the factory program (END only)",
             path);
    emu_flash_factory_reset();
    emu_flash_save();
    return;
  }
  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fseek(file, 0, SEEK_SET);
  got = size == (long)IMAGE_SIZE ? fread((void *)(uintptr_t)PLC_PROGRAM_BASE, 1, IMAGE_SIZE, file) : 0;
  fclose(file);
  if (got != IMAGE_SIZE) {
    char bad[MAX_PATH + 8];
    snprintf(bad, sizeof bad, "%s.bad", path);
    LOG_ERROR(EMU_CAT_FLASH,
              "Flash image %s is damaged (size %ld bytes, expected %lu): renamed to %s, "
              "factory program loaded",
              path, size, (unsigned long)IMAGE_SIZE, bad);
    MoveFileExA(path, bad, MOVEFILE_REPLACE_EXISTING);
    emu_flash_factory_reset();
    emu_flash_save();
    return;
  }
  LOG_INFO(EMU_CAT_FLASH, "Flash image loaded from %s", path);
}

bool emu_flash_save(void) {
  char temporary[MAX_PATH + 8];
  FILE *file;
  size_t written;

  if (!image_path[0]) return true;
  snprintf(temporary, sizeof temporary, "%s.tmp", image_path);
  file = fopen(temporary, "wb");
  if (!file) {
    LOG_ERROR(EMU_CAT_FLASH, "Cannot create %s: the flash image is not saved (errno %d)", temporary,
              errno);
    return false;
  }
  written = fwrite((const void *)(uintptr_t)PLC_PROGRAM_BASE, 1, IMAGE_SIZE, file);
  if (fclose(file) != 0 || written != IMAGE_SIZE) {
    LOG_ERROR(EMU_CAT_FLASH, "Cannot write %s: the flash image is not saved", temporary);
    DeleteFileA(temporary);
    return false;
  }
  if (!MoveFileExA(temporary, image_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    LOG_ERROR(EMU_CAT_FLASH, "Cannot replace %s: Windows error %lu", image_path, GetLastError());
    DeleteFileA(temporary);
    return false;
  }
  return true;
}

void emu_flash_set_fail(bool fail) { fail_flash = fail; }
bool emu_flash_get_fail(void) { return fail_flash; }

unsigned emu_flash_take_failures(void) {
  unsigned count = failures;
  failures = 0;
  return count;
}

unsigned long emu_flash_write_count(void) { return flash_writes; }

/* ---- plc_flash.c replacement ------------------------------------------------------------- */

/** Same address rule as the firmware driver: a block-aligned address inside the PLC area. */
static bool valid_block(uint32_t address, const char *operation) {
  if (address < PLC_PROGRAM_BASE || address > PLC_FLASH_END - PLC_FLASH_BLOCK_SIZE ||
      (address & (PLC_FLASH_BLOCK_SIZE - 1U))) {
    LOG_ERROR(EMU_CAT_FLASH, "Flash %s rejected: bad block address 0x%08lX", operation,
              (unsigned long)address);
    failures++;
    return false;
  }
  if (fail_flash) {
    LOG_ERROR(EMU_CAT_FLASH, "Flash %s of block 0x%08lX failed (fault injection is ON)", operation,
              (unsigned long)address);
    failures++;
    return false;
  }
  return true;
}

bool plc_flash_write_block(uint32_t address, const uint16_t *data) {
  if (!data) {
    LOG_ERROR(EMU_CAT_FLASH, "Flash write of block 0x%08lX rejected: NULL data",
              (unsigned long)address);
    failures++;
    return false;
  }
  if (!valid_block(address, "write")) return false;
  memcpy((void *)(uintptr_t)address, data, PLC_FLASH_BLOCK_SIZE);
  flash_writes++;
  LOG_DEBUG(EMU_CAT_FLASH, "Block 0x%08lX written", (unsigned long)address);
  emu_flash_save();
  return true;
}

bool plc_flash_erase_block(uint32_t address) {
  if (!valid_block(address, "erase")) return false;
  memset((void *)(uintptr_t)address, 0xFF, PLC_FLASH_BLOCK_SIZE);
  flash_writes++;
  LOG_DEBUG(EMU_CAT_FLASH, "Block 0x%08lX erased", (unsigned long)address);
  emu_flash_save();
  return true;
}
