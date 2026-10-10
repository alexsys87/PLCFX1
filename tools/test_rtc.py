#!/usr/bin/env python3
"""Run RTC regression tests without Tasklet or third-party Python packages.

Requires Python 3 and a host C compiler (CC, cc, gcc, or clang). Optionally
compiles the unchanged production source with arm-none-eabi-gcc if installed.
Run from any directory: python3 tools/test_rtc.py [--sanitize] [--skip-arm]

The host harness substitutes peripheral storage and injects deterministic
status transitions into a temporary copy of rtc_wait. Production files are
never modified. Hardware timing, backup reset side effects, interrupts and
counter-read rollover races are not simulated; use hardware for those checks.
"""

import argparse
import datetime
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


def run(command, **kwargs):
    print("+", shlex.join(map(str, command)), flush=True)
    return subprocess.run(command, check=True, timeout=120, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true",
                        help="enable host AddressSanitizer and UBSan (compiler support required)")
    parser.add_argument("--skip-arm", action="store_true",
                        help="skip optional Cortex-M3 compile check")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    fixtures = root / "tools" / "fixtures" / "rtc"
    source = (root / "src" / "RTC.C").read_text()
    compiler = shlex.split(os.environ.get("CC", ""))
    if not compiler:
        compiler = [next((c for c in ("cc", "gcc", "clang") if shutil.which(c)), "")]
    if not compiler[0] or not shutil.which(compiler[0]):
        parser.error("a host C compiler is required; set CC to its executable")

    # Fail rather than silently running stale fixtures if production changes.
    needle = "while (limit-- != 0U) {"
    if source.count(needle) != 1:
        raise RuntimeError("rtc_wait changed: review the status-transition injection")
    host_source = re.sub(r"^#include[^\n]*\n", "", source, flags=re.MULTILINE)
    host_source = host_source.replace(needle,
        "status_begin(reg, mask); " + needle + " status_tick(reg, mask);")

    with tempfile.TemporaryDirectory(prefix="rtc-regression-") as temporary:
        work = Path(temporary)
        shutil.copyfile(fixtures / "main.h", work / "main.h")
        (work / "PLC_Dialogue.h").write_text("")
        (work / "PLC_CONF.H").write_text("")
        harness = ((fixtures / "harness_prefix.c").read_text() + host_source +
                   (fixtures / "harness_tests.c").read_text())
        (work / "test.c").write_text(harness)
        includes = ["-I" + str(work), "-I" + str(root / "CMSIS" / "device"),
                    "-I" + str(root / "CMSIS")]
        executable = work / ("test.exe" if os.name == "nt" else "test")
        flags = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
                 "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast"]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        run(compiler + flags + includes + [str(work / "test.c"), "-o", str(executable)])
        # Independent Gregorian oracle: Python datetime, not the RTC helpers.
        oracle = work / "calendar.txt"
        epoch = datetime.datetime(2016, 1, 1)
        date = epoch
        with oracle.open("w") as output:
            while date.year <= 2136:
                instant = date.replace(hour=23, minute=59, second=59)
                seconds = int((instant - epoch).total_seconds())
                weekday = (instant.weekday() + 1) % 7
                output.write(f"{date.year} {date.month} {date.day} 23 59 59 {seconds} {weekday}\n")
                date += datetime.timedelta(days=1)
        with oracle.open() as stdin:
            run([str(executable)], stdin=stdin)

        arm = shutil.which("arm-none-eabi-gcc")
        if arm and not args.skip_arm:
            # Real migrated project headers; no stubs and no instrumentation.
            run([arm, "-x", "c", "-std=c99", "-mcpu=cortex-m3", "-mthumb",
                 "-DSTM32F103xB", "-Wall", "-Wextra", "-Werror",
                 "-I" + str(root / "inc"), "-I" + str(root / "CMSIS" / "device"),
                 "-I" + str(root / "CMSIS"), "-c", str(root / "src" / "RTC.C"),
                 "-o", str(work / "rtc.o")])
            print("PASS: unchanged production RTC.C compiled for Cortex-M3 with project headers")
        else:
            print("SKIP: optional ARM compile (--skip-arm or arm-none-eabi-gcc not installed)")


if __name__ == "__main__":
    main()
