#!/usr/bin/env python3
"""Compile and execute real plc_serial_protocol.c with fixed-address host memory and stubs.

No target files are generated. Requires Linux mmap and a C compiler; defaults
to cc. Set CC to choose another compiler. Flash timing/physics are not simulated.
"""
import os
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime', choices=['minimal', 'experimental'], default='minimal')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="plc-protocol-") as temporary:
    exe = Path(temporary) / "protocol"
    compile_command = [
        os.environ.get("CC", "cc"), "-x", "c", "-std=c11", "-D_GNU_SOURCE",
        "-g", "-O1", "-fno-pie", "-no-pie", "-Wall", "-Wextra",
        "-Wno-int-to-pointer-cast", "-fsanitize=address,undefined",
        "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
        "-I", str(ROOT / "tools/tests/protocol"), "-I", str(ROOT / "inc"),
        str(ROOT / "tools/tests/protocol/test_protocol.c"), "-o", str(exe),
    ]
    subprocess.run(compile_command, check=True)
    subprocess.run([str(exe)], check=True)
    subprocess.run([os.environ.get("PYTHON", "python3"),
                    str(ROOT / "tools/tests/protocol/test_link.py"), str(exe)], check=True)
    # Compile the SAME wire fixture against the actual bounded ladder runtime.
    # The experimental interpreter is a set of separately compiled modules (src/plc_exp_*.c).
    runtime_modules = sorted(str(p) for p in (ROOT / "src").glob("plc_exp_*.c")) \
        if args.runtime == 'experimental' else []
    subprocess.run(compile_command + ["-DTEST_WITH_RUNTIME",
      "-DPLC_RUNTIME_EXPERIMENTAL=" + str(int(args.runtime == 'experimental'))] + runtime_modules + ["-lm"], check=True)
    subprocess.run([str(exe), "--integration"], check=True)
    subprocess.run([os.environ.get("PYTHON", "python3"),
                    str(ROOT / "tools/tests/protocol/test_link.py"), str(exe), "--runtime"], check=True)
    print('PASS: wire/runtime integration profile:', args.runtime)
