"""Fresh-project acceptance tests; generated applications live in examples/templates."""
import argparse
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import time
import tomllib

ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(os.environ.get("VYNEC", ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))).resolve()
BASE = ROOT / "examples/templates/_generated"


def run(*args, cwd=ROOT, ok=True, env=None):
    result = subprocess.run([str(COMPILER), *args], cwd=cwd, capture_output=True, text=True, env=env)
    if ok and result.returncode:
        raise AssertionError(f"{args}: exit {result.returncode}\n{result.stdout}\n{result.stderr}")
    if not ok and result.returncode == 0:
        raise AssertionError(f"{args}: invalid configuration accepted")
    return result


def init(name, *args):
    path = BASE / name
    if path.exists():
        shutil.rmtree(path)
    run("init", str(path), "--name", name, "--no-git", *args)
    for file in path.rglob("*.vy"):
        assert "{{" not in file.read_text(encoding="utf-8"), file
    return path


def report(path, *args):
    return tomllib.loads(run("hardware", "show", *args, cwd=path).stdout)


def check_build_actions(hw):
    """Exercise build/upload/monitor selection with a recording toolchain double."""
    tools = BASE / "recording_toolchain"
    tools.mkdir(exist_ok=True)
    recorder = tools / "record.py"
    log = tools / "calls.jsonl"
    recorder.write_text('import json, os, sys\nwith open(os.environ["HARDWARE_TEST_LOG"], "a") as f:\n f.write(json.dumps(sys.argv[1:]) + "\\n")\n')
    shim = tools / ("arduino-cli.cmd" if os.name == "nt" else "arduino-cli")
    shim.write_text(f'@"{sys.executable}" "{recorder}" %*\n' if os.name == "nt" else
                    f'#!/bin/sh\nexec "{sys.executable}" "{recorder}" "$@"\n')
    shim.chmod(0o755)
    env = {**os.environ, "PATH": str(tools) + os.pathsep + os.environ["PATH"], "HARDWARE_TEST_LOG": str(log)}
    for command, extra, expected in [
        ("build", (), ["compile"]),
        ("run", (), ["compile", "upload"]),
        ("run", ("--monitor", "--port", "COM9", "--baud", "57600"), ["compile", "upload", "monitor"]),
        ("run", ("--esp32", "esp32", "--monitor", "--port", "COM9", "--baud", "57600"), ["compile", "upload", "monitor"]),
    ]:
        log.write_text("")
        run(command, *extra, cwd=hw, env=env)
        calls = [json.loads(line) for line in log.read_text().splitlines()]
        assert [call[0] for call in calls] == expected, calls
        if command == "run":
            port = "COM9" if extra else report(hw)["connection"]["port"]
            assert calls[1][calls[1].index("-p") + 1] == port
        if extra:
            assert "baudrate=57600" in calls[2]
            assert "-DVYNE_SERIAL_BAUD=57600" in (hw / "build/firmware/main_sketch/build_opt.h").read_text()
    log.write_text("")
    run("build", "--esp32-build-property", "compiler.cpp.extra_flags=-DVYNE_DISPLAY_ROTATION=3",
        "--esp32-build-property", "build.extra_flags=-DTEST_ADDITIVE=1", cwd=hw, env=env)
    flags = (hw / "build/firmware/main_sketch/build_opt.h").read_text()
    assert flags.rfind("-DVYNE_DISPLAY_ROTATION=3") > flags.find("-DVYNE_DISPLAY_ROTATION=2")
    assert "-DTEST_ADDITIVE=1" in flags
    assert not any(arg.startswith("build.extra_flags=") for arg in json.loads(log.read_text().splitlines()[0]))
    # Emulate the response file left by the real core's prebuild hook. Changes
    # must invalidate the object cache; identical options may reuse it.
    cache = hw / "build/firmware/main_cache"
    cache.mkdir(exist_ok=True)
    (cache / "build_opt.h").write_text(flags)
    log.write_text("")
    run("build", cwd=hw, env=env)
    assert "--clean" in json.loads(log.read_text().splitlines()[0])
    shutil.copyfile(hw / "build/firmware/main_sketch/build_opt.h", cache / "build_opt.h")
    log.write_text("")
    run("build", cwd=hw, env=env)
    assert "--clean" not in json.loads(log.read_text().splitlines()[0])
    log.write_text("")
    run("build", "--hardware-set", "display.rotation=0", cwd=hw, env=env)
    assert "--clean" in json.loads(log.read_text().splitlines()[0])
    run("build", cwd=hw, env=env) # restore the staged default used below
    print("PASS: build-only/run upload, explicit monitoring, CLI port/baud and legacy additive macro overrides")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile", action="store_true", help="Build desktop, Uno, ESP32 and HW-950 with real toolchains")
    parser.add_argument("--port", help="Upload/reset HW-950 and check serial startup (requires pyserial)")
    args = parser.parse_args()
    BASE.mkdir(parents=True, exist_ok=True)
    run("init", "--self-test")
    listed = run("hardware", "list").stdout
    assert "HW-950" in listed and "ESP32-S3" in listed
    desktop = init("desktop", "--kind", "blank")
    uno = init("uno", "--kind", "embedded", "--board", "arduino-uno", "--port", "COM5", "--baud", "9600")
    esp32 = init("esp32", "--kind", "embedded", "--board", "esp32")
    hw = init("hw950", "--kind", "embedded", "--preset", "hw950", "--port", args.port or "COM5")
    assert report(uno)["board"]["name"] == "arduino:avr:uno"
    assert report(uno)["connection"]["baud"] == 9600
    assert "flash_kb = 32" in (uno / "hardware.toml").read_text()
    assert report(hw)["display"]["width"] == 240
    assert report(hw)["display"]["mirror_y"] is True
    assert report(hw)["display"]["rotation"] == 1
    assert report(hw)["display"]["driver"] == "st7796"
    assert report(hw)["display"]["hspi"] is True
    assert report(hw)["display"]["enable"] == 27
    assert report(hw)["display"]["spi_frequency"] == 20000000
    assert report(hw)["touch"]["hspi"] is True
    assert report(hw)["touch"]["cs"] == 33
    baseline = json.loads((ROOT / "vyne/runtime/esp32/profiles/hw950-screen-rgb.json").read_text())["build_properties"][0]
    resolved = report(hw)
    for section, key, define in [("display", "width", "TFT_WIDTH"), ("display", "height", "TFT_HEIGHT"),
                                  ("display", "cs", "TFT_CS"), ("touch", "cs", "VYNE_TOUCH_CS")]:
        assert f"-D{define}={resolved[section][key]}" in baseline
    with (hw / "hardware.toml").open("a") as stream:
        stream.write('\n[display]\nrotation = 2\nspi_frequency = 16000000\n')
    assert report(hw)["display"]["rotation"] == 2
    assert report(hw, "--hardware-set", "display.rotation=1")["display"]["rotation"] == 1
    for assignment, message in [
        ("display.rotation=4", "rotation"), ("display.width=0", "width"),
        ("display.mosi=36", "input-only"), ("display.mosi=40", "GPIO"),
        ('display.driver="bogus"', "driver"), ('touch.driver="capacitive"', "driver"),
        ('display.driver="st7789"', "unsupported"),
        ("display.width=320", "dimensions"),
        ("touch.cs=15", "chip-select"), ('board.name="arduino:avr:uno"', "incompatible"),
        ("connection.baud=115200oops", "integer"), ("display.mirror_y=1", "true or false"),
        ("display.rotaton=1", "unknown"), ('board.name="esp32:esp32:esp32s3"', "classic"),
    ]:
        result = run("hardware", "show", "--hardware-set", assignment, cwd=hw, ok=False)
        assert message in result.stderr, result.stderr
    run("hardware", "show", "--preset", "missing", cwd=hw, ok=False)
    run("init", str(BASE / "invalid"), "--name", "invalid", "--kind", "embedded", "--preset", "missing", "--no-git", ok=False)
    run("init", str(BASE / "invalid"), "--name", "invalid", "--kind", "embedded", "--board", "esp32s3", "--preset", "hw950", "--no-git", ok=False)
    text = (hw / "hardware.toml").read_text()
    try:
        (hw / "hardware.toml").write_text(text + "rotation = 1\n")
        assert "duplicate" in run("hardware", "show", cwd=hw, ok=False).stderr
        (hw / "hardware.toml").write_text('[board]\npreset = "hw950"\n[display\n')
        assert ":3:" in run("hardware", "show", cwd=hw, ok=False).stderr
    finally:
        (hw / "hardware.toml").write_text(text)
    nested = hw / "src"
    assert report(nested)["board"]["preset"] == "hw950"
    # Non-TTY init must exit before entering an interactive screen.
    result = run("init", str(BASE / "no_tty"), ok=False)
    assert result.returncode == 2 and "non-interactive" in result.stderr
    # Explicit target choices are shared with the interactive questionnaire.
    for target in ("esp32s2", "esp32s3", "esp32c3", "esp32c6", "hw950"):
        sample = init("target_" + target, "--kind", "embedded", "--targets", target)
        assert report(sample)["board"]["family"] == "esp32"
    check_build_actions(hw)
    flags = (hw / "build/firmware/main_sketch/build_opt.h").read_text()
    assert "-DST7796_DRIVER=1" in flags and "-UST7789_DRIVER" in flags
    assert "-UVYNE_DISPLAY_MIRROR_Y\n-DVYNE_DISPLAY_MIRROR_Y=1" in flags
    assert "-DVYNE_DISPLAY_ENABLE_PIN=27" in flags and "-DVYNE_TOUCH_SPI_PORT=HSPI" in flags
    def definitions(tokens):
        result = {}
        for token in tokens:
            if token.startswith("-D"):
                name, _, value = token[2:].partition("=")
                result[name] = value or "1"
            elif token.startswith("-U"):
                result.pop(token[2:], None)
        return result
    # Compare EVERY define in the visually/contact-verified historical profile,
    # not merely dimensions and chip-selects. Only the explicit TOML SPI override
    # above differs; configured rotation is new and absent from the old profile.
    expected = definitions(shlex.split(baseline.split("=", 1)[1]))
    expected["SPI_FREQUENCY"] = "16000000"
    actual = definitions(flags.split())
    for name, value in expected.items():
        assert actual.get(name) == value, (name, value, actual.get(name))
    staged_runtime = (hw / "build/firmware/main_sketch/detail/mcu_display.h").read_text()
    explicit_body = staged_runtime.split("esp32_display_init(VyneValue rotation)", 1)[1].split("esp32_display_init_default", 1)[0]
    assert "setRotation((uint8_t)(rotation.as.i64 & 0x3))" in explicit_body
    assert "setRotation(VYNE_DISPLAY_ROTATION)" not in explicit_body
    generated = (hw / "build/firmware/main_sketch/main_sketch.ino").read_text()
    assert "esp32_display_init_default()" in generated
    assert "esp32_touch_screen_configured()" in generated and "esp32_touch_screen_calibrated()" in generated
    assert "HW950_HEARTBEAT" in generated and "HW950_CALIBRATION_RESULT" in generated
    # The exact working radar/touch sources must compile under project hardware
    # without their explicit landscape rotation being replaced by the preset.
    tools = BASE / "recording_toolchain"
    env = {**os.environ, "PATH": str(tools) + os.pathsep + os.environ["PATH"],
           "HARDWARE_TEST_LOG": str(tools / "calls.jsonl")}
    for name in ("esp32_wifi_radar", "esp32_touch"):
        source = ROOT / "examples/test" / (name + ".vy")
        run("build", str(source), cwd=hw, env=env)
        ino = (hw / f"build/firmware/{name}_sketch/{name}_sketch.ino").read_text()
        assert "esp32_display_init(" in ino and "esp32_display_init_default()" not in ino
    print("PASS: full known-good HW-950 profile parity, configured default versus explicit example rotation, touch status and persistent polling")
    print("PASS: interactive events, initialization, TOML types, presets, precedence, validation, discovery, legacy Uno and non-TTY guard")
    if args.compile or args.port:
        run("build", cwd=desktop)
        assert "Hello from desktop" in run("run", cwd=desktop).stdout
        native_zero = run("run", str(ROOT / "examples/transpiler/native_zero_side_effects.vy"),
                          "-o", str(BASE / "native_zero"), cwd=desktop)
        assert "nativeZero side effects: PASS" in native_zero.stdout
        for path in (uno, esp32, hw):
            extra = ("--hardware-set", "display.rotation=1") if path == hw else ()
            result = run("build", *extra, cwd=path)
            assert ">> flash" not in result.stdout
            assert not list((path / "src").glob("*.ino*"))
            assert not list((path / "src").glob("*.vy.c"))
            assert (path / "build/firmware").is_dir()
            print(f"PASS: real {path.name} build")
        flags = (hw / "build/firmware/main_sketch/build_opt.h").read_text()
        assert "-DVYNE_DISPLAY_ROTATION=1" in flags and "-DTFT_WIDTH=240" in flags
        assert "-DSPI_FREQUENCY=16000000" in flags
        commands = (hw / "build/firmware/main_cache/compile_commands.json").read_text()
        assert "ARDUINO_USB_CDC_ON_BOOT" in commands and "build_opt.h" in commands
        assert list((hw / "build/firmware/main_build").glob("*.bin"))
        print("PASS: hardware defines affect firmware; platform USB defines retained; artifacts confined to build")
    if args.port:
        import serial
        run("run", cwd=hw)
        with serial.Serial(args.port, 115200, timeout=0.2) as port:
            # Reopen after upload, then reset through the serial adapter so the
            # starter's startup diagnostics are observable deterministically.
            port.dtr = False
            port.rts = True
            time.sleep(0.1)
            port.rts = False
            deadline = time.monotonic() + 14
            output = bytearray()
            while time.monotonic() < deadline:
                output.extend(port.read(4096))
            assert b"hw950: HW-950 ready" in output, output.decode(errors="replace")
            assert b"HW950_GEOMETRY rotation=2 width=240 height=320" in output, output.decode(errors="replace")
            assert b"HW950_TOUCH configured=true" in output, output.decode(errors="replace")
            assert output.count(b"HW950_HEARTBEAT") >= 5, output.decode(errors="replace")
        print(f"PASS: fresh HW-950 build/run/upload/reset/start on {args.port}")
        rotation_source = ROOT / "examples/test/esp32_display_rotation.vy"
        run("run", str(rotation_source), cwd=hw)
        with serial.Serial(args.port, 115200, timeout=3) as uart:
            uart.dtr = False
            uart.rts = True
            time.sleep(0.1)
            uart.reset_input_buffer()
            uart.rts = False
            text = uart.read(16384).decode(errors="replace")
        expected = ["DEFAULT rotation=2 width=240 height=320",
                    "EXPLICIT_0 rotation=0 width=240 height=320",
                    "EXPLICIT_1 rotation=1 width=320 height=240",
                    "EXPLICIT_2 rotation=2 width=240 height=320",
                    "EXPLICIT_3 rotation=3 width=320 height=240",
                    "DEFAULT_AGAIN rotation=2 width=240 height=320"]
        assert all(line in text for line in expected), text
        print("PASS: physical configured-default versus explicit 0/1/2/3 rotation precedence")
        run("run", cwd=hw) # leave the interactive starter on the board


if __name__ == "__main__":
    main()
