"""Focused optimization acceptance tests; generated artifacts stay temporary."""

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


def run(command, *, cwd, expected, required=(), forbidden=(), env=None):
    result = subprocess.run(
        [str(arg) for arg in command], cwd=cwd, capture_output=True,
        text=True, encoding="utf-8", errors="replace", timeout=120, env=env,
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


def check_v02(compiler, work, *, baseline):
    safe = HERE / "profile_safe.vy"
    warning = HERE / "profile_warning.vy"
    overflow = HERE / "profile_overflow.vy"
    if baseline:
        # Before V02 these unknown equals-form flags were silently ignored.
        run([compiler, safe, "--c-profile=diagnostic", "-o", work / "ignored_diagnostic"],
            cwd=work, expected=0, required=("V02 checksum: 10", "gcc -O3"),
            forbidden=("-Wall", "-fsanitize="))
        run([compiler, overflow, "--c-profile=sanitize", "-o", work / "ignored_sanitize"],
            cwd=work, expected=0, required=("V02 UBSan probe escaped (BUG)", "gcc -O3"),
            forbidden=("runtime error: signed integer overflow", "-fsanitize="))
        print("BASELINE V02: requested profiles ignored; emitted overflow ran without detection")
        return

    checks = 0
    run([compiler, "--help"], cwd=work, expected=0,
        required=("--c-profile", "diagnostic", "sanitize"))
    checks += 1
    run([compiler, safe, "-o", work / "safe_default"], cwd=work, expected=0,
        required=("V02 checksum: 10", "-O3 -w"), forbidden=("-fsanitize=",))
    checks += 1
    profiles = [("release", "-O3 -w"), ("diagnostic", "-O2 -g -Wall -Wextra -Wformat=2")]
    if os.name != "nt":
        profiles.append(("sanitize", "-fsanitize=address,undefined"))
    # Do not inherit local sanitizer configuration into reproducible fixture runs.
    env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1:abort_on_error=0",
           "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    reference_c = None
    for profile, flags in profiles:
        base = work / f"safe_{profile}"
        run([compiler, "--compile", safe, "--c-profile", profile, "-o", base],
            cwd=work, expected=0, required=(flags,), env=env)
        generated = Path(str(base) + ".vy.c").read_bytes()
        if reference_c is None:
            reference_c = generated
        elif generated != reference_c:
            raise AssertionError(f"{profile}: profile unexpectedly changed emitted C semantics")
        binary = Path(str(base) + (".exe" if os.name == "nt" else ""))
        run([binary], cwd=work, expected=0, required=("V02 checksum: 10",),
            forbidden=("AddressSanitizer:", "runtime error:"), env=env)
        run([compiler, safe, f"--c-profile={profile}", "-o", base],
            cwd=work, expected=0, required=("V02 checksum: 10", flags), env=env)
        print(f"PASS V02 {profile}: identical emitted C and checksum; binary bytes={binary.stat().st_size}")
        checks += 3

    # Verify real compiler diagnostics, not just a printed flag label.
    warnings = run([compiler, "--compile", warning, "--c-profile=diagnostic", "-o", work / "warning"],
                   cwd=work, expected=0, required=("warning:", "intentionally_unused"))
    print(f"PASS V02 diagnostic warning visibility: {(warnings.stdout + warnings.stderr).count('warning:')} compiler warnings surfaced")
    checks += 1
    for mode in ("build", "run"):
        required = ("-O2 -g -Wall -Wextra -Wformat=2",)
        if mode == "run":
            required += ("V02 checksum: 10",)
        run([compiler, mode, safe, "--c-profile=diagnostic", "-o", work / f"project_{mode}"],
            cwd=work, expected=0, required=required)
        checks += 1
    run([compiler, HERE / "exit_bounds_failure.vy", "--c-profile=diagnostic",
         "-o", work / "diagnostic_bounds"], cwd=work, expected=1,
        required=("VNE-072",), forbidden=("AddressSanitizer:",))
    checks += 1

    # Assembly uses the same downstream profile; emit-C and interpreter do not.
    asm_base = work / "diagnostic_assembly"
    run([compiler, "--emit-asm", safe, "--c-profile=diagnostic", "-o", asm_base],
        cwd=work, expected=0, required=("-O2 -g -Wall -Wextra -Wformat=2",))
    if not Path(str(asm_base) + ".vy.s").is_file():
        raise AssertionError("diagnostic assembly was not produced")
    checks += 1
    for args, marker in [
        (["--c-profile"], "--c-profile requires"),
        (["--c-profile", "--run"], "--c-profile requires"),
        (["--c-profile=unknown"], "unknown C profile"),
        (["--c-profile="], "unknown C profile"),
        (["--sanitizers"], "--sanitizers requires"),
        (["--sanitizers=thread", "--c-profile=sanitize"], "unsupported sanitizer list"),
        (["--sanitizers="], "unsupported sanitizer list"),
        (["--sanitizers=address"], "--sanitizers requires --c-profile sanitize"),
        (["--sanitizers=address,undefined", "--c-profile=diagnostic"], "--sanitizers requires"),
        (["--ast", "--c-profile=diagnostic"], "require a desktop"),
        (["--emit-c", "--c-profile=sanitize"], "require a desktop"),
        (["--arduino", "uno", "--c-profile=diagnostic"], "require a desktop"),
        (["--esp32", "esp32", "--c-profile=sanitize"], "require a desktop"),
    ]:
        run([compiler, safe, *args, "-o", work / "rejected"], cwd=work,
            expected=2, required=(marker,))
        checks += 1
    run([compiler, "bench", safe, "--c-profile=diagnostic"], cwd=work, expected=2,
        required=("are not supported by bench",))
    checks += 1

    if os.name == "nt":
        run([compiler, safe, "--c-profile=sanitize", "-o", work / "unsupported"],
            cwd=work, expected=2, required=("unsupported by the Windows GCC backend",))
        checks += 1
        print("PASS V02 Windows sanitizer request explicitly rejected; no sanitizer execution claimed")
    else:
        sanitized_asm = work / "sanitized_assembly"
        run([compiler, "--emit-asm", safe, "--c-profile=sanitize", "-o", sanitized_asm],
            cwd=work, expected=0, required=("-fsanitize=address,undefined",), env=env)
        if "__asan" not in Path(str(sanitized_asm) + ".vy.s").read_text():
            raise AssertionError("sanitized assembly contains no address instrumentation")
        checks += 1
        run([compiler, HERE / "exit_bounds_failure.vy", "--c-profile=sanitize",
             "--sanitizers=undefined,address", "-o", work / "sanitized_bounds"],
            cwd=work, expected=1, required=("VNE-072", "-fsanitize=address,undefined"),
            forbidden=("AddressSanitizer:",), env=env)
        checks += 1
        # Independent detector selection proves both runtimes work; combined mode
        # was exercised with valid output above and remains the default.
        for source, marker, escaped, extra in [
            (overflow, "runtime error: signed integer overflow", "V02 UBSan probe escaped (BUG)",
             ["--sanitizers", "undefined"]),
            (HERE / "profile_address.vy", "AddressSanitizer: stack-buffer-overflow", "V02 ASan probe escaped (BUG)",
             ["--sanitizers=address", "--no-scratch-bounds"]),
        ]:
            base = work / source.stem
            run([compiler, "--compile", source, "--c-profile=sanitize", *extra, "-o", base],
                cwd=work, expected=0, required=("-fsanitize=",), env=env)
            run([base], cwd=work, expected=1, required=(marker,), forbidden=(escaped,), env=env)
            run([compiler, source, "--c-profile=sanitize", *extra, "-o", base], cwd=work,
                expected=1, required=(marker, "exited with code 1"), forbidden=(escaped,), env=env)
            checks += 3
            print(f"PASS V02 emitted-runtime detector: {source.name}")
    print(f"PASS V02: {checks} acceptance checks")


def check_v03(compiler, work, *, baseline):
    if baseline:
        run([sys.executable, HERE / "v03_runner_tests.py", "--baseline"],
            cwd=ROOT, expected=0, required=("BASELINE V03 preserved:",))
        return
    run([sys.executable, HERE / "v03_runner_tests.py"], cwd=ROOT, expected=0,
        required=("PASS V03 runner regressions: 30 checks",))
    result = run([sys.executable, ROOT / "scripts/run_tests.py", "--suite", "compiler",
                  "--suite", "safety", "--suite", "codegen", "--suite", "ast",
                  "--compiler", compiler], cwd=ROOT, expected=0,
                 required=("Registered tests: 17 executed, 17 passed, 0 failed",))
    print(ANSI.sub("", result.stdout).strip())
    print("PASS V03: 30 adversarial runner regressions and 17 registered core cases")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--task", choices=["V01", "V02", "V03", "V04", "all"], default="all")
    parser.add_argument("--compiler", default=os.environ.get(
        "VYNEC", str(ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--baseline", action="store_true",
                        help="Assert the selected task's pre-fix behavior; not acceptance")
    args = parser.parse_args()
    if args.baseline and args.task == "all":
        parser.error("--baseline requires a specific --task")
    compiler = Path(args.compiler).resolve()
    if not compiler.is_file():
        parser.error(f"compiler not found: {compiler}; build first or set VYNEC")
    # Space-containing output paths are deliberate; both host shells must handle them.
    artifacts = ROOT / "build" / "optimization"
    artifacts.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="vyne optimization ", dir=artifacts) as temporary:
        if args.task in ("V01", "all"):
            check_v01(compiler, Path(temporary), baseline=args.baseline, cxx=args.cxx)
        if args.task in ("V02", "all"):
            check_v02(compiler, Path(temporary), baseline=args.baseline)
        if args.task in ("V03", "all"):
            check_v03(compiler, Path(temporary), baseline=args.baseline)
        if args.task in ("V04", "all"):
            command = [sys.executable, HERE / "v04_benchmark_tests.py", "--compiler", compiler,
                       "--cxx", args.cxx]
            if args.baseline:
                command.append("--baseline")
            result = run(command, cwd=ROOT, expected=0,
                         required=("BASELINE V04 reproduced:" if args.baseline else "PASS V04:",))
            print(ANSI.sub("", result.stdout).strip())


if __name__ == "__main__":
    main()
