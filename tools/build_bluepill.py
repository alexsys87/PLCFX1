#!/usr/bin/env python3
"""Build a Cortex-M3 bare-metal image for STM32F103C8 (GNU Arm toolchain).

Uses the checked-in STM32F103xB vector list and a real C reset adapter.
This is a flashable build, not proof of hardware operation or an IAR build.
"""
import argparse
from pathlib import Path
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
 .data : { _sdata = .; *(.data*) _edata = .; } > RAM AT> FLASH
 _sidata = LOADADDR(.data);
 .bss (NOLOAD) : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; } > RAM
 .stack ORIGIN(RAM) + LENGTH(RAM) - 2048 (NOLOAD) : {
  _stack_bottom = .; . += 2048; _stack_top = .;
 } > RAM
 .plc_steps (NOLOAD) : { KEEP(*(.plc_steps)) } > STEPS
 .plc_ram (NOLOAD) : { KEEP(*(.plc_ram)) } > PLC
 .plc_rom : { KEEP(*(.plc_rom)) } > PROGRAM
 ASSERT(_ebss <= _stack_bottom, "Ordinary RAM overlaps stack")
 ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(FLASH) + LENGTH(FLASH), "Firmware exceeds 48K Flash")
 ASSERT(SIZEOF(.plc_rom) == 110, "Unexpected factory program header size")
 ASSERT(SIZEOF(.plc_steps) == 1024, "Wrong step state reservation")
 ASSERT(SIZEOF(.plc_ram) == 12288, "Wrong PLC RAM reservation")
}
'''
RESET = '''
#include <stdint.h>
extern uint8_t _sidata, _sdata, _edata, _sbss, _ebss;
extern void SystemInit(void);
extern int main(void);
void Reset_Handler(void) {
 /* Byte copies do not require aligned Flash LMA or a rounded data size. */
 uint8_t *src = &_sidata, *dst;
 for (dst = &_sdata; dst < &_edata;) *dst++ = *src++;
 for (dst = &_sbss; dst < &_ebss;) *dst++ = 0;
 SystemInit();
 (void)main();
 for (;;) {}
}
'''

def run(args):
    subprocess.run([str(x) for x in args], check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        help='Override profile-specific build/minimal or build/experimental directory.')
    parser.add_argument('--runtime', choices=['minimal', 'experimental'], default='minimal',
                        help='Experimental restores unvalidated legacy handlers; minimal is the default.')
    args = parser.parse_args()
    cc = shutil.which('arm-none-eabi-gcc')
    if not cc:
        raise SystemExit('Install gcc-arm-none-eabi and newlib first.')
    destination = (args.output or ROOT / 'build' / args.runtime).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    profile_marker = destination / '.runtime-profile'
    if profile_marker.exists() and profile_marker.read_text().strip() != args.runtime:
        raise SystemExit('Output directory contains another runtime profile; choose a different --output.')
    profile_marker.write_text(args.runtime + '\n')
    # Cache cloud-backed inputs/artifacts once. Compilers, objcopy and nm make
    # many small reads, which otherwise dominate runtime on remote storage.
    temporary = tempfile.TemporaryDirectory(prefix='plc-bluepill-build-')
    source_root = Path(temporary.name) / 'source'
    for folder in ('App', 'src', 'inc', 'CMSIS'):
        shutil.copytree(ROOT / folder, source_root / folder)
    build = Path(temporary.name) / 'output'
    build.mkdir()
    startup = (source_root / 'CMSIS/device/startup_stm32f103xb.s').read_text()
    vectors = re.findall(r'^\s+DCD\s+(\S+)', startup, re.M)
    if len(vectors) != 59:
        raise SystemExit('Unexpected STM32F103xB vector count')
    handlers = sorted(set(vectors[1:]) - {'0', 'Reset_Handler'})
    assembly = '.syntax unified\n.cpu cortex-m3\n.thumb\n.section .vectors,"a"\n'
    assembly += '.word _stack_top\n' + ''.join('.word '+x+'\n' for x in vectors[1:])
    assembly += '.section .text.Default_Handler,"ax"\n.thumb_func\nDefault_Handler:\nb .\n'
    for name in handlers:
        assembly += f'.weak {name}\n.thumb_set {name},Default_Handler\n'
    (build / 'vectors.S').write_text(assembly)
    (build / 'reset.c').write_text(RESET)
    (build / 'bluepill.ld').write_text(LINKER)
    flags = ['-mcpu=cortex-m3', '-mthumb', '-Os', '-ffunction-sections',
             '-fdata-sections', '-DSTM32F103xB', '-I'+str(source_root/'inc'),
             '-I'+str(source_root/'CMSIS'), '-I'+str(source_root/'CMSIS/device')]
    flags += ['-DPLC_RUNTIME_EXPERIMENTAL=' + str(int(args.runtime == 'experimental'))]
    if args.runtime == 'experimental':
        print('WARNING: experimental legacy instruction profile; not validated or safe for loads.', flush=True)
    sources = sorted([*source_root.glob('App/*.c'), *source_root.glob('src/*.c'),
                      *source_root.glob('src/*.C'), *source_root.glob('CMSIS/device/*.c')])
    sources += [build / 'reset.c']
    objects = []
    for i, source in enumerate(sources):
        obj = build / f'{i}.o'
        run([cc, *flags, '-x', 'c', '-std=c99', '-Wall', '-Wextra', '-c', source, '-o', obj])
        objects.append(obj)
    vector = build / 'vectors.o'
    run([cc, *flags, '-c', build/'vectors.S', '-o', vector])
    image = build / 'plc_fx1n_bluepill.elf'
    run([cc, *flags, '-nostartfiles', '-Wl,--gc-sections',
         '-Wl,-Map='+str(build/'plc_fx1n_bluepill.map'), '-T', build/'bluepill.ld',
         *objects, vector, '-lm', '-lc', '-lgcc', '-o', image])
    for fmt, suffix in [('ihex', 'hex'), ('binary', 'bin')]:
        run(['arm-none-eabi-objcopy', '-O', fmt, image, build/f'plc_fx1n_bluepill.{suffix}'])
    run(['arm-none-eabi-size', '-A', image])
    sections = subprocess.check_output(['arm-none-eabi-size', '-A', str(image)], text=True)
    sizes = {name: int(size) for name, size in re.findall(r'^(\.\S+)\s+(\d+)\s+\d+', sections, re.M)}
    symbol_text = subprocess.check_output(['arm-none-eabi-nm', '-n', str(image)], text=True)
    symbols = {name: int(address, 16) for address, name in
               re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\S+)$', symbol_text, re.M)}
    static_payload = sizes.get('.data', 0) + sizes.get('.bss', 0)
    static_span = symbols['_ebss'] - 0x20000000
    code_flash = symbols['_sidata'] + sizes.get('.data', 0) - 0x08000000
    slack = symbols['_stack_bottom'] - symbols['_ebss']
    print(f'Memory: {code_flash}/49152 allocated code Flash bytes; {static_payload} static ordinary RAM '
          f'payload ({static_span} including padding) + 2048 reserved stack; {slack} bytes static slack.')
    print('Stack high-water is NOT proven; PLC image 12288 bytes + pulse bitmap 1024 bytes; 6000 steps.')
    for name in ('plc_fx1n_bluepill.elf', 'plc_fx1n_bluepill.hex', 'plc_fx1n_bluepill.bin',
                 'plc_fx1n_bluepill.map', 'bluepill.ld', 'vectors.S', 'reset.c'):
        shutil.copyfile(build / name, destination / name)
    temporary.cleanup()
    print('PASS: flashable GNU Arm image built; hardware operation remains unverified.')
    print('Runtime profile:', args.runtime)
    print('Outputs:', destination)
    print('Program at 0x08000000 with ST-Link/SWD. Factory image replaces PLC header/program.')

if __name__ == '__main__':
    main()
