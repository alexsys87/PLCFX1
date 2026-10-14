#!/usr/bin/env python3
"""Compile/link a GNU diagnostic image; NOT an IAR build or flashable firmware.

Requires arm-none-eabi-gcc with newlib. The temporary vector adapter is derived
from the IAR startup table. It checks symbols and the C8 memory budget, but does
not replace IAR's runtime initialization or validate IAR assembler syntax.
"""
from pathlib import Path
import argparse
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LINKER = '''
ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 48K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 7K
  STEPS (rw) : ORIGIN = 0x20001C00, LENGTH = 1K
  PLC (rw) : ORIGIN = 0x20002000, LENGTH = 12K
  PROGRAM (rx) : ORIGIN = 0x0800C000, LENGTH = 14K
}
SECTIONS {
  .vectors : { KEEP(*(.vectors)) } > FLASH
  .text : { *(.text*) *(.rodata*) } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  .data : { *(.data*) } > RAM AT> FLASH
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
  .stack (NOLOAD) : { . = ALIGN(8); . += 2048; } > RAM
  .plc_steps (NOLOAD) : { KEEP(*(.plc_steps)) } > STEPS
  .plc_ram (NOLOAD) : { KEEP(*(.plc_ram)) } > PLC
  .plc_rom : { KEEP(*(.plc_rom)) } > PROGRAM
  ASSERT(SIZEOF(.plc_rom) == 110, "Unexpected PLC ROM header size")
  ASSERT(SIZEOF(.plc_steps) == 1024, "Wrong pulse-state reservation")
  ASSERT(SIZEOF(.plc_ram) == 12288, "Wrong PLC RAM reservation")
}
'''

def run(args):
    subprocess.run([str(x) for x in args], check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--optimization', default='-Os', choices=['-Os', '-O1', '-O2'])
    parser.add_argument('--runtime', choices=['minimal', 'experimental'], default='minimal')
    args = parser.parse_args()
    compiler = shutil.which('arm-none-eabi-gcc')
    if not compiler:
        raise SystemExit('Install gcc-arm-none-eabi and newlib first.')
    startup = (ROOT / 'CMSIS/device/startup_stm32f103xb.s').read_text()
    vectors = re.findall(r'^\s+DCD\s+(\S+)', startup, re.M)
    if len(vectors) != 59:
        raise SystemExit(f'Unexpected STM32F103xB vector count: {len(vectors)}')
    handlers = sorted(set(vectors[1:]) - {'0', 'Reset_Handler'})
    assembly = '.syntax unified\n.cpu cortex-m3\n.thumb\n.section .vectors,"a"\n'
    assembly += '.word 0x20001C00\n'
    assembly += ''.join('.word '+x+'\n' for x in vectors[1:])
    assembly += '.section .text\n.global Reset_Handler\n.thumb_func\nReset_Handler:\nbl SystemInit\nbl main\nb .\n'
    assembly += '.thumb_func\nDefault_Handler:\nb .\n'
    for name in handlers:
        assembly += f'.weak {name}\n.thumb_set {name},Default_Handler\n'
    with tempfile.TemporaryDirectory(prefix='plc-arm-check-') as directory:
        build = Path(directory)
        (build / 'vectors.s').write_text(assembly)
        (build / 'memory.ld').write_text(LINKER)
        flags = ['-mcpu=cortex-m3', '-mthumb', args.optimization,
                 '-ffunction-sections', '-fdata-sections', '-DSTM32F103xB',
                 '-I'+str(ROOT/'inc'), '-I'+str(ROOT/'CMSIS'), '-I'+str(ROOT/'CMSIS/device')]
        flags += ['-DPLC_RUNTIME_EXPERIMENTAL=' + str(int(args.runtime == 'experimental'))]
        # Every module is compiled in both profiles; the modules of the other profile are empty units.
        sources = sorted([*ROOT.glob('App/*.c'), *ROOT.glob('src/*.c'),
                          *ROOT.glob('CMSIS/device/*.c')])
        objects = []
        for i, source in enumerate(sources):
            obj = build / f'{i}.o'
            run([compiler, *flags, '-x', 'c', '-std=c99', '-Wall', '-Wextra',
                 '-c', source, '-o', obj])
            objects.append(obj)
        vector_obj = build / 'vectors.o'
        run([compiler, *flags, '-c', build/'vectors.s', '-o', vector_obj])
        image = build / 'diagnostic.elf'
        run([compiler, *flags, '-nostartfiles', '-Wl,--gc-sections',
             '-Wl,-Map='+str(build/'diagnostic.map'), '-T', build/'memory.ld',
             *objects, vector_obj, '-lm', '-lc', '-lgcc', '-o', image])
        run(['arm-none-eabi-size', '-A', image])
        print(f'PASS: {len(sources)} C files compiled and linked ({args.optimization}); '
              '48 KB code Flash / 7 KB RAM total (including 2 KB stack) checked.')
        print('Diagnostic image deleted; build and flash with IAR, then test on hardware.')

if __name__ == '__main__':
    main()
