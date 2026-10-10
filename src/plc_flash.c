#include "plc_flash.h"

#define FLASH_WAIT_LIMIT 2000000UL
#define FLASH_ERRORS (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)
#define FLASH_CLEAR_FLAGS (FLASH_ERRORS | FLASH_SR_EOP)

static bool storage_address(uint32_t address) {
    return address >= PLC_PROGRAM_BASE &&
           address <= PLC_FLASH_END - PLC_FLASH_BLOCK_SIZE &&
           (address & (PLC_FLASH_BLOCK_SIZE - 1UL)) == 0UL;
}

static bool wait_ready(void) {
    uint32_t timeout = FLASH_WAIT_LIMIT;
    while (FLASH->SR & FLASH_SR_BSY) {
        if (--timeout == 0U) return false;
    }
    return (FLASH->SR & FLASH_ERRORS) == 0U;
}

static bool unlock(void) {
    uint32_t timeout = FLASH_WAIT_LIMIT;
    while (FLASH->SR & FLASH_SR_BSY) {
        if (--timeout == 0U) return false;
    }
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123UL;
        FLASH->KEYR = 0xCDEF89ABUL;
    }
    FLASH->SR = FLASH_CLEAR_FLAGS; /* Write one to clear stale status. */
    return (FLASH->CR & FLASH_CR_LOCK) == 0U;
}

static bool erase_page(uint32_t address) {
    bool ok;
    FLASH->SR = FLASH_CLEAR_FLAGS;
    FLASH->CR = (FLASH->CR & ~FLASH_CR_PG) | FLASH_CR_PER;
    FLASH->AR = address;
    FLASH->CR |= FLASH_CR_STRT;
    ok = wait_ready();
    FLASH->CR &= ~FLASH_CR_PER;
    return ok;
}

static bool program_halfword(uint32_t address, uint16_t value) {
    bool ok;
    if (value == 0xFFFFU) return true; /* Leave erased halfwords unchanged. */
    FLASH->SR = FLASH_CLEAR_FLAGS;
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)address = value;
    ok = wait_ready();
    FLASH->CR &= ~FLASH_CR_PG;
    return ok && *(volatile const uint16_t *)address == value;
}

static bool update_block(uint32_t address, const uint16_t *data) {
    bool ok;
    uint32_t primask;
    uint32_t i;
    if (!storage_address(address)) return false;
    primask = __get_PRIMASK();
    __disable_irq();
    ok = unlock();
    if (ok) ok = erase_page(address);
    if (ok) ok = erase_page(address + PLC_FLASH_PAGE_SIZE);
    if (ok && data != 0) {
        for (i = 0; i < PLC_FLASH_BLOCK_SIZE / 2UL; i++) {
            if (!program_halfword(address + i * 2UL, data[i])) {
                ok = false;
                break;
            }
        }
    }
    FLASH->CR |= FLASH_CR_LOCK;
    __set_PRIMASK(primask);
    return ok;
}

bool plc_flash_write_block(uint32_t address, const uint16_t *data) {
    return data != 0 && update_block(address, data);
}

bool plc_flash_erase_block(uint32_t address) {
    return update_block(address, 0);
}
