#!/usr/bin/env python3
"""Instrument test-only emitted C to observe allocator mode, without a new API."""
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUILD = ROOT / "build/runtime-vraylib-blockers"


def check_region_state(compiler, env, run, native_dir, libraries):
    binary = BUILD / "region_state"
    run([compiler, "--compile", "-o", binary, Path(__file__).with_name("region_exception_state.vy")], env)
    if native_dir is None:
        native_dir = max((BUILD / "cache").glob("*/lib/libvyne_raylib.a"), key=lambda p: p.stat().st_mtime_ns).parent
    emitted = binary.with_suffix(".vy.c").read_text()
    # These observation points are the only changes to compiler-produced C.
    # Calls, exception lowering and policy cleanup remain exactly as emitted.
    pattern = r'vyne_out\(vyne_string_static\("EXPECT_ZERO_MODE_(\d+)"\)\);'
    instrumented, count = re.subn(pattern, lambda m: f'assert(g_arena_zero_mode == {m[1]});', emitted)
    assert count >= 10, f"Missing allocator-mode observation points: {count}"
    source = BUILD / "region_state_observed.c"
    source.write_text("#include <assert.h>\n" + instrumented)
    executable = BUILD / ("region_state_observed.exe" if os.name == "nt" else "region_state_observed")
    run(["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-O3", source,
         native_dir / "libvyne_raylib.a", native_dir / "libraylib.a", *libraries, "-o", executable], env)
    run([executable], env, "REGION_EXCEPTION_STATE_OK")


if __name__ == "__main__":
    # The focused runner also runs this check as part of the full blocker set.
    from run_vraylib_blockers import run
    import platform
    BUILD.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, VYNE_RAYLIB_CACHE=str(BUILD / "cache"))
    compiler = Path(os.environ.get("VYNEC", str(ROOT / ("vynec.exe" if os.name == "nt" else "vynec")))).resolve()
    libraries = ["-lopengl32", "-lgdi32", "-lwinmm", "-lshell32", "-luser32"] if platform.system() == "Windows" else \
        ["-framework", "Cocoa", "-framework", "IOKit", "-framework", "CoreVideo", "-framework", "OpenGL", "-framework", "QuartzCore", "-lpthread"] if platform.system() == "Darwin" else \
        ["-lGL", "-lX11", "-lpthread", "-ldl", "-lm"]
    check_region_state(compiler, env, run, None, libraries)
