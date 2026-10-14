/**
 * @file    emu_board.c
 * @brief   Emulated core and board services: peripheral registers, PRIMASK, clocks.
 *
 * Replaces src/board_clock.c and the CMSIS core intrinsics for the host build, and owns
 * the USART1/DMA1 register objects that platform.h maps the firmware's USART1, DMA1 and
 * DMA1_Channel4 names to.
 */
#include "emu_board.h"

#include "board_clock.h"
#include "emu_log.h"

emu_usart_regs emu_usart1;
emu_dma_regs emu_dma1;
emu_dma_channel_regs emu_dma1_channel4;

/** Milliseconds since reset, saturating at 65535 (App/main.c owns it on the target). */
volatile u16 PLC_RUN_TIME;

static u32 primask;
static unsigned long irq_disable_count;

/* ---- CMSIS core intrinsics --------------------------------------------------------------- */

u32 __get_PRIMASK(void) { return primask; }

void __disable_irq(void) {
  primask = 1U;
  irq_disable_count++;
}

void __set_PRIMASK(u32 value) { primask = value & 1U; }

bool emu_irq_enabled(void) { return primask == 0U; }

void emu_check_primask(const char *where) {
  if (primask) {
    LOG_ERROR(EMU_CAT_RUNTIME,
              "Interrupts are still masked after %s: a critical section was not left "
              "(PRIMASK restored to 0 by the emulator)",
              where);
    primask = 0U;
  }
}

/* ---- board_clock.c replacement ----------------------------------------------------------- */

/* The emulated board always runs from the 8 MHz crystal at 72 MHz. */
bool board_clock_init(void) { return true; }
uint32_t board_apb1_timer_hz(void) { return 72000000U; }
uint32_t board_apb2_hz(void) { return 72000000U; }
