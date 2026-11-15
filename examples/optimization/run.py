"""Focused optimization acceptance tests; generated artifacts stay temporary."""

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


def run(command, *, cwd, expected, required=(), forbidden=()):
    result = subprocess.run(
        [str(arg) for arg in command], cwd=cwd, capture_output=True,
        text=True, encoding="utf-8", errors="replace", timeout=120,
    )
    output = ANSI.sub("", result.stdout + result.stderr)
    if result.returncode != expected:
        raise AssertionError(
            f"{command}: expected exit {expected}, got {result.returncode}\n{output}"
        )
    for marker in required:
        if marker not in output:
            raise AssertionError(f"{command}: missing {marker!r}\n{output}")
    for marker in forbidden:
        if marker in output:
            raise AssertionError(f"{command}: unexpected {marker!r}\n{output}")
    return result


def check_v01(compiler, work, *, baseline, cxx):
    cases = [
        ("exit_success", 0, "V01 success reached"),
        ("exit_seven", 7, "V01 exit seven reached"),
        ("exit_twenty_three", 23, "V01 exit twenty-three reached"),
        ("exit_bounds_failure", 1, "VNE-072"),
    ]
    checks = 0
    for name, status, marker in cases:
        source = HERE / f"{name}.vy"
        base = work / name
        binary = Path(str(base) + (".exe" if os.name == "nt" else ""))
        # Compile-only must succeed even if the resulting application exits nonzero.
        run([compiler, "--compile", source, "-o", base], cwd=work, expected=0,
            forbidden=("V01 unreachable (BUG)", "V01 success reached",
                       "V01 exit seven reached", "V01 exit twenty-three reached"))
        checks += 1
        run([binary], cwd=work, expected=status, required=(marker,),
            forbidden=("V01 unreachable (BUG)",))
        checks += 1
        for mode in ([], ["--run"]):
            cli_status = 0 if baseline else status
            # Baseline POSIX output prints the encoded wait status. Windows does not.
            displayed = status * 256 if baseline and os.name != "nt" else status
            required = (marker,) + ((f"exited with code {displayed}",) if status else ())
            run([compiler, *mode, source, "-o", base], cwd=work,
                expected=cli_status, required=required,
                forbidden=("V01 unreachable (BUG)",))
            checks += 1
        print(f"PASS V01 {name}: binary={status}, CLI={0 if baseline else status}, both run modes")

    if baseline:
        print(f"BASELINE reproduced: {checks} checks; nonzero child exits masked by CLI")
        return

    contract = work / ("process_status.exe" if os.name == "nt" else "process_status")
    run([*shlex.split(cxx, posix=os.name != "nt"), "-std=c++23", "-O2",
         "-Wall", "-Wextra", "-pedantic", "-I", ROOT,
         HERE / "process_status.cpp", "-o", contract], cwd=work, expected=0)
    run([contract], cwd=work, expected=0, required=("V01 process status contract: PASS",))
    checks += 2
    for source, marker in [
        (ROOT / "examples/transpiler/native_zero_side_effects.vy", "nativeZero side effects: PASS"),
        (ROOT / "examples/test/codegen_peepholes.vy", "=== done ==="),
    ]:
        run([compiler, "--run", source, "-o", work / source.stem], cwd=work,
            expected=0, required=(marker,), forbidden=("false", "False"))
        checks += 1
        print(f"PASS V01 existing regression: {source.name}")
    print(f"PASS V01: {checks} checks including platform process-status contract")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--task", choices=["V01"], default="V01")
    parser.add_argument("--compiler", default=os.environ.get(
        "VYNEC", str(ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--baseline", action="store_true",
                        help="Assert the pre-V01 false-success behavior; not acceptance")
    args = parser.parse_args()
    compiler = Path(args.compiler).resolve()
    if not compiler.is_file():
        parser.error(f"compiler not found: {compiler}; build first or set VYNEC")
    # Space-containing output paths are deliberate; both host shells must handle them.
    artifacts = ROOT / "build" / "optimization"
    artifacts.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="vyne optimization ", dir=artifacts) as temporary:
        check_v01(compiler, Path(temporary), baseline=args.baseline, cxx=args.cxx)


if __name__ == "__main__":
    main()
