#!/usr/bin/env python3
"""Run selected experimental full-dispatch scans under ASan/UBSan.

Passing smoke fixtures do not establish correctness of untested legacy bodies.
Compiler warnings are intentionally visible: this is unvalidated legacy code.
"""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='plc-experimental-') as directory:
    exe=Path(directory)/'experimental'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-D_GNU_SOURCE','-O1','-g',
      '-fno-pie','-no-pie','-Wall','-Wextra','-Wno-int-to-pointer-cast',
      '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',
      '-I',str(ROOT/'tools/tests/runtime'),'-I',str(ROOT/'inc'),
      str(ROOT/'tools/tests/runtime/test_experimental.c'),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
