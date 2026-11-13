#!/usr/bin/env python3
"""Build ESP32 examples and optionally validate bounded serial runs on a board."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
CORE = {"hello", "smoke", "arena", "bits", "math", "nvs", "realtime", "analog"}
MARKERS = {
    **{name: f"{name}: PASS" for name in CORE},
    "smoke": "SMOKE: PASS",
    "display": "display test complete",
    "i2c_scan": "i2c scan: DONE",
    "imu": "imu: DONE",
    "interrupts": "interrupts: DONE",
    "touch": "touch: DONE",
    "touch_calibrate": "touch calibrate: DONE",
    "wifi_scan": "wifi scan: DONE",
    "wifi_radar": "Scan complete.",
    "wifi_connect": "wifi connect: PASS",
    "wifi_http": "http: PASS",
    "contracts": "contracts: PASS",
    "serial": "serial: DONE",
    "power": "wake_cause",
    "runtime": "runtime: PASS",
    "touch_probe": "touch probe: DONE",
}
PANIC = re.compile(r"Guru Meditation|Backtrace:|abort\(\)|assert failed|: FAIL|SMOKE: FAIL")


def merge_build_properties(properties):
    """Append extra compiler flags to profile flags; last value wins for other keys."""
    merged = {}
    for property in properties:
        key, value = property.split("=", 1)
        if key in {"compiler.cpp.extra_flags", "compiler.c.extra_flags"} and key in merged:
            merged[key] += " " + value
        else:
            merged[key] = value
    return [key + "=" + value for key, value in merged.items()]


def classify(name, text):
    if PANIC.search(text):
        return "FAIL", "panic or FAIL diagnostic observed"
    if "SKIP" in text:
        return "SKIP", "example prerequisites not configured"
    marker = MARKERS[name]
    if marker not in text:
        return "FAIL", f"completion marker not observed: {marker}"
    if name == "touch_probe":
        readings = re.findall(r"raw touch: x=(\d+) y=(\d+) z=(\d+)", text)
        if readings and all(x == "0" and y == "0" for x, y, z in readings):
            return "SKIP", "raw controller response has only zero coordinates; contact is not established"
    if name == "touch_probe" and "contact=" in text and "contact=1" not in text:
        return "SKIP", "raw probe completed without any contact response"
    unavailable = {
        "imu": "MPU6050 NOT detected",
        "interrupts": "edge seen: none",
        "touch": "touch: no contact",
        "touch_probe": "raw touch: x=8191 y=8191 z=0",
    }
    if name in unavailable and unavailable[name] in text:
        if name != "touch_probe" or all(
            line == unavailable[name] for line in text.splitlines() if line.startswith("raw touch:")
        ):
            return "SKIP", "sketch completed, but no device/signal/contact response was observed"
    if name == "serial" and "rx: 86" not in text:
        return "FAIL", "injected UART byte was not echoed"
    if name == "power" and "wake_cause = 4" not in text:
        return "FAIL", "timer wake from deep sleep was not observed"
    if name == "runtime" and not all(value in text for value in (
        "int64 max = 9223372036854775807", "int64 min = -9223372036854775808"
    )):
        return "FAIL", "UART0 Int64 output was truncated"
    return "PASS", "completion marker observed without a failure diagnostic"


def command(args, log, timeout):
    # Log directly to disk: a compiler descendant keeping a stdout pipe
    # open must not make timeout cleanup wait indefinitely for EOF.
    with log.open("w", encoding="utf-8") as output:
        process = subprocess.Popen(args, stdout=output, stderr=subprocess.STDOUT,
                                   start_new_session=os.name != "nt")
        try:
            return process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            if os.name == "nt":
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                               stdout=output, stderr=subprocess.STDOUT, timeout=30)
            else:
                os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=30)
            output.write(f"\nCommand timed out after {timeout} seconds\n")
            return 124


def monitor(port, seconds, name):
    import serial
    with serial.Serial(port, 115200, timeout=0.1) as uart:
        # Reset into normal boot after opening, capturing setup() output as well.
        uart.dtr = False
        uart.rts = True
        time.sleep(0.1)
        uart.reset_input_buffer()
        uart.rts = False
        output = bytearray()
        deadline = time.monotonic() + seconds
        sent = False
        while time.monotonic() < deadline:
            output.extend(uart.read(max(1, uart.in_waiting)))
            if name == "serial" and not sent and b"=== ESP32 serial" in output:
                time.sleep(0.5)
                uart.write(b"V")
                sent = True
        return output.decode(errors="replace")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path,
                        default=ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))
    parser.add_argument("--board", default="esp32")
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--run", nargs="*", default=None,
                        help="flash named examples; no names means the core suite")
    parser.add_argument("--only", nargs="*", help="build only these example suffixes")
    parser.add_argument("--seconds", type=float, default=12)
    parser.add_argument("--build-timeout", type=float, default=300,
                        help="per-sketch build timeout in seconds")
    parser.add_argument("--build-property", action="append", default=[],
                        help="repeatable arduino-cli key=value override, including TFT driver/pins")
    parser.add_argument("--profile", type=Path, help="JSON file containing build_properties")
    parser.add_argument("--output", type=Path,
                        default=Path(tempfile.gettempdir()) / "opencode" / "vyne-esp32-tests")
    args = parser.parse_args()
    if args.profile:
        profile = json.loads(args.profile.read_text(encoding="utf-8"))
        args.build_property = profile["build_properties"] + args.build_property
    for prop in args.build_property:
        if not isinstance(prop, str) or "=" not in prop or prop.startswith("="):
            parser.error("build properties must be key=value strings")
    args.build_property = merge_build_properties(args.build_property)
    if args.seconds <= 0 or args.build_timeout <= 0:
        parser.error("timeouts must be positive")
    if args.board not in {"esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6"}:
        parser.error("unsupported ESP32 board alias")
    args.output.mkdir(parents=True, exist_ok=True)
    run = set(args.run) if args.run else CORE | {"runtime"} if args.run is not None else set()
    results = []
    sources = sorted((ROOT / "examples" / "test").glob("esp32_*.vy"))
    sources.extend(sorted((ROOT / "examples/test/fixtures").glob("esp32_*/*.ino")))
    names = {source.stem.removeprefix("esp32_") for source in sources}
    unknown = (run | set(args.only or [])) - names
    if unknown:
        parser.error("unknown examples: " + ", ".join(sorted(unknown)))
    (args.output / "run.json").write_text(json.dumps({
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "board": args.board, "port": args.port, "seconds": args.seconds,
        "compiler": str(args.compiler.resolve()), "build_properties": args.build_property,
        "run": sorted(run), "only": args.only,
    }, indent=2) + "\n", encoding="utf-8")
    for source in sources:
        name = source.stem.removeprefix("esp32_")
        if args.only is not None and name not in args.only:
            continue
        base = args.output / source.stem
        build_log = args.output / f"{source.stem}.build.log"
        print(f"Building {source.stem} (timeout {args.build_timeout:g}s)...", flush=True)
        if source.suffix == ".ino":
            sketch = Path(str(base) + "_sketch")
            sketch.mkdir(exist_ok=True)
            shutil.copyfile(source, sketch / f"{sketch.name}.ino")
            shutil.copyfile(ROOT / "vyne/runtime/esp32/vyne_esp32.h", sketch / "vyne_esp32.h")
            shutil.copytree(ROOT / "vyne/runtime/esp32/detail", sketch / "detail", dirs_exist_ok=True)
            cmd = ["arduino-cli", "compile", "--fqbn", f"esp32:esp32:{args.board}",
                   "--output-dir", str(base) + "_build", str(sketch)]
            for prop in args.build_property:
                cmd.extend(["--build-property", prop])
        else:
            cmd = [str(args.compiler.resolve()), "--esp32", args.board, "-o", str(base), str(source)]
            for prop in args.build_property:
                cmd.extend(["--esp32-build-property", prop])
        rc = command(cmd, build_log, args.build_timeout)
        entry = {"test": name, "build": "PASS" if rc == 0 else "FAIL", "build_exit": rc}
        if rc == 0 and name in run:
            print(f"Uploading {source.stem} to {args.port}...", flush=True)
            upload = ["arduino-cli", "upload", "-p", args.port, "--fqbn",
                      f"esp32:esp32:{args.board}", "--input-dir", str(base) + "_build"]
            rc = command(upload, args.output / f"{source.stem}.upload.log", 90)
            entry["upload"] = "PASS" if rc == 0 else "FAIL"
            if rc == 0:
                try:
                    text = monitor(args.port, args.seconds, name)
                except Exception as exc:
                    text = f"monitor: FAIL ({exc})\n"
                (args.output / f"{source.stem}.serial.log").write_text(text, encoding="utf-8")
                entry["runtime"], entry["reason"] = classify(name, text)
                print(text, flush=True)
        results.append(entry)
        (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(entry), flush=True)
    print(f"Logs and report: {args.output}")
    return int(any("FAIL" in entry.values() for entry in results))


if __name__ == "__main__":
    raise SystemExit(main())
