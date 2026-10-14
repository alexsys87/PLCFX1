#!/usr/bin/env python3
"""Send a battery of faulty requests to the PLC (or the emulator) and check the replies.

Run it against the host end of the virtual null-modem pair while the emulator runs on
the other end. Every scenario must be answered the way the firmware is specified to
answer it (mostly NAK). Afterwards the emulator log must contain one ERROR/WARN entry
per scenario with a decoded reason, which is what this script is meant to exercise.

    python emulator/tools/error_scenarios.py --port COM6

The last scenario uploads a program with an unsupported instruction word and requests
RUN: the PLC must stay in STOP and the emulator logs the PLC fault with its step.
The factory END-only program is restored at the end.
"""
import argparse
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from plc_link import ACK, ENQ, ETX, NAK, STX, LinkError, PLCLink, frame  # noqa: E402


def raw_frame(command, payload_text, checksum_text=None):
    """STX command payload ETX checksum, with an optional deliberately wrong checksum."""
    body = command.encode("ascii") + payload_text.encode("ascii") + bytes([ETX])
    checksum = checksum_text if checksum_text is not None else f"{sum(body) & 255:02X}"
    return bytes([STX]) + body + checksum.encode("ascii")


def scenarios():
    """(name, request bytes, expected first reply byte or None for no reply)."""
    good_read = frame("0", bytes([0x00, 0x00, 0x01]))
    return [
        ("bad checksum", raw_frame("0", "000001", "00"), NAK),
        ("non-hex checksum digits", raw_frame("0", "000001", "ZZ"), NAK),
        ("unknown command 'Z'", raw_frame("Z", "00"), NAK),
        ("unknown extended command E99", frame("E", bytes([0x99])), NAK),
        ("odd number of payload digits", raw_frame("0", "00001"), NAK),
        ("non-hex payload", raw_frame("0", "00G001"), NAK),
        ("read beyond the PLC image", frame("0", bytes([0x2F, 0xFF, 0x10])), NAK),
        ("read longer than 70 bytes", frame("0", bytes([0x00, 0x00, 0x50])), NAK),
        ("write to protected M8000..M8007 byte", frame("1", bytes([0x01, 0xC0, 0x01, 0x00])), NAK),
        ("write length mismatch", frame("1", bytes([0x00, 0x00, 0x02, 0xAA])), NAK),
        ("force protected bit M8000", raw_frame("E", "7000E"), NAK),
        ("E10 into program memory", frame("E", bytes([0x10, 0x80, 0x5C, 0x02, 0x0F, 0x00])), NAK),
        ("E11 odd program address", frame("E", bytes([0x11, 0x80, 0x5D, 0x02, 0x0F, 0x00])), NAK),
        ("E11 beyond the program area", frame("E", bytes([0x11, 0xB7, 0xFE, 0x04, 0, 0, 0, 0])), NAK),
        ("E01 beyond the program area", frame("E", bytes([0x01, 0xB8, 0x00, 0x02])), NAK),
        ("oversized frame", bytes([STX]) + b"1" + b"00" * 80 + bytes([ETX]) + b"00", NAK),
        ("noise before a valid request", b"\xFF\x10garbage" + good_read, STX),
        ("frame restarted by STX", bytes([STX]) + b"0000" + good_read, STX),
        ("ENQ link test", bytes([ENQ]), ACK),
    ]


def exchange_raw(port, request, timeout=1.0):
    port.reset_input_buffer()
    port.write(request)
    port.flush()
    port.timeout = timeout
    reply = port.read(1)
    if reply and reply[0] == STX:
        # Swallow the rest of a data frame (up to ETX plus two checksum digits).
        rest = bytearray()
        while not rest.endswith(bytes([ETX])) and len(rest) < 200:
            part = port.read(1)
            if not part:
                break
            rest += part
        port.read(2)
    return reply[0] if reply else None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="host end of the null-modem pair (COM6, /dev/pts/3, ...)")
    parser.add_argument("--baud", type=int, default=19200)
    args = parser.parse_args(argv)
    try:
        import serial
    except ImportError:
        parser.error("Install pyserial: python -m pip install pyserial")

    failures = 0
    with serial.Serial(args.port, args.baud, timeout=1.0, write_timeout=2.0) as port:
        for name, request, expected in scenarios():
            got = exchange_raw(port, request)
            ok = got == expected
            failures += not ok
            shown = {None: "no reply", ACK: "ACK", NAK: "NAK", STX: "data"}.get(got, f"0x{got:02X}")
            wanted = {None: "no reply", ACK: "ACK", NAK: "NAK", STX: "data"}[expected]
            print(f"[{'PASS' if ok else 'FAIL'}] {name}: {shown} (expected {wanted})")
            time.sleep(0.05)

        link = PLCLink(port)
        port.timeout = 3.0
        print("Uploading a program with an unsupported instruction word (FFFF FFFF 000F) ...")
        try:
            link.connect()
            link.upload(bytes.fromhex("FF FF FF FF 0F 00"))
            try:
                link.run()
                print("[FAIL] RUN accepted an invalid program")
                failures += 1
            except LinkError:
                print("[PASS] RUN refused, the PLC stays in STOP (fault logged by the emulator)")
            link.stop()
            print("Restoring the END-only program ...")
            link.exchange(frame("E", bytes([0x60])))
        except (LinkError, OSError) as error:
            print(f"[FAIL] program scenario: {error}")
            failures += 1

    print(f"\n{failures} failure(s). Check the emulator log for one entry per scenario.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
