#!/usr/bin/env python3
"""Isolated shader runtime checks (requires a desktop OpenGL 3.3 display)."""
import os
import platform
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TESTS = Path(__file__).resolve().parent
BUILD = ROOT / "build/shader-runtime-dev"
SUFFIX = ".exe" if os.name == "nt" else ""


def run(command, env, marker=None):
    result = subprocess.run([str(c) for c in command], cwd=ROOT, env=env,
                            capture_output=True, text=True, errors="replace", timeout=240)
    output = result.stdout + result.stderr
    if result.returncode or (marker and marker not in output):
        raise RuntimeError(f"Failed ({result.returncode}): {command}\n{output}")
    print(marker or f"PASS: {Path(str(command[-1])).name}")


def main():
    assert BUILD.parent.is_dir()
    BUILD.mkdir(exist_ok=True)
    env = dict(os.environ, VYNE_RAYLIB_CACHE=str(BUILD / "cache"))
    compiler = Path(os.environ.get("VYNEC", str(ROOT / ("vynec" + SUFFIX)))).resolve()
    run([compiler, "--compile", "-o", BUILD / "studio", TESTS / "shader_studio.vy"], env)
    run([BUILD / ("studio" + SUFFIX)], env, "SHADER_STUDIO_VY_OK")
    archives = list((BUILD / "cache").glob("*/lib/libraylib.a"))
    raylib = max(archives, key=lambda p: p.stat().st_mtime_ns)
    libraries = ["-lopengl32", "-lgdi32", "-lwinmm", "-lshell32", "-luser32"] if platform.system() == "Windows" else \
        ["-framework", "Cocoa", "-framework", "IOKit", "-framework", "CoreVideo", "-framework", "OpenGL", "-framework", "QuartzCore", "-lpthread"] if platform.system() == "Darwin" else \
        ["-lGL", "-lX11", "-lpthread", "-ldl", "-lm"]
    for name, source, marker in (
        ("native", "shader_runtime.c", "SHADER_RUNTIME_NATIVE_OK"),
        ("state", "shader_state.c", "SHADER_STATE_NATIVE_OK"),
    ):
        native = BUILD / (name + SUFFIX)
        run(["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-O2", TESTS / source,
             raylib, *libraries, "-o", native], env)
        run([native], env, marker)


if __name__ == "__main__":
    main()
