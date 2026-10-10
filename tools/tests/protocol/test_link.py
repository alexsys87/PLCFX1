"""Drive the actual C RX/command/TX state machines using the Python client.

The default fixture is transport-only; --runtime includes the actual ladder
engine. Neither fixture emulates electrical UART or Flash timing/physics.
"""
import importlib.util
import contextlib
import io
import os
from pathlib import Path
import select
import subprocess
import sys
import types

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("plc_link", ROOT / "tools/plc_link.py")
link_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(link_module)


class PipeSerial:
    def __init__(self, executable):
        self.process = subprocess.Popen([str(executable), "--wire"], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE)

    def write(self, data):
        return self.process.stdin.write(data)

    def flush(self):
        self.process.stdin.flush()

    def read(self, count):
        ready, _, _ = select.select([self.process.stdout], [], [], 3.0)
        return os.read(self.process.stdout.fileno(), count) if ready else b""

    def reset_input_buffer(self):
        pass

    def close(self):
        self.process.stdin.close()
        assert self.process.wait(timeout=3) == 0

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def rejected(action):
    try:
        action()
    except link_module.LinkError:
        return
    raise AssertionError("Expected a rejected transaction")


def test(executable, runtime=False):
    port = PipeSerial(executable)
    try:
        link = link_module.PLCLink(port)
        assert link.connect() == {"model_code": "0x6266", "capacity_words": 6000, "state": "STOP"}
        original_header = link.read_program(0x8000, 92)
        assert original_header[:2] == b"\x06\x00"
        program = link_module.load_program(ROOT / "examples/always_on_m0.hex")
        assert link.upload(program) == program
        assert link.identify()["state"] == "STOP"
        link.clear_smoke_m0()
        link.run()
        assert link.monitor()["RUN"]
        if runtime:
            assert link.monitor()["M0"]  # Real LD M8000 OUT M0 scan executed.
            link_module.assert_smoke_samples([link.monitor()], "m0")
        else:
            rejected(lambda: link_module.assert_smoke_samples([link.monitor()], "m0"))
        link.stop()
        assert not link.monitor()["RUN"]
        assert link.read_program(0x805C, len(program)) == program
        # Exercise chunking and all seven flash blocks without assuming erase.
        large = bytes([0x00, 0x00]) * 5999 + b"\x0f\x00"
        assert link.upload(large) == large
        assert link.read_program(0x8000, 92) == original_header  # factory header preserved
        rejected(lambda: link.exchange(link_module.frame("E", b"\xff")))
        rejected(lambda: link.exchange(link_module.frame("0", b"\x2f\xff\x02"), 2))
        malformed = bytearray(link_module.frame("0", b"\x00\x00\x01"))
        malformed[-1] = ord("Z")
        rejected(lambda: link.exchange(bytes(malformed), 1))
        assert link.identify()["state"] == "STOP"  # next STX recovers
        if runtime:
            link.upload(b"\xfe\xff\x0f\x00")  # unsupported opcode
            rejected(link.run)
            assert link.identify()["state"] == "STOP"
            link.upload(program)
            link.run()
            assert link.monitor()["M0"]
            link.stop()
        rejected(lambda: link_module.assert_smoke_samples([], "m0"))
        rejected(lambda: link_module.assert_smoke_samples([{"RUN": True, "M8013": False, "Y0": False}], "clock-y0"))
        link_module.assert_smoke_samples([
            {"RUN": True, "M8013": False, "Y0": False},
            {"RUN": True, "M8013": True, "Y0": True}], "clock-y0")
        assert link_module.smoke_expectation(program) == "m0"
        print("PASS: Python client -> actual C framed connect/identify/STOP/upload/flush/readback/RUN/monitor/STOP; 6000-word chunking and NAK recovery" +
              ("; actual ladder M0 execution, malformed-program STOP and recovery" if runtime else " (transport-only)"))
    finally:
        port.close()

    # Exercise the actual CLI dispatch/finally path, not just link methods.
    # Replace only pyserial's transport with the real C wire fixture.
    previous_serial = sys.modules.get("serial")
    sys.modules["serial"] = types.SimpleNamespace(
        Serial=lambda *a, **k: PipeSerial(executable), EIGHTBITS=8,
        PARITY_NONE="N", STOPBITS_ONE=1, SerialException=OSError)
    try:
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            status = link_module.main(["--port", "fixture", "smoke",
                                       str(ROOT / "examples/always_on_m0.hex"), "--seconds", "0.3"])
        assert status == (0 if runtime else 1), (status, output.getvalue(), errors.getvalue())
        assert "STOP confirmed" in output.getvalue()
        if runtime:
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                status = link_module.main(["--port", "fixture", "smoke",
                                           str(ROOT / "examples/clock_y0.hex"), "--seconds", "1.5"])
            assert status == 0, (status, output.getvalue(), errors.getvalue())
            print("PASS: full CLI known-example smoke asserts M0 and real runtime clock/Y0 phases; finally STOP")
        else:
            assert "did not set M0" in errors.getvalue()
            print("PASS: full CLI smoke returns failure when ladder never executes, with finally STOP")
    finally:
        if previous_serial is None:
            del sys.modules["serial"]
        else:
            sys.modules["serial"] = previous_serial


if __name__ == "__main__":
    test(sys.argv[1], len(sys.argv) > 2 and sys.argv[2] == "--runtime")
