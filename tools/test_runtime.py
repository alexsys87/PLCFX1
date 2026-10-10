#!/usr/bin/env python3
"""Run complete scans of the real ladder.c using guarded fixed-address RAM."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="plc-runtime-") as directory:
    exe = Path(directory) / "runtime"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-D_GNU_SOURCE", "-O1", "-g",
        "-fno-pie", "-no-pie", "-Wall", "-Wextra", "-Werror",
        "-Wno-int-to-pointer-cast", "-fsanitize=address,undefined",
        "-fno-sanitize-recover=all", "-fno-omit-frame-pointer",
        "-I", str(ROOT / "tools/tests/runtime"), "-I", str(ROOT / "inc"),
        str(ROOT / "tools/tests/runtime/test_runtime.c"), "-o", str(exe)
    ], check=True)
    subprocess.run([str(exe)], check=True)
