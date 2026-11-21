#!/usr/bin/env python3
"""Run the maintained VMGE acceptance with isolated outputs and fixture paths."""
import importlib.util
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUILD = ROOT / "build/runtime-ownership-hardening/vmge-acceptance"
RUNNER = ROOT / "examples/projects/vmge/tests/run.py"


def main():
    spec = importlib.util.spec_from_file_location("vmge_acceptance", RUNNER)
    suite = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(suite)
    suite.BUILD = BUILD
    os.environ["VYNE_RAYLIB_CACHE"] = str(BUILD.parent / "cache")
    fixtures = BUILD / "fixtures"
    fixtures.mkdir(parents=True, exist_ok=True)
    copies = {}
    for name in ("map", "render"):
        original = suite.PROJECT / "tests" / (name + ".vy")
        text = original.read_text().replace("build/vmge-tests", BUILD.relative_to(ROOT).as_posix())
        # Keep imports pointing to production source after relocating only the
        # two fixtures whose generated-file destinations were hardcoded.
        text = re.sub(r'^use "([^"]+)";',
                      lambda m: 'use "' + (original.parent / m[1]).resolve().as_posix() + '";',
                      text, flags=re.MULTILINE)
        copy = fixtures / (name + ".vy")
        copy.write_text(text)
        copies[original.resolve()] = copy
    original_run = suite.run

    def isolated_run(command, *args, **kwargs):
        command = list(command)
        if "--compile" in command:
            source = Path(command[-1]).resolve()
            if source in copies:
                command[-1] = copies[source]
        return original_run(command, *args, **kwargs)

    suite.run = isolated_run
    suite.main()


if __name__ == "__main__":
    main()
