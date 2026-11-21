#!/usr/bin/env python3
"""Focused recovery/font regressions; independent of the VMGE runners."""
import argparse
import os
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TESTS = Path(__file__).resolve().parent
BUILD = ROOT / "build/runtime-vraylib-blockers"
SUFFIX = ".exe" if os.name == "nt" else ""


def run(command, env, marker=None):
    result = subprocess.run([str(c) for c in command], cwd=ROOT, env=env,
                            capture_output=True, text=True, errors="replace", timeout=240)
    output = result.stdout + result.stderr
    if result.returncode or (marker and marker not in output):
        raise RuntimeError(f"Failed ({result.returncode}): {command}\n{output}")
    print(marker or f"PASS: {Path(str(command[-1])).name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, help="also run hidden-window font checks using a Latin-extended TTF")
    opts = parser.parse_args()
    BUILD.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, VYNE_RAYLIB_CACHE=str(BUILD / "cache"))
    compiler = Path(os.environ.get("VYNEC", str(ROOT / ("vynec" + SUFFIX)))).resolve()
    for name, source, marker in (
        ("recovery", TESTS / "vraylib_recovery.vy", "VRAYLIB_RECOVERY_OK"),
        ("managed", ROOT / "examples/projects/raylib-showcase/tests/managed.vy", "VRAYLIB_MANAGED_OK"),
        ("try_test", ROOT / "examples/transpiler/try_test.vy", "done"),
        ("deferred_close", TESTS / "vraylib_deferred_close.vy", "VRAYLIB_DEFERRED_CLOSE_VY_OK"),
    ):
        if name == "managed":
            # The existing fixture writes to this dedicated acceptance location.
            (ROOT / "build/vraylib-tests").mkdir(parents=True, exist_ok=True)
        binary = BUILD / name
        run([compiler, "--compile", "-o", binary, source], env)
        run([str(binary) + SUFFIX], env, marker)
    archives = list((BUILD / "cache").glob("*/lib/libvyne_raylib.a"))
    native_dir = max(archives, key=lambda p: p.stat().st_mtime_ns).parent
    libraries = ["-lopengl32", "-lgdi32", "-lwinmm", "-lshell32", "-luser32"] if platform.system() == "Windows" else \
        ["-framework", "Cocoa", "-framework", "IOKit", "-framework", "CoreVideo", "-framework", "OpenGL", "-framework", "QuartzCore", "-lpthread"] if platform.system() == "Darwin" else \
        ["-lGL", "-lX11", "-lpthread", "-ldl", "-lm"]
    binary = BUILD / ("recovery_native" + SUFFIX)
    run(["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-O2", TESTS / "vraylib_recovery.c",
         native_dir / "libvyne_raylib.a", native_dir / "libraylib.a", *libraries, "-o", binary], env)
    run([binary], env, "VRAYLIB_RECOVERY_NATIVE_OK")
    from check_region_state import check_region_state
    check_region_state(compiler, env, run, native_dir, libraries)
    binary = BUILD / "font"
    run([compiler, "--compile", "-o", binary, TESTS / "vraylib_font.vy"], env)
    if opts.font:
        env["VYNE_RUNTIME_FONT"] = str(opts.font.resolve())
        run([str(binary) + SUFFIX], env, "VRAYLIB_FONT_OK")
        native = BUILD / ("font_failure" + SUFFIX)
        run(["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-O2", TESTS / "vraylib_font_failure.c",
             native_dir / "libraylib.a", *libraries, "-o", native], env)
        run([native, opts.font.resolve(), TESTS / "vraylib_font.vy"], env, "VRAYLIB_FONT_FAILURE_OK")
        native = BUILD / ("deferred_close_native" + SUFFIX)
        run(["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-O2", TESTS / "vraylib_deferred_close.c",
             native_dir / "libraylib.a", *libraries, "-o", native], env)
        run([native], env, "VRAYLIB_DEFERRED_CLOSE_OK")
    run([sys.executable, TESTS / "run_ownership_hardening.py"], env)
    run([sys.executable, ROOT / "scripts/generate_vraylib.py", "--check"], env)


if __name__ == "__main__":
    main()
