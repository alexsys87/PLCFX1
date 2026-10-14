#!/usr/bin/env python3
"""Run the actual board I/O implementation (src/plc_io.c) against modelled CMSIS registers.

The test copies the project headers into a temporary directory, replaces platform.h by a model
that has only the GPIO and RCC registers used by the I/O code, and compiles src/plc_io.c
unchanged together with a small C test (AddressSanitizer and UBSan). The C test checks the pin
configuration, the input debounce filter, the per-scan snapshot of the inputs and the gating of
the outputs while the PLC is stopped.

Usage: python3 tools/test_io.py
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# Replacement for inc/platform.h: the integer names and the register blocks that plc_io.c uses.
PLATFORM = '''
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int16_t s16; typedef int32_t s32;
/* Register models: a GPIO port (configuration, input, output, set/reset) and the APB2 clock enable. */
typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR; } MockGPIO;
typedef struct { volatile uint32_t APB2ENR; } MockRCC;
extern MockGPIO gpio_a, gpio_b;
extern MockRCC rcc;
/* The ports and the clock controller are ordinary variables defined by the test. */
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define RCC (&rcc)
#define RCC_APB2ENR_IOPAEN (1U<<2)
#define RCC_APB2ENR_IOPBEN (1U<<3)
/* The PLC image is placed relative to this address; the test maps host memory there. */
#define SRAM_BASE 0x20000000UL
#endif
'''
# C test program: X0..X7 are PA0..PA7, Y0..Y7 are PB0, PB1 and PB5..PB10 (mask 0x7E3).
TEST = '''
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include "platform.h"
#include "plc_memory_map.h"
MockGPIO gpio_a, gpio_b;
MockRCC rcc;
/* Stand-in for the runtime's RUN request, which the output stage consults on every refresh. */
static u8 requested_run = 1U;
u8 PLC_Runtime_RunRequested(void) { return requested_run; }
/* Debounce time in 1 ms ticks and the functions under test, all defined in src/plc_io.c. */
extern u16 X_DIY;
extern void PLC_IO_config(void), PLC_IO_Refresh(void), X_filter(void), PLC_IO_SampleInputs(void);
int main(void) {
 /* The PLC image lives in SRAM: map 20 KiB of host memory at the real SRAM address. */
 assert(mmap((void*)SRAM_BASE, 20*1024, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) == (void*)SRAM_BASE);
 /* Pin configuration. PA0..PA7: inputs with pull (CRL nibble 8) and pull-down (BRR clears ODR).
  * Y pins: driven low first, then 2 MHz push-pull outputs (nibble 2) on PB0, PB1, PB5..PB10. */
 PLC_IO_config();
 assert(gpio_a.CRL == 0x88888888U);
 assert(gpio_a.BRR == 0xFFU);
 assert(gpio_b.BRR == 0x7E3U);
 assert((gpio_b.CRL & 0xFFF000FFU) == 0x22200022U);
 assert((gpio_b.CRH & 0xFFFU) == 0x222U);
 /* Debounce: X0 and X7 go high; with the default 10 ms the level is accepted on the tenth tick. */
 gpio_a.IDR = 0x81;
 for (int i=0;i<9;i++) X_filter();
 PLC_IO_SampleInputs(); assert(PLC_8BIT(Addr_X) == 0);
 X_filter(); assert(PLC_8BIT(Addr_X) == 0); /* IRQ does not alter scan image. */
 PLC_IO_SampleInputs(); assert(PLC_8BIT(Addr_X) == 0x81);
 /* Releasing the inputs needs the same ten ticks. */
 gpio_a.IDR = 0; for (int i=0;i<10;i++) X_filter();
 PLC_IO_SampleInputs(); assert(PLC_8BIT(Addr_X) == 0);
 /* A debounce time of zero passes a level change on the first tick. */
 X_DIY=0; gpio_a.IDR=0xFF; X_filter(); PLC_IO_SampleInputs();
 assert(PLC_8BIT(Addr_X) == 0xFF);
 /* Outputs. Y image all ON: with M8001 (STOP flag) set the pins are driven low (BSRR upper half),
  * with M8000 set (RUN) they are driven high, and a STOP request gates them low at once. */
 PLC_8BIT(Addr_Y)=0xFF; PLC_8BIT(Addr_M8)=0x0A;
 PLC_IO_Refresh(); assert(gpio_b.BSRR == (0x7E3U<<16));
 PLC_8BIT(Addr_M8)=0x09; PLC_IO_Refresh(); assert(gpio_b.BSRR == 0x7E3U);
 requested_run=0; PLC_IO_Refresh(); assert(gpio_b.BSRR == (0x7E3U<<16));
 requested_run=1;
 /* Y0 and Y7 on: PB0 and PB10 are set (0x401), the other six pins are reset (0x3E2). */
 PLC_8BIT(Addr_Y)=0x81; PLC_IO_Refresh();
 assert(gpio_b.BSRR == (0x401U | (0x3E2U<<16)));
 /* M8000 and M8001 both set is not a valid RUN state: the outputs stay off. */
 PLC_8BIT(Addr_M8)=0x0B; PLC_IO_Refresh(); assert(gpio_b.BSRR == (0x7E3U<<16));
 puts("PASS: GPIO mapping, filtering, scan snapshot and STOP gating (modeled registers)");
}
'''

def main():
    """Build the test in a temporary directory and run it."""
    with tempfile.TemporaryDirectory(prefix='plc-io-') as d:
        temp = Path(d)
        # Copy the real project headers, except platform.h, which the model above replaces.
        for p in (ROOT/'inc').glob('*'):
            if p.is_file() and p.name != 'platform.h': shutil.copyfile(p, temp/p.name)
        (temp/'platform.h').write_text(PLATFORM)
        (temp/'test.c').write_text(TEST)
        # src/plc_io.c is compiled unchanged, together with the test and with the sanitizers enabled.
        subprocess.run(['gcc', '-std=c99', '-D_GNU_SOURCE', '-Wall', '-Wextra',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                        '-I'+str(temp), '-x', 'c', str(ROOT/'src/plc_io.c'),
                        str(temp/'test.c'), '-o', str(temp/'test')], check=True)
        subprocess.run([str(temp/'test')], check=True)

if __name__ == '__main__': main()
