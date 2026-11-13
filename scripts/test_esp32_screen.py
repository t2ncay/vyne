#!/usr/bin/env python3
"""Flash a standalone screen demo and record firmware and visual results separately."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import tempfile
import time

if __package__:
    from .test_esp32 import ROOT, PANIC, command
else:
    from test_esp32 import ROOT, PANIC, command

PATTERNS = {"red": "r", "green": "g", "blue": "b", "white": "w",
            "black": "k", "grid": "c", "text": "t", "bounds": "d"}
PROFILE = ROOT / "vyne/runtime/esp32/profiles/hw950-screen-rgb.json"
DRIVERS = {"ili9341": "ILI9341_DRIVER", "st7789": "ST7789_DRIVER",
           "st7796": "ST7796_DRIVER", "ili9488": "ILI9488_DRIVER"}


def screen_properties(profile, backlight_pin=None, active="high", frequency=None, enable_pin=None, driver=None):
    """Make a screen-only override without changing the saved hardware profile."""
    properties = list(profile["build_properties"])
    if not properties or not all(isinstance(p, str) and "=" in p for p in properties):
        raise ValueError("profile build_properties must contain key=value strings")
    matches = [i for i, p in enumerate(properties) if p.startswith(("build.extra_flags=", "compiler.cpp.extra_flags="))]
    if len(matches) != 1:
        raise ValueError("screen profile must have exactly one C++ flags property")
    index = matches[0]
    flags = properties[index].split("=", 1)[1]
    tokens = shlex.split(flags)
    if "-DUSER_SETUP_LOADED" not in tokens:
        raise ValueError("screen profile must define USER_SETUP_LOADED so wiring is explicit")
    definitions = {}
    for token in tokens:
        match = re.fullmatch(r"-D([A-Za-z_][A-Za-z_0-9]*)=(.*)", token)
        if match:
            definitions[match[1]] = match[2]
    for control_pin in (backlight_pin, enable_pin):
        if control_pin is None:
            continue
        # Classic ESP32 pins only: exclude flash, input-only, and host UART pins.
        valid = {0, 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33}
        if control_pin not in valid:
            raise ValueError("backlight pin must be an output-capable classic ESP32 GPIO outside flash/UART0")
        for name in ("TFT_MOSI", "TFT_MISO", "TFT_SCLK", "TFT_CS", "TFT_DC", "TFT_RST"):
            try:
                pin = int(definitions[name], 0)
            except (KeyError, ValueError):
                raise ValueError(f"cannot validate backlight against numeric {name} in profile")
            if pin >= 0 and pin == control_pin:
                raise ValueError(f"backlight GPIO conflicts with {name}")
    # This demo exercises the screen only. In particular, do not drive GPIO21
    # as touch CS while investigating whether it is actually a backlight pin.
    flags += " -UTOUCH_CS -UVYNE_TOUCH_XPT2046"
    if backlight_pin is not None:
        flags += (f" -UTFT_BL -UTFT_BACKLIGHT_ON -DTFT_BL={backlight_pin}"
                  f" -DTFT_BACKLIGHT_ON={1 if active == 'high' else 0}")
    if frequency is not None:
        if not 100000 <= frequency <= 40000000:
            raise ValueError("SPI frequency must be between 100000 and 40000000 Hz")
        flags += f" -USPI_FREQUENCY -DSPI_FREQUENCY={frequency}"
    if enable_pin is not None:
        flags += f" -DVYNE_DISPLAY_ENABLE_PIN={enable_pin}"
    if driver is not None:
        if driver not in DRIVERS:
            raise ValueError("unsupported display driver")
        flags += " -UILI9341_2_DRIVER " + " ".join("-U" + name for name in DRIVERS.values())
        flags += " -D" + DRIVERS[driver]
        if driver == "st7789":
            flags += " -UVYNE_DISPLAY_MIRROR_X -UVYNE_DISPLAY_MIRROR_Y"
    # C++ extra flags preserve Arduino's ESP32/platform build.extra_flags.
    properties[index] = "compiler.cpp.extra_flags=" + flags
    return properties


def firmware_result(text, pattern):
    if PANIC.search(text):
        return "FAIL", "panic or failure diagnostic observed"
    if "SCREEN_DEMO_READY" not in text:
        return "FAIL", "screen initialization did not reach the ready marker"
    observed = set(re.findall(r"^SCREEN_STAGE:([A-Z]+)\s*$", text, re.MULTILINE))
    expected = set(PATTERNS) if pattern == "auto" else {pattern}
    missing = {p.upper() for p in expected} - observed
    if missing:
        return "FAIL", "patterns not observed in serial: " + ", ".join(sorted(missing))
    return "PASS", "drawing commands executed; physical rendering requires visual confirmation"


def capture(port, seconds, pattern):
    import serial
    chunks = []
    selected = pattern == "auto"
    with serial.Serial(port, 115200, timeout=0.1) as uart:
        uart.dtr = False
        uart.rts = True
        time.sleep(0.1)
        uart.reset_input_buffer()
        uart.rts = False
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            data = uart.readline()
            if not data:
                continue
            text = data.decode(errors="replace")
            chunks.append(text)
            print(text.rstrip(), flush=True)
            if not selected and "SCREEN_DEMO_READY" in "".join(chunks):
                uart.write(PATTERNS[pattern].encode("ascii"))
                uart.flush()
                selected = True
    return "".join(chunks)


def visual_observations():
    print("\nPhysical screen observation (serial success is separate):")
    print("  all = all patterns correct; partial = some graphics; blank = lit but blank;")
    print("  dark = no screen/backlight glow; unknown = not observed")
    while True:
        result = input("What appeared? [all/partial/blank/dark/unknown]: ").strip().lower()
        if result in {"all", "partial", "blank", "dark", "unknown"}:
            break
    glow = input("Did the backlight glow? [yes/no/unknown]: ").strip().lower()
    notes = input("Optional notes (colors, flicker, faint text under flashlight): ").strip()
    return {"result": result, "backlight_glow": glow, "notes": notes}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--profile", type=Path, default=PROFILE)
    parser.add_argument("--pattern", choices=["auto", *PATTERNS], default="auto")
    parser.add_argument("--seconds", type=float, default=40,
                        help="live serial observation window; eight-stage cycle takes about 32 seconds")
    parser.add_argument("--compile-only", action="store_true", help="build without uploading")
    parser.add_argument("--non-interactive", action="store_true", help="leave visual result unverified")
    parser.add_argument("--backlight-pin", type=int, help="confirmed LED/BL GPIO on a classic ESP32")
    parser.add_argument("--backlight-active", choices=["high", "low"], default=None)
    parser.add_argument("--spi-frequency", type=int, help="override display SPI frequency in Hz")
    parser.add_argument("--enable-pin", type=int, help="confirmed auxiliary display enable GPIO, driven HIGH")
    parser.add_argument("--driver", choices=DRIVERS, help="override driver while retaining the profile wiring")
    parser.add_argument("--build-timeout", type=float, default=300)
    parser.add_argument("--output", type=Path,
                        default=Path(tempfile.gettempdir()) / "opencode/vyne-esp32-screen")
    args = parser.parse_args()
    if args.seconds <= 0 or args.build_timeout <= 0:
        parser.error("timeouts must be positive")
    if args.backlight_active and args.backlight_pin is None:
        parser.error("--backlight-active requires --backlight-pin")
    try:
        profile = json.loads(args.profile.read_text(encoding="utf-8"))
        properties = screen_properties(profile, args.backlight_pin,
                                       args.backlight_active or "high", args.spi_frequency, args.enable_pin, args.driver)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.error(str(exc))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    sketch = output / "esp32_screen_demo"
    sketch.mkdir(exist_ok=True)
    shutil.copyfile(ROOT / "scripts/esp32_screen_demo/esp32_screen_demo.ino",
                    sketch / "esp32_screen_demo.ino")
    shutil.copytree(ROOT / "vyne/runtime/esp32/detail", sketch / "detail", dirs_exist_ok=True)
    report = {"port": args.port, "profile": str(args.profile.resolve()),
              "build_properties": properties, "pattern": args.pattern, "seconds": args.seconds,
              "visual": {"result": "unknown", "backlight_glow": "unknown", "notes": ""}}
    report_file = output / "results.json"

    def save():
        report_file.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(f"Screen-only demo on {args.port}; touch CS initialization disabled.")
    if args.backlight_pin is None:
        print("No backlight override supplied; existing profile settings apply.")
    else:
        print(f"Confirmed backlight GPIO {args.backlight_pin}, active {args.backlight_active or 'high'}.")
    build = output / "build"
    cmd = ["arduino-cli", "compile", "--fqbn", "esp32:esp32:esp32",
           "--output-dir", str(build), str(sketch)]
    for prop in properties:
        cmd.extend(["--build-property", prop])
    save()
    print(f"Building screen demo (timeout {args.build_timeout:g}s)...", flush=True)
    rc = command(cmd, output / "build.log", args.build_timeout)
    report["build"] = "PASS" if rc == 0 else "FAIL"
    report["build_exit"] = rc
    save()
    if rc != 0 or args.compile_only:
        print(f"Build: {report['build']}. Logs: {output}")
        return int(rc != 0)
    print("Uploading screen demo...")
    rc = command(["arduino-cli", "upload", "--fqbn", "esp32:esp32:esp32",
                  "-p", args.port, "--input-dir", str(build)], output / "upload.log", 90)
    report["upload"] = "PASS" if rc == 0 else "FAIL"
    save()
    if rc != 0:
        print(f"Upload failed. Logs: {output}")
        return 1
    print("Watch the screen now: red, green, blue, white, black, grid, text, bounds.", flush=True)
    try:
        text = capture(args.port, args.seconds, args.pattern)
    except Exception as exc:
        text = f"monitor: FAIL ({exc})\n"
        print(text, end="")
    (output / "serial.log").write_text(text, encoding="utf-8")
    report["firmware"], report["firmware_reason"] = firmware_result(text, args.pattern)
    if not args.non_interactive and os.isatty(0):
        try:
            report["visual"] = visual_observations()
        except (EOFError, KeyboardInterrupt):
            print("\nVisual observation not recorded.")
    save()
    print(f"\nFirmware: {report['firmware']}; physical screen: {report['visual']['result']}.")
    print(f"Logs and report: {output}")
    return int(report["firmware"] == "FAIL" or report["visual"]["result"] in {"partial", "blank", "dark"})


if __name__ == "__main__":
    raise SystemExit(main())
