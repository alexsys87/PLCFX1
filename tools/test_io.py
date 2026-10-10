#!/usr/bin/env python3
"""Run the actual board I/O implementation against modeled CMSIS registers."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = '''
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int16_t s16; typedef int32_t s32;
typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR; } MockGPIO;
typedef struct { volatile uint32_t APB2ENR; } MockRCC;
extern MockGPIO gpio_a, gpio_b;
extern MockRCC rcc;
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define RCC (&rcc)
#define RCC_APB2ENR_IOPAEN (1U<<2)
#define RCC_APB2ENR_IOPBEN (1U<<3)
#define SRAM_BASE 0x20000000UL
#endif
'''
TEST = '''
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include "platform.h"
#include "PLC_Dialogue.h"
MockGPIO gpio_a, gpio_b;
MockRCC rcc;
static u8 requested_run = 1U;
u8 PLC_Runtime_RunRequested(void) { return requested_run; }
extern u16 X_DIY;
extern void PLC_IO_config(void), PLC_IO_Refresh(void), X_filter(void), PLC_IO_SampleInputs(void);
int main(void) {
 assert(mmap((void*)SRAM_BASE, 20*1024, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) == (void*)SRAM_BASE);
 PLC_IO_config();
 assert(gpio_a.CRL == 0x88888888U);
 assert(gpio_a.BRR == 0xFFU);
 assert(gpio_b.BRR == 0x7E3U);
 assert((gpio_b.CRL & 0xFFF000FFU) == 0x22200022U);
 assert((gpio_b.CRH & 0xFFFU) == 0x222U);
 gpio_a.IDR = 0x81;
 for (int i=0;i<9;i++) X_filter();
 PLC_IO_SampleInputs(); assert(PLC_8BIT(Addr_X) == 0);
 X_filter(); assert(PLC_8BIT(Addr_X) == 0); /* IRQ does not alter scan image. */
 PLC_IO_SampleInputs(); assert(PLC_8BIT(Addr_X) == 0x81);
 gpio_a.IDR = 0; for (int i=0;i<10;i++) X_filter();
 PLC_IO_SampleInputs(); assert(PLC_8BIT(Addr_X) == 0);
 X_DIY=0; gpio_a.IDR=0xFF; X_filter(); PLC_IO_SampleInputs();
 assert(PLC_8BIT(Addr_X) == 0xFF);
 PLC_8BIT(Addr_Y)=0xFF; PLC_8BIT(Addr_M8)=0x0A;
 PLC_IO_Refresh(); assert(gpio_b.BSRR == (0x7E3U<<16));
 PLC_8BIT(Addr_M8)=0x09; PLC_IO_Refresh(); assert(gpio_b.BSRR == 0x7E3U);
 requested_run=0; PLC_IO_Refresh(); assert(gpio_b.BSRR == (0x7E3U<<16));
 requested_run=1;
 PLC_8BIT(Addr_Y)=0x81; PLC_IO_Refresh();
 assert(gpio_b.BSRR == (0x401U | (0x3E2U<<16)));
 PLC_8BIT(Addr_M8)=0x0B; PLC_IO_Refresh(); assert(gpio_b.BSRR == (0x7E3U<<16));
 puts("PASS: GPIO mapping, filtering, scan snapshot and STOP gating (modeled registers)");
}
'''

def main():
    with tempfile.TemporaryDirectory(prefix='plc-io-') as d:
        temp = Path(d)
        for p in (ROOT/'inc').glob('*'):
            if p.is_file() and p.name != 'platform.h': shutil.copyfile(p, temp/p.name)
        (temp/'platform.h').write_text(PLATFORM)
        (temp/'test.c').write_text(TEST)
        subprocess.run(['gcc', '-std=c99', '-D_GNU_SOURCE', '-Wall', '-Wextra',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
                        '-I'+str(temp), '-x', 'c', str(ROOT/'src/PLC_IO.C'),
                        str(temp/'test.c'), '-o', str(temp/'test')], check=True)
        subprocess.run([str(temp/'test')], check=True)

if __name__ == '__main__': main()
