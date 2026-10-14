#!/usr/bin/env python3
"""Run the experimental instruction-layer tests under ASan/UBSan.

Known-answer tests for every implemented handler, exhaustive contact truth tables,
operand/special-relay rules and a fixed-seed per-slot fuzzer. The build is -Werror:
the layer is expected to compile warning-free. Host-model results only; they are not
hardware measurements or GX Developer interoperability evidence.
"""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='plc-experimental-') as directory:
    exe=Path(directory)/'experimental'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-D_GNU_SOURCE','-O1','-g',
      '-fno-pie','-no-pie','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast',
      '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',
      '-DPLC_RUNTIME_EXPERIMENTAL=1',
      '-I',str(ROOT/'tools/tests/runtime'),'-I',str(ROOT/'inc'),
      str(ROOT/'tools/tests/runtime/test_experimental.c'),
      *sorted(str(p) for p in (ROOT/'src').glob('plc_exp_*.c')),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
