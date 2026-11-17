"""Sniffer host contracts; optional real COM5 packet-rate/memory/stop verification."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import time

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[2]
COMPILER = Path(os.environ.get("VYNEC", ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))).resolve()


def command(args, *, cwd=PROJECT, timeout=300, expected=0):
    result = subprocess.run([str(arg) for arg in args], cwd=cwd, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=timeout)
    text = result.stdout + result.stderr
    assert result.returncode == expected, text
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--esp32", action="store_true")
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--core-only", action="store_true")
    args = parser.parse_args()
    build = PROJECT / "build/sniff-tests"
    build.mkdir(parents=True, exist_ok=True)
    binary = build / ("sniff.exe" if os.name == "nt" else "sniff")
    flags = ["-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitize else []
    command(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-pthread", *flags,
             "-I", PROJECT / "tests/sniff_stubs", PROJECT / "tests/sniff.cpp", "-o", binary])
    assert "SNIFF_HOST_PASS" in command([binary])
    print("PASS: sniffer driver classification, failure rollback, ownership, concurrency, clock/saturation and callback allocation contracts")
    if args.core_only:
        return
    # Arity is rejected by Vyne before invoking any hardware build.
    invalid = build / "invalid.vy"
    invalid.write_text('use native esp32; esp32.wifi_sniff_start();\n', encoding="utf-8")
    text = command([COMPILER, "--esp32", "esp32", "--emit-c", invalid], expected=1)
    assert "expects 1 arguments" in text
    # Emit only: verifies resolved alias and opt-in sniff feature selection.
    binding = build / "binding.vy"
    binding.write_text('use native esp32 as radio; radio.wifi_sniff_start(6); radio.wifi_sniff_sample(); radio.wifi_sniff_stop();\n', encoding="utf-8")
    command([COMPILER, "--esp32", "esp32", "--emit-c", "-o", build / "binding", binding])
    ino = (build / "binding.vy.c").read_text()
    assert "#define VYNE_USE_WIFI_SNIFF" in ino and "esp32_wifi_sniff_start(" in ino
    binding.write_text('use native esp32; esp32.wifi_mode(0);\n', encoding="utf-8")
    command([COMPILER, "--esp32", "esp32", "--emit-c", "-o", build / "ordinary", binding])
    ordinary = (build / "ordinary.vy.c").read_text()
    assert "#define VYNE_USE_WIFI" in ordinary and "#define VYNE_USE_WIFI_SNIFF" not in ordinary
    print("PASS: ESP32 native arity, aliases and optional capture feature lowering")
    if not args.esp32:
        return
    import serial

    def capture(seconds, *, stop=False):
        output = bytearray()
        with serial.Serial(args.port, 115200, timeout=.1) as uart:
            uart.dtr = False; uart.rts = True; time.sleep(.1)
            uart.reset_input_buffer(); uart.rts = False
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                output.extend(uart.read(4096))
            if stop:
                uart.write(b"Q")
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and b"SNIFF_STOPPED" not in output:
                    output.extend(uart.read(4096))
        return output.decode(errors="replace")

    command([COMPILER, "run", "tests/sniff_device.vy", "--port", args.port], timeout=600)
    cycles = capture(15)
    (build / "cycles.log").write_text(cycles, encoding="utf-8")
    assert "SNIFF_CYCLE_PASS" in cycles and "SNIFF_CYCLE_FAIL" not in cycles, cycles
    samples = [tuple(map(int, row)) for row in re.findall(r"SNIFF_CYCLE cycle=(\d+) heap_free=(\d+) arena=(\d+)", cycles)]
    assert len(samples) == 3 and len({row[2] for row in samples}) == 1, cycles
    assert max(row[1] for row in samples) - min(row[1] for row in samples) <= 1024, cycles
    print(cycles)
    print("PASS: physical busy/invalid rejection, repeated start/stop and warmed heap stability")
    text = command([COMPILER, "run", "--port", args.port], timeout=600)
    (build / "upload.log").write_text(text, encoding="utf-8")
    text = capture(args.seconds, stop=True)
    (build / "device.log").write_text(text, encoding="utf-8")
    assert "SNIFF_READY channel=6" in text and "SNIFF_ERROR" not in text, text
    assert not re.search(r"Guru Meditation|panic|abort\(\)", text), text
    records = [tuple(map(int, row)) for row in re.findall(
        r"SNIFF_STATUS channel=(\d+) elapsed_us=(\d+) management=(\d+) data=(\d+) control=(\d+) frames=(\d+) samples=(\d+) native_bytes=(\d+) allocations=(\d+) heap_free=(\d+) arena=(\d+)", text)]
    assert len(records) >= 3 and records[-1][5] > records[0][5], text
    assert all(row[1] >= 500000 and row[6] <= 512 for row in records)
    assert sum(sum(row[2:5]) for row in records) > 0, "No actual traffic observed; radio correctness not validated"
    assert all(len({row[index] for row in records}) == 1 for index in (7, 8, 10)), records
    assert "SNIFF_STOPPED native_bytes=0" in text, text
    print(text)
    print("PASS: real channel-6 frames, advancing chart, bounded native/arena memory and explicit stop cleanup")
    # Reset into the user's chart after testing Q cleanup.
    with serial.Serial(args.port, 115200, timeout=.1) as uart:
        uart.dtr = False; uart.rts = True; time.sleep(.1); uart.rts = False


if __name__ == "__main__":
    main()
