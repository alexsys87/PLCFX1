#!/usr/bin/env python3
"""Conservative serial smoke client for this project's inherited FX framing.

Not a general Mitsubishi programming tool. Only raw project-runtime bytecode
is accepted; GX Developer/GX Works2 project files are not converted.
"""
import argparse
from pathlib import Path
import struct
import sys
import time

STX, ETX, ENQ, ACK, NAK = 2, 3, 5, 6, 21
PROGRAM_START = 0x805C
MAX_WORDS = 6000
READ_CHUNK = 64
WRITE_CHUNK = 60  # E11 header + payload fits the firmware's 144-byte RX array.


class LinkError(Exception):
    """A rejected, malformed or incomplete transaction."""


def frame(command, payload=b"", *, ascii_payload=None):
    text = payload.hex().upper().encode("ascii") if ascii_payload is None else ascii_payload
    body = command.encode("ascii") + text + bytes([ETX])
    return bytes([STX]) + body + f"{sum(body) & 255:02X}".encode("ascii")


class PLCLink:
    def __init__(self, serial_port):
        self.port = serial_port

    def _exact(self, count):
        result = bytearray()
        while len(result) < count:
            part = self.port.read(count - len(result))
            if not part:
                raise LinkError(f"Serial timeout after {len(result)}/{count} response bytes")
            result.extend(part)
        return bytes(result)

    def exchange(self, request, expected=None):
        """One outstanding request only. Never retry writes or auto-change baud."""
        if self.port.write(request) != len(request):
            raise LinkError("Incomplete serial write")
        self.port.flush()
        first = self._exact(1)[0]
        if first == NAK:
            raise LinkError("PLC returned NAK: invalid command/range or Flash failure")
        if expected is None:
            if first != ACK:
                raise LinkError(f"Expected ACK, received 0x{first:02X}")
            return b""
        if first != STX:
            raise LinkError(f"Expected STX data response, received 0x{first:02X}")
        rest = self._exact(expected * 2 + 3)
        if rest[-3] != ETX:
            raise LinkError("Invalid response terminator")
        try:
            checksum = int(rest[-2:], 16)
            text = rest[:-3].decode("ascii")
            if any(c not in "0123456789abcdefABCDEF" for c in text):
                raise ValueError("non-hex data")
            data = bytes.fromhex(text)
        except (ValueError, UnicodeError) as error:
            raise LinkError("Invalid hexadecimal response") from error
        if (sum(rest[:-2]) & 255) != checksum or len(data) != expected:
            raise LinkError("Response checksum/length mismatch")
        return data

    def connect(self):
        self.port.reset_input_buffer()
        self.exchange(bytes([ENQ]))
        return self.identify()

    def identify(self):
        model, capacity = struct.unpack("<HH", self.read_ram(0x0E02, 4))
        state = self.read_ram(0x01C0, 1)[0]
        return {"model_code": f"0x{model:04X}", "capacity_words": capacity * 1000,
                "state": "RUN" if state & 1 else "STOP"}

    def read_ram(self, address, count):
        if not (0 <= address <= 0x3000 and 0 <= count <= 0x3000 - address):
            raise ValueError("RAM range exceeds the C8 project's 12 KB PLC workspace")
        result = bytearray()
        while count:
            size = min(count, READ_CHUNK)
            result.extend(self.exchange(frame("0", struct.pack(">HB", address, size)), size))
            address += size
            count -= size
        return bytes(result)

    def read_program(self, address, count):
        if not (0x8000 <= address <= 0xB800 and 0 <= count <= 0xB800 - address):
            raise ValueError("Program range exceeds the 14 KB Flash partition")
        result = bytearray()
        while count:
            size = min(count, READ_CHUNK)
            payload = bytes([1]) + struct.pack(">HB", address, size)
            result.extend(self.exchange(frame("E", payload), size))
            address += size
            count -= size
        return bytes(result)

    def stop(self):
        self.exchange(frame("E", ascii_payload=b"7250E"))
        self._wait_state("STOP")

    def run(self):
        self.exchange(frame("E", ascii_payload=b"7240E"))
        self._wait_state("RUN")

    def _wait_state(self, expected):
        # ACK accepts an IRQ mailbox request, not the eventual runtime state.
        until = time.monotonic() + 1.0
        while True:
            time.sleep(0.02)
            if self.identify()["state"] == expected:
                return
            if time.monotonic() >= until:
                raise LinkError(f"{expected} readback failed; runtime may have rejected the program")

    def upload(self, program):
        if not program or len(program) % 2 or len(program) > MAX_WORDS * 2:
            raise ValueError("Program must contain 1..6000 little-endian 16-bit words")
        if program[-2:] != b"\x0f\x00":
            raise ValueError("Program must end with the runtime END word 0x000F")
        self.stop()
        advertised = self.identify()["capacity_words"]
        if not (0 < advertised <= MAX_WORDS) or len(program) > advertised * 2:
            raise LinkError(f"Inconsistent/insufficient reported capacity: {advertised} words")
        # Abort old volatile staging, retain parameter/header bytes, replace body.
        self.exchange(frame("E", bytes([0x61])))
        for offset in range(0, len(program), WRITE_CHUNK):
            part = program[offset:offset + WRITE_CHUNK]
            payload = bytes([0x11]) + struct.pack(">HB", PROGRAM_START + offset, len(part)) + part
            self.exchange(frame("E", payload))
        self.exchange(frame("E", bytes([0x42])))
        actual = self.read_program(PROGRAM_START, len(program))
        if actual != program:
            raise LinkError("Program Flash readback differs from uploaded bytes; do not RUN")
        return actual

    def monitor(self):
        m = self.read_ram(0, 1)[0]
        special = int.from_bytes(self.read_ram(0x01C0, 2), "little")
        y = self.read_ram(0x0180, 1)[0]
        return {"RUN": bool(special & 1), "M0": bool(m & 1),
                "M8000": bool(special & 1), "M8013": bool(special & (1 << 13)),
                "Y0": bool(y & 1)}

    def clear_smoke_m0(self):
        """Clear only M0 while STOP so a stale internal coil cannot pass smoke."""
        value = self.read_ram(0, 1)[0] & 0xFE
        self.exchange(frame("1", b"\x00\x00\x01" + bytes([value])))


def smoke_expectation(program, requested="auto"):
    if requested != "auto":
        return requested
    if program == bytes.fromhex("C2 01 00 8E 00 C0 0F 00"):
        return "m0"
    if program == bytes.fromhex("C2 01 0D 8E 00 CC 0F 00"):
        return "clock-y0"
    raise ValueError("Unknown smoke program: select --expect m0, clock-y0, or none explicitly")


def assert_smoke_samples(samples, expectation):
    if any(not sample["RUN"] for sample in samples):
        raise LinkError("Smoke failed: PLC left RUN during monitoring")
    if expectation == "m0" and (not samples or not all(sample["M0"] for sample in samples)):
        raise LinkError("Smoke failed: LD M8000 OUT M0 did not set M0")
    if expectation == "clock-y0":
        if {sample["M8013"] for sample in samples} != {False, True}:
            raise LinkError("Smoke failed: M8013 did not show both clock phases (use --seconds 3)")
        if {sample["Y0"] for sample in samples} != {False, True}:
            raise LinkError("Smoke failed: Y0 did not change with the clock")
        # Two reads can straddle a real clock boundary. Require successful
        # observations of BOTH matching phases, not equality at every sample.
        if {sample["Y0"] for sample in samples if sample["Y0"] == sample["M8013"]} != {False, True}:
            raise LinkError("Smoke failed: Y0 did not track both M8013 clock phases")


def load_program(path):
    source = Path(path)
    if source.suffix.lower() == ".hex":
        # Plain hexadecimal BYTES, not Intel HEX, executable ELF, or GX project.
        lines = [line.split("#", 1)[0] for line in source.read_text().splitlines()]
        return bytes.fromhex(" ".join(lines))
    return source.read_bytes()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="COM3, /dev/ttyUSB0, etc.")
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--trace", action="store_true", help="Print transmitted/received bytes")
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("identify", "stop", "run"):
        sub.add_parser(name)
    upload = sub.add_parser("upload")
    upload.add_argument("file", help="Raw .bin or plain byte .hex ending in END")
    upload.add_argument("--run", action="store_true", help="Explicitly RUN after verified upload")
    smoke = sub.add_parser("smoke", help="Upload, verify, RUN, monitor, finally STOP")
    smoke.add_argument("file")
    smoke.add_argument("--seconds", type=float, default=3.0)
    smoke.add_argument("--expect", choices=("auto", "m0", "clock-y0", "none"), default="auto",
                       help="Assert known example operation; unknown programs require an explicit choice")
    monitor = sub.add_parser("monitor")
    monitor.add_argument("--seconds", type=float, default=3.0)
    read = sub.add_parser("readback")
    read.add_argument("file", help="Destination raw binary")
    read.add_argument("--bytes", type=int, required=True)
    args = parser.parse_args(argv)
    try:
        import serial
    except ImportError:
        parser.error("Install pyserial: python -m pip install pyserial")
    try:
        with serial.Serial(args.port, 19200, bytesize=serial.EIGHTBITS,
                           parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE,
                           timeout=args.timeout, write_timeout=args.timeout,
                           xonxoff=False, rtscts=False, dsrdtr=False) as port:
            if args.trace:
                original_read, original_write = port.read, port.write
                def traced_read(count):
                    data = original_read(count)
                    print("RX", data.hex(" "), file=sys.stderr)
                    return data
                def traced_write(data):
                    print("TX", data.hex(" "), file=sys.stderr)
                    return original_write(data)
                port.read, port.write = traced_read, traced_write
            link = PLCLink(port)
            print("Connected:", link.connect())
            if args.command in ("upload", "smoke"):
                # Smoke always requests STOP even if RUN/monitor fails.
                try:
                    program = load_program(args.file)
                    expectation = smoke_expectation(program, args.expect) if args.command == "smoke" else None
                    actual = link.upload(program)
                    print(f"Verified {len(actual)} program bytes in Flash; STOP")
                    if expectation == "m0":
                        link.clear_smoke_m0()
                    if args.command == "smoke" or args.run:
                        link.run()
                        print("RUN confirmed")
                    if args.command == "smoke":
                        samples = []
                        until = time.monotonic() + max(0, args.seconds)
                        while time.monotonic() < until:
                            sample = link.monitor()
                            samples.append(sample)
                            print(sample)
                            time.sleep(0.2)
                        assert_smoke_samples(samples, expectation)
                        print("Smoke assertions passed:" + expectation if expectation != "none" else
                              "Transport-only smoke: no ladder behavior asserted (--expect none)")
                finally:
                    if args.command == "smoke":
                        link.stop()
                        print("STOP confirmed")
            elif args.command == "readback":
                Path(args.file).write_bytes(link.read_program(PROGRAM_START, args.bytes))
            elif args.command == "monitor":
                until = time.monotonic() + max(0, args.seconds)
                while time.monotonic() < until:
                    print(link.monitor())
                    time.sleep(0.2)
            elif args.command in ("stop", "run"):
                getattr(link, args.command)()
                print(link.identify())
    except (OSError, ValueError, LinkError, serial.SerialException) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
