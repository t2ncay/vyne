#!/usr/bin/env python3
"""Desktop acceptance runner. Run from any directory; outputs stay under build/."""
import argparse
import json
import os
import platform
import re
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
TESTS = Path(__file__).resolve().parent
BUILD = ROOT / "build/vraylib-tests"
SUFFIX = ".exe" if os.name == "nt" else ""


def run(command, expected=None, failure=None, env=None, timeout=240):
    result = subprocess.run([str(c) for c in command], cwd=ROOT, env=env,
                            capture_output=True, text=True, errors="replace", timeout=timeout)
    output = result.stdout + result.stderr
    if failure:
        if result.returncode == 0 or failure not in output:
            raise RuntimeError(f"Expected failure containing {failure!r}:\n{output}")
    elif result.returncode or (expected and expected not in output):
        raise RuntimeError(f"Command failed ({result.returncode}): {command}\n{output}")
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--graphics", action="store_true", help="run GPU readback and rlgl acceptance")
    parser.add_argument("--camera", action="store_true", help="run original bodycam assets for 30 frames")
    parser.add_argument("--audio", action="store_true", help="enable bodycam audio device during --camera")
    parser.add_argument("--sector", action="store_true", help="run the complete Sector Shift story with original local assets")
    parser.add_argument("--sanitize", action="store_true", help="instrument native bridge and C test with ASan/UBSan (Linux)")
    opts = parser.parse_args()
    BUILD.mkdir(parents=True, exist_ok=True)
    compiler = Path(os.environ.get("VYNEC", str(ROOT / ("vynec" + SUFFIX)))).resolve()
    env = dict(os.environ)
    env.setdefault("VYNE_RAYLIB_CACHE", str(BUILD / "cache"))
    run([sys.executable, ROOT / "scripts/generate_vraylib.py", "--check"])
    inventory = json.loads((ROOT / "vyne/runtime/vraylib/api.json").read_text())
    for source, count in {"raylib": 619, "raymath": 146, "rlgl": 163}.items():
        assert sum(f["source"] == source for f in inventory["functions"]) == count
    print("PASS: pinned API inventory and reproducible generation")

    with tempfile.TemporaryDirectory(prefix="init-vraylib-", dir=BUILD) as directory:
        project = Path(directory) / "app"
        run([compiler, "init", project, "--name", "raytest", "--kind", "blank", "--libraries", "vraylib", "--no-git"], env=env)
        config = tomllib.loads((project / "project.toml").read_text())
        assert config["dependencies"]["vraylib"] == "6.2", config
    print("PASS: project scaffolding records vraylib 6.2")

    for name in ("api", "ownership", "callbacks", "managed", "sector_state") + (("render", "managed_render", "sector_render") if opts.graphics else ()):
        output = BUILD / name
        run([compiler, "--compile", "-o", output, TESTS / (name + ".vy")], env=env)
        run([str(output) + SUFFIX], expected="VRAYLIB_" + name.upper() + "_OK", env=env)
        print("PASS:", name)

    # Every unavailable declaration must fail during Vyne emission, including
    # aliases. Test helpers write only disposable fixtures under build/.
    unavailable = [f for f in inventory["functions"] if f["status"] == "unavailable"]
    for f in unavailable:
        fixture = BUILD / "unavailable.vy"
        args = ",".join("null" for _ in f["args"])
        fixture.write_text(f'use native vraylib as ray; ray.{f["vyne"]}({args});\n')
        run([compiler, "--emit-c", fixture], failure=f["reason"], env=env)
    fixture = BUILD / "backend.vy"
    fixture.write_text("use native vraylib; vraylib.get_time();\n")
    run([compiler, "--ast", fixture], failure="requires desktop compilation", env=env)
    run([compiler, "--arduino", "uno", fixture], failure="compiled desktop target", env=env)
    run([compiler, "--esp32", "esp32", fixture], failure="compiled desktop target", env=env)
    fixture.write_text("use native vraylib as ray; ray.init_window(1);\n")
    run([compiler, "--emit-c", fixture], failure="expects 3 arguments", env=env)
    print(f"PASS: {len(unavailable)} capability diagnostics, three backend diagnostics, and arity")

    # Build a real user TU with both runtime modules. Reuse the native driver
    # cache to avoid a second full raylib build for the non-sanitized check.
    archives = list(Path(env["VYNE_RAYLIB_CACHE"]).glob("*/lib/libvyne_raylib.a"))
    assert archives, "native driver did not produce its cache"
    native_dir = max(archives, key=lambda p: p.stat().st_mtime_ns).parent
    flags = ["-std=c11", "-D_DEFAULT_SOURCE", "-O1", "-g"]
    if opts.sanitize:
        if platform.system() != "Linux":
            raise RuntimeError("--sanitize currently requires Linux GCC ASan/UBSan")
        sanitized = BUILD / "sanitized"
        run(["cmake", "-S", ROOT / "vyne/runtime/vraylib", "-B", sanitized,
             "-DCMAKE_C_COMPILER=gcc", "-DCMAKE_BUILD_TYPE=Debug",
             "-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"], env=env)
        run(["cmake", "--build", sanitized, "--parallel", "4"], env=env)
        native_dir = sanitized / "lib"
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    system = platform.system()
    libraries = ["-lopengl32", "-lgdi32", "-lwinmm", "-lshell32", "-luser32"] if system == "Windows" else \
        ["-framework", "Cocoa", "-framework", "IOKit", "-framework", "CoreVideo", "-framework", "OpenGL", "-framework", "QuartzCore", "-lpthread"] if system == "Darwin" else \
        ["-lGL", "-lX11", "-lpthread", "-ldl", "-lm"]
    native = BUILD / ("native" + SUFFIX)
    run(["gcc", *flags, TESTS / "native.c", native_dir / "libvyne_raylib.a", native_dir / "libraylib.a", *libraries, "-o", native], env=env)
    run([native], expected="VRAYLIB_NATIVE_OK", env=env)
    print("PASS: native runtime / Windows headers / stb coexistence" + (" with ASan/UBSan" if opts.sanitize else ""))
    saturation = BUILD / ("saturation" + SUFFIX)
    run(["gcc", *flags, TESTS / "saturation.c", "-lm", "-o", saturation], env=env)
    run([saturation], expected="VRAYLIB_SATURATION_OK", env=env)
    print("PASS: lock-free stereo saturation kernel" + (" with ASan/UBSan" if opts.sanitize else ""))

    for name, marker in (("try_test", "done"), ("native_zero_side_effects", "nativeZero side effects: PASS")):
        binary=BUILD / name
        run([compiler, "--compile", "-o", binary, ROOT / f"examples/transpiler/{name}.vy"], env=env)
        run([str(binary)+SUFFIX], expected=marker, env=env)
    print("PASS: existing try/finally and native zero-argument regressions")

    camera = BUILD / "bodycam"
    run([compiler, "--compile", "-o", camera, ROOT / "examples/graphics/camera_test.vy"], env=env)
    print("PASS: complete camera demo compiles")
    if opts.camera:
        env.update(VYNE_CAMERA_FRAMES="30", VYNE_CAMERA_WIDTH="640", VYNE_CAMERA_HEIGHT="360",
                   VYNE_CAMERA_AUDIO="1" if opts.audio else "0")
        output = run([str(camera) + SUFFIX], expected="BODYCAM_COMPLETE", env=env)
        assert re.search(r"frames=\s*30\s*live_handles=\s*0", output), output
        if opts.audio:
            assert re.search(r"BODYCAM_AUDIO active=\s*true", output), output
        print("PASS: bodycam shaders/models/post-processing, 30 frames, zero live handles")
    sector = BUILD / "sector_shift"
    run([compiler, "--compile", "-o", sector, ROOT / "examples/graphics/sector_shift.vy"], env=env)
    print("PASS: Sector Shift compiles")
    if opts.sector:
        env.update(VYNE_SECTOR_FRAMES="60", VYNE_SECTOR_WIDTH="640", VYNE_SECTOR_HEIGHT="360",
                   VYNE_SECTOR_WINDOWED="1", VYNE_SECTOR_AUTOPLAY="1", VYNE_SECTOR_AUDIO="1" if opts.audio else "0")
        output = run([str(sector) + SUFFIX], expected="SECTOR_COMPLETE", env=env)
        assert re.search(r"events=\s*2\s*phase=\s*2\s*fade=\s*1", output), output
        assert re.search(r"frames=\s*60\s*live_handles=\s*0", output), output
        if opts.audio:
            assert re.search(r"SECTOR_AUDIO active=\s*true", output), output
            assert re.search(r"radio=\s*1\s*dsp=\s*true\s*dsp_blocks=\s*[1-9][0-9]*", output), output
        print("PASS: Sector Shift full progression, two freezes, ending and cleanup" + (" with live radio/saturation" if opts.audio else ""))
    print("All requested vraylib checks passed.")


if __name__ == "__main__":
    main()
