#!/usr/bin/env python3
"""Compile and execute real PLC_Com.c with fixed-address host memory and stubs.

No target files are generated. Requires Linux mmap and a C compiler; defaults
to cc. Set CC to choose another compiler. Flash timing/physics are not simulated.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="plc-protocol-") as temporary:
    exe = Path(temporary) / "protocol"
    subprocess.run([
        os.environ.get("CC", "cc"), "-x", "c", "-std=c11", "-D_GNU_SOURCE",
        "-g", "-O1", "-fno-pie", "-no-pie", "-Wall", "-Wextra",
        "-Wno-int-to-pointer-cast", "-fsanitize=address,undefined",
        "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
        "-I", str(ROOT / "tools/tests/protocol"), "-I", str(ROOT / "inc"),
        str(ROOT / "tools/tests/protocol/test_protocol.c"), "-o", str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True)
