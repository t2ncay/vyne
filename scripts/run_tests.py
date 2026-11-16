"""Run registered Vyne regression suites with phase-specific outcome contracts."""

import argparse
from dataclasses import dataclass
import difflib
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import tomllib


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "scripts/tests/suites.toml"
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
KINDS = {"native", "runtime_failure", "compile_failure", "interpret", "golden", "command"}


class TestFailure(RuntimeError):
    pass


@dataclass
class Outcome:
    returncode: int
    stdout: str = ""
    stderr: str = ""

    @property
    def output(self):
        return ANSI.sub("", self.stdout + "\n" + self.stderr)


def execute(command, *, cwd, timeout, env=None):
    """Never count a timed-out process as success, even if it printed a marker."""
    options = ({"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
               if os.name == "nt" else {"start_new_session": True})
    try:
        process = subprocess.Popen(
            [str(arg) for arg in command], cwd=cwd, env=env, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            encoding="utf-8", errors="replace", **options,
        )
    except OSError as error:
        raise TestFailure(f"cannot launch {command}: {error}") from error
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        if os.name == "nt":
            # Kill descendants too: vynec may still be waiting for GCC or a user exe.
            try:
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               timeout=10, check=False)
            finally:
                if process.poll() is None:
                    process.kill()
        else:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        stdout, stderr = process.communicate(timeout=10)
        raise TestFailure(f"timeout after {timeout}s: {command}\n{stdout}{stderr}")
    return Outcome(process.returncode, stdout, stderr)


def assert_outcome(result, expected, required, forbidden, phase):
    if result.returncode != expected:
        raise TestFailure(f"{phase}: expected exit {expected}, got {result.returncode}\n{result.output}")
    for pattern in required:
        if not re.search(pattern, result.output):
            raise TestFailure(f"{phase}: missing required output {pattern!r}\n{result.output}")
    for pattern in forbidden:
        if re.search(pattern, result.output):
            raise TestFailure(f"{phase}: forbidden output {pattern!r}\n{result.output}")


def source_path(root, name):
    path = (root / name).resolve()
    if not path.is_relative_to(root.resolve()):
        raise TestFailure(f"fixture is outside test root: {name}")
    if not path.is_file():
        raise TestFailure(f"missing fixture: {name}")
    return path


def load_cases(manifest, root, suites):
    try:
        data = tomllib.loads(Path(manifest).read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as error:
        raise TestFailure(f"cannot load manifest {manifest}: {error}") from error
    if type(data.get("version")) is not int or data["version"] != 1:
        raise TestFailure("test manifest requires version = 1")
    definitions = data.get("suites", {})
    if not isinstance(definitions, dict):
        raise TestFailure("suites must be a table")
    requested = list(definitions) if suites == ["all"] else suites
    if not requested or len(requested) != len(set(requested)):
        raise TestFailure("select one or more distinct registered suites")
    cases = data.get("cases", [])
    if not isinstance(cases, list):
        raise TestFailure("cases must be an array of tables")
    seen = set()
    for case in cases:
        if not isinstance(case, dict):
            raise TestFailure("each case must be a table")
        identifier = case.get("id", "")
        if not isinstance(identifier, str) or not re.fullmatch(r"[A-Za-z0-9_-]+", identifier) or identifier in seen:
            raise TestFailure(f"invalid or duplicate case id: {identifier!r}")
        seen.add(identifier)
        if case.get("suite") not in definitions or case.get("kind") not in KINDS:
            raise TestFailure(f"{identifier}: unknown suite or outcome kind")
        if "skip" in case:
            raise TestFailure(f"{identifier}: skipped cases are not outcome contracts")
        if not isinstance(case.get("source"), str) or not case["source"]:
            raise TestFailure(f"{identifier}: source path is missing")
        if not isinstance(case.get("required"), list) or not case["required"]:
            raise TestFailure(f"{identifier}: required output contract is missing")
        if not isinstance(case.get("forbidden", []), list):
            raise TestFailure(f"{identifier}: forbidden patterns must be a list")
        for pattern in case["required"] + case.get("forbidden", []):
            if not isinstance(pattern, str) or not pattern:
                raise TestFailure(f"{identifier}: output patterns must be nonempty strings")
            try:
                re.compile(pattern)
            except (re.error, TypeError) as error:
                raise TestFailure(f"{identifier}: invalid output pattern: {error}") from error
        expected = case.get("expected_exit", 0)
        if not isinstance(expected, int) or isinstance(expected, bool):
            raise TestFailure(f"{identifier}: expected_exit must be an integer")
        negative = case["kind"] in {"compile_failure", "runtime_failure"}
        if (negative and expected <= 0) or (not negative and expected != 0):
            raise TestFailure(f"{identifier}: exit status contradicts outcome kind")
        timeout = case.get("timeout", 120)
        if not isinstance(timeout, (int, float)) or isinstance(timeout, bool) or not math.isfinite(timeout) or timeout <= 0:
            raise TestFailure(f"{identifier}: timeout must be positive")
        if case["kind"] == "golden" and not isinstance(case.get("golden"), str):
            raise TestFailure(f"{identifier}: golden path is missing")
        if case["kind"] == "command":
            tokens = case.get("command")
            if not isinstance(tokens, list) or not tokens or any(not isinstance(token, str) or not token for token in tokens):
                raise TestFailure(f"{identifier}: command must be a nonempty argument list")
            try:
                for token in tokens:
                    token.format(python="python", root="root", compiler="compiler", source="source")
            except (KeyError, ValueError) as error:
                raise TestFailure(f"{identifier}: invalid command placeholder: {error}") from error

    selected = []
    for suite in requested:
        if suite not in definitions:
            raise TestFailure(f"no registered automated suite {suite!r}; available: {', '.join(definitions)}")
        members = [case for case in cases if case["suite"] == suite]
        if not members:
            raise TestFailure(f"{suite}: no test cases registered")
        discovered = set()
        definition = definitions[suite]
        if not isinstance(definition, dict) or not isinstance(definition.get("patterns"), list):
            raise TestFailure(f"{suite}: discovery patterns must be a list")
        for pattern in definition["patterns"]:
            if not isinstance(pattern, str) or not pattern:
                raise TestFailure(f"{suite}: discovery patterns must be nonempty strings")
            discovered.update(path.resolve() for path in root.glob(pattern) if path.is_file())
        if not discovered:
            raise TestFailure(f"{suite}: discovery matched zero fixtures")
        registered = set()
        for case in members:
            registered.add(source_path(root, case["source"]))
            if case["kind"] == "golden":
                source_path(root, case.get("golden", ""))
        unknown = discovered - registered
        missing = registered - discovered
        if unknown or missing:
            raise TestFailure(f"{suite}: discovery/contract mismatch; unregistered={sorted(map(str, unknown))}; "
                              f"not discovered={sorted(map(str, missing))}")
        selected.extend(members)
    if not selected:
        raise TestFailure("no executable test cases selected")
    return selected


def normalize_c(content):
    # Normalize only the machine-specific runtime include and insignificant layout.
    # Do not inline temps, remove guards, or otherwise erase codegen differences.
    def runtime_include(match):
        path = match.group(1).replace("\\", "/")
        marker = "/vyne/runtime/"
        if marker in path:
            return '#include "vyne/runtime/' + path.split(marker, 1)[1] + '"'
        return match.group(0)
    content = re.sub(r'^#include "([^"\n]+)"$', runtime_include, content, flags=re.MULTILINE)
    return "\n".join(line.rstrip() for line in content.splitlines() if line.strip()) + "\n"


def run_case(case, compiler, root, artifacts):
    source = source_path(root, case["source"])
    base = artifacts / case["id"]
    binary = Path(str(base) + (".exe" if os.name == "nt" else ""))
    required = case["required"]
    forbidden = [r"(?m)^FAIL\b", r"\(BUG\)", *case.get("forbidden", [])]
    timeout = case.get("timeout", 120)
    kind = case["kind"]
    if kind == "command":
        command = [arg.format(python=sys.executable, root=str(root), compiler=str(compiler), source=str(source))
                   for arg in case["command"]]
        env = {**os.environ, "VYNEC": str(compiler)}
        result = execute(command, cwd=root, timeout=timeout, env=env)
        assert_outcome(result, 0, required, forbidden, "suite command")
        return
    if kind == "interpret":
        result = execute([compiler, "--ast", source], cwd=root, timeout=timeout)
        assert_outcome(result, 0, required, forbidden, "interpreter")
        return

    compile_result = execute([compiler, "--compile", source, "-o", base], cwd=root, timeout=timeout)
    if kind == "compile_failure":
        assert_outcome(compile_result, case["expected_exit"], required, forbidden, "compile rejection")
        if binary.exists():
            raise TestFailure("compile rejection produced an executable")
        return
    assert_outcome(compile_result, 0, [], [], "compilation")
    if not binary.is_file():
        raise TestFailure(f"compilation succeeded but executable is missing: {binary}")
    if kind == "golden":
        generated = Path(str(base) + ".vy.c")
        expected = source_path(root, case["golden"])
        if not generated.is_file():
            raise TestFailure("generated C is missing")
        actual_c = normalize_c(generated.read_text(encoding="utf-8"))
        expected_c = normalize_c(expected.read_text(encoding="utf-8"))
        if actual_c != expected_c:
            diff = "".join(difflib.unified_diff(expected_c.splitlines(True), actual_c.splitlines(True),
                                              fromfile=str(expected), tofile=str(generated)))
            raise TestFailure(f"generated C differs from golden\n{diff}")
    runtime_result = execute([binary], cwd=root, timeout=timeout)
    assert_outcome(runtime_result, case.get("expected_exit", 0), required, forbidden, "generated executable")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", action="append", default=[])
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--compiler", type=Path, default=Path(os.environ.get(
        "VYNEC", ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))))
    parser.add_argument("--list", action="store_true", help="Validate discovery/contracts and list selected cases")
    parser.add_argument("--source", type=Path, help="Select a source already registered in the requested suites")
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        cases = load_cases(args.manifest, root, args.suite or ["compiler", "safety", "codegen", "ast"])
        if args.source:
            requested_source = args.source.resolve()
            cases = [case for case in cases if source_path(root, case["source"]) == requested_source]
            if not cases:
                raise TestFailure(f"source has no registered contract in requested suites: {requested_source}")
        if args.list:
            for case in cases:
                print(f"{case['suite']}/{case['id']}: {case['kind']} {case['source']}")
            print(f"Discovered {len(cases)} registered cases")
            return 0
        compiler = args.compiler.resolve()
        if not compiler.is_file():
            raise TestFailure(f"compiler not found: {compiler}; build first or set VYNEC")
        storage = root / "build" / "test-runs"
        storage.mkdir(parents=True, exist_ok=True)
        failed = 0
        with tempfile.TemporaryDirectory(prefix="vyne tests ", dir=storage) as directory:
            for case in cases:
                try:
                    run_case(case, compiler, root, Path(directory))
                    print(f"PASS {case['suite']}/{case['id']} ({case['kind']})", flush=True)
                except (TestFailure, OSError) as error:
                    failed += 1
                    print(f"FAIL {case['suite']}/{case['id']}: {error}", flush=True)
        print(f"Registered tests: {len(cases)} executed, {len(cases) - failed} passed, {failed} failed")
        return 1 if failed else 0
    except TestFailure as error:
        print(f"test runner error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
