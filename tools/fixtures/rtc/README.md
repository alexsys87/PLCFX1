# RTC test fixtures

Run from the repository root:

```sh
python3 tools/test_rtc.py
python3 tools/test_rtc.py --sanitize  # optional GCC/Clang sanitizers
python3 tools/test_rtc.py --skip-arm  # host tests only
```

Requirements: Python 3 standard library and a GCC-compatible host C compiler
(`cc`, `gcc`, `clang`, or the `CC` environment variable). If available,
`arm-none-eabi-gcc` also compiles unchanged `src/RTC.C` with the real project
headers. Temporary files are automatically removed. No Tasklet paths, network,
external Python modules, or ARM compiler are required for host testing.

`main.h` maps the PLC date words into volatile mock storage and uses the
repository's CMSIS device definitions. `harness_prefix.c` supplies mock
registers and deterministic RTC/LSE status transitions. `harness_tests.c`
contains assertions. The runner injects transitions only into a temporary
copy of `rtc_wait`; production files are not modified.

The Python runner generates independent Gregorian expectations for every
date in 2016–2136 (44,195 dates). Host tests also cover epoch initialization,
leap/century rules, invalid date/time writes, wide PLC month rejection,
retained counter/prescaler behavior, initial and completion write timeouts,
LSE and synchronization timeouts, missing LSE on first and retained startup,
no LSI fallback, M8018 status, and backup write-access restoration.

Limitations: this is a register **model**, not a hardware emulator. It does
not reproduce backup-domain reset side effects, calibrated timeout duration,
crystal startup, interrupts, or split-counter rollover between register
reads. Host PLC addressing is mocked; optional ARM compilation checks the
actual PLC headers. IAR compilation and physical-board startup still require
their respective environments.
