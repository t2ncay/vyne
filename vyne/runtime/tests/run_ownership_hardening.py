#!/usr/bin/env python3
"""Optimized ownership cleanup and fault-injected atomic-write regressions."""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TESTS = Path(__file__).resolve().parent
BUILD = ROOT / "build/runtime-ownership-hardening"
SUFFIX = ".exe" if os.name == "nt" else ""


def run(command, env, marker=None):
    result = subprocess.run([str(c) for c in command], cwd=ROOT, env=env,
                            capture_output=True, text=True, errors="replace", timeout=240)
    output = result.stdout + result.stderr
    if result.returncode or marker and marker not in output:
        raise RuntimeError(f"Failed ({result.returncode}): {command}\n{output}")
    print(marker or f"PASS: {Path(str(command[-1])).name}")


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    compiler = Path(os.environ.get("VYNEC", str(ROOT / ("vynec" + SUFFIX)))).resolve()
    env = dict(os.environ, VYNE_RAYLIB_CACHE=str(BUILD / "cache"))
    binary = BUILD / "ownership"
    run([compiler, "--compile", "-o", binary, TESTS / "exception_ownership.vy"], env)
    emitted = binary.with_suffix(".vy.c").read_text()
    for declaration in (
        r"int64_t volatile v_raw_cleanup_raw",
        r"int64_t volatile v_catch_cleanup_raw",
        r"vyne_vraylib_App volatile v_partial_startup_app",
        r"vyne_vraylib_Image volatile v_partial_startup_image",
        r"VyneValue volatile v_boxed_startup_owner",
    ):
        assert re.search(declaration, emitted), f"Missing longjmp-safe declaration: {declaration}"
    run([str(binary) + SUFFIX], env, "EXCEPTION_OWNERSHIP_OK")
    with tempfile.TemporaryDirectory(prefix="atomic-", dir=BUILD) as directory:
        path = Path(directory) / "document.vscene"
        env["VYNE_ATOMIC_TEST_PATH"] = str(path)
        binary = BUILD / "atomic"
        run([compiler, "--compile", "-o", binary, TESTS / "vfs_atomic.vy"], env)
        run([str(binary) + SUFFIX], env, "VFS_ATOMIC_VY_OK")
        native = BUILD / ("atomic_failure" + SUFFIX)
        run(["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-O3", TESTS / "vfs_atomic_failure.c", "-lm", "-o", native], env)
        run([native, path], env, "VFS_ATOMIC_FAILURE_OK")
        assert not list(Path(directory).iterdir()), "Atomic-write temporary leaked"


if __name__ == "__main__":
    main()
