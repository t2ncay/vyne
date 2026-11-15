"""V03: adversarial discovery, phase, output, golden, and timeout regressions."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import run_tests as runner


class RunnerRegressions(unittest.TestCase):
    def setUp(self):
        storage = ROOT / "build/optimization"
        storage.mkdir(parents=True, exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(prefix="V03 contracts ", dir=storage)
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "sample.vy").write_text('out("EXPECTED");\n', encoding="utf-8")
        self.case = {"id": "sample", "suite": "probe", "source": "sample.vy",
                     "kind": "native", "required": ["EXPECTED"]}

    def manifest(self, case=None, *, patterns='["*.vy"]', cases=None):
        case = self.case if case is None else case
        if cases is None:
            fields = []
            for key, value in case.items():
                if isinstance(value, str):
                    fields.append(f'{key} = "{value}"')
                elif isinstance(value, list):
                    fields.append(f'{key} = [{", ".join(repr(item) for item in value)}]')
                elif isinstance(value, bool):
                    fields.append(f'{key} = {str(value).lower()}')
                else:
                    fields.append(f'{key} = {value}')
            cases = "[[cases]]\n" + "\n".join(fields)
        path = self.root / "suite.toml"
        path.write_text(f'version = 1\n[suites.probe]\npatterns = {patterns}\n{cases}\n', encoding="utf-8")
        return path

    def load(self, **kwargs):
        return runner.load_cases(self.manifest(**kwargs), self.root, ["probe"])

    def test_exact_success_output(self):
        runner.assert_outcome(runner.Outcome(0, "EXPECTED"), 0, ["EXPECTED"], [], "positive")

    def test_matching_output_does_not_mask_nonzero_exit(self):
        result = runner.execute([sys.executable, "-c", "print('EXPECTED'); raise SystemExit(7)"],
                                cwd=self.root, timeout=10)
        with self.assertRaisesRegex(runner.TestFailure, "expected exit 0, got 7"):
            runner.assert_outcome(result, 0, ["EXPECTED"], [], "positive")

    def test_zero_exit_without_success_marker_fails(self):
        result = runner.execute([sys.executable, "-c", "print('Syntax Error')"], cwd=self.root, timeout=10)
        with self.assertRaisesRegex(runner.TestFailure, "missing required output"):
            runner.assert_outcome(result, 0, ["EXPECTED"], [], "positive")

    def test_runtime_error_text_with_zero_exit_fails(self):
        with self.assertRaisesRegex(runner.TestFailure, "expected exit 1, got 0"):
            runner.assert_outcome(runner.Outcome(0, "VNE-072"), 1, ["VNE-072"], [], "runtime")

    def test_wrong_nonzero_exit_fails(self):
        with self.assertRaisesRegex(runner.TestFailure, "expected exit 1, got 2"):
            runner.assert_outcome(runner.Outcome(2, "VNE-072"), 1, ["VNE-072"], [], "runtime")

    def test_wrong_diagnostic_is_not_compile_success(self):
        with self.assertRaisesRegex(runner.TestFailure, "missing required output"):
            runner.assert_outcome(runner.Outcome(1, "Syntax Error"), 1, ["VNE-070"], [], "compile")

    def test_forbidden_failure_marker(self):
        with self.assertRaisesRegex(runner.TestFailure, "forbidden output"):
            runner.assert_outcome(runner.Outcome(0, "EXPECTED\nFAIL incorrect result"),
                                  0, ["EXPECTED"], [r"(?m)^FAIL\b"], "positive")

    def test_successful_compilation_requires_executable(self):
        with patch.object(runner, "execute", return_value=runner.Outcome(0)):
            with self.assertRaisesRegex(runner.TestFailure, "executable is missing"):
                runner.run_case(self.case, Path(sys.executable), self.root, self.root)

    def test_compiler_failure_cannot_satisfy_runtime_contract(self):
        case = {**self.case, "kind": "runtime_failure", "expected_exit": 1, "required": ["VNE-072"]}
        with patch.object(runner, "execute", return_value=runner.Outcome(1, "VNE-072")) as execute:
            with self.assertRaisesRegex(runner.TestFailure, "compilation: expected exit 0"):
                runner.run_case(case, Path(sys.executable), self.root, self.root)
            self.assertEqual(execute.call_count, 1)

    def test_compile_rejection_cannot_leave_executable(self):
        binary = self.root / ("sample.exe" if os.name == "nt" else "sample")
        binary.write_bytes(b"not a rejected build")
        case = {**self.case, "kind": "compile_failure", "expected_exit": 1, "required": ["VNE-070"]}
        with patch.object(runner, "execute", return_value=runner.Outcome(1, "VNE-070")):
            with self.assertRaisesRegex(runner.TestFailure, "produced an executable"):
                runner.run_case(case, Path(sys.executable), self.root, self.root)

    def test_empty_suite_fails(self):
        with self.assertRaisesRegex(runner.TestFailure, "no test cases registered"):
            self.load(cases="")

    def test_empty_discovery_fails(self):
        with self.assertRaisesRegex(runner.TestFailure, "matched zero fixtures"):
            self.load(patterns='["absent/*.vy"]')

    def test_missing_fixture_fails(self):
        with self.assertRaisesRegex(runner.TestFailure, "missing fixture"):
            self.load(case={**self.case, "source": "missing.vy"})

    def test_unregistered_discovered_fixture_fails(self):
        (self.root / "extra.vy").write_text("out(1);\n", encoding="utf-8")
        with self.assertRaisesRegex(runner.TestFailure, "discovery/contract mismatch"):
            self.load()

    def test_duplicate_ids_fails(self):
        content = self.manifest().read_text(encoding="utf-8").split("[[cases]]", 1)[1]
        with self.assertRaisesRegex(runner.TestFailure, "duplicate case id"):
            self.load(cases="[[cases]]" + content + "\n[[cases]]" + content)

    def test_missing_golden_fails_before_compilation(self):
        with self.assertRaisesRegex(runner.TestFailure, "missing fixture"):
            self.load(case={**self.case, "kind": "golden", "golden": "missing.expected.c"})

    def test_empty_output_pattern_rejected(self):
        with self.assertRaisesRegex(runner.TestFailure, "nonempty strings"):
            self.load(case={**self.case, "required": [""]})

    def test_missing_output_contract_rejected(self):
        with self.assertRaisesRegex(runner.TestFailure, "output contract is missing"):
            self.load(case={**self.case, "required": []})

    def test_all_skipped_cannot_pass(self):
        with self.assertRaisesRegex(runner.TestFailure, "skipped cases"):
            self.load(case={**self.case, "skip": True})

    def test_unknown_outcome_kind_rejected(self):
        with self.assertRaisesRegex(runner.TestFailure, "unknown suite or outcome kind"):
            self.load(case={**self.case, "kind": "anything"})

    def test_unknown_requested_suite_rejected(self):
        with self.assertRaisesRegex(runner.TestFailure, "no registered automated suite"):
            runner.load_cases(self.manifest(), self.root, ["absent"])

    def test_invalid_timeout_rejected(self):
        with self.assertRaisesRegex(runner.TestFailure, "timeout must be positive"):
            self.load(case={**self.case, "timeout": -1})

    def test_fixture_cannot_escape_root(self):
        with self.assertRaisesRegex(runner.TestFailure, "outside test root"):
            runner.source_path(self.root, "../foreign.vy")

    def test_golden_difference_fails(self):
        (self.root / "sample.expected.c").write_text("int main(void) { return 0; }\n", encoding="utf-8")
        (self.root / "sample.vy.c").write_text("int main(void) { return 9; }\n", encoding="utf-8")
        (self.root / ("sample.exe" if os.name == "nt" else "sample")).write_bytes(b"placeholder")
        case = {**self.case, "kind": "golden", "golden": "sample.expected.c"}
        with patch.object(runner, "execute", return_value=runner.Outcome(0)):
            with self.assertRaisesRegex(runner.TestFailure, "differs from golden"):
                runner.run_case(case, Path(sys.executable), self.root, self.root)

    def test_normalization_preserves_guards_and_runtime_header(self):
        windows = '#include "C:\\repo\\vyne\\runtime\\modules\\vmem.h"\nif (i < 3) exit(1);\n'
        linux = '#include "/repo/vyne/runtime/modules/vmem.h"\nif (i < 3) exit(1);\n'
        self.assertEqual(runner.normalize_c(windows), runner.normalize_c(linux))
        self.assertNotEqual(runner.normalize_c(linux), runner.normalize_c(linux.replace("i < 3", "i < 4")))

    def test_timeout_after_marker_kills_process_tree(self):
        code = "import subprocess,sys,time; print('EXPECTED',flush=True); subprocess.Popen([sys.executable,'-c','import time; time.sleep(60)']); time.sleep(60)"
        start = time.monotonic()
        with self.assertRaisesRegex(runner.TestFailure, "timeout after"):
            runner.execute([sys.executable, "-c", code], cwd=self.root, timeout=1)
        self.assertLess(time.monotonic() - start, 15)

    def test_real_registered_inventory(self):
        cases = runner.load_cases(runner.MANIFEST, ROOT, ["compiler", "safety", "codegen", "ast"])
        self.assertEqual(len(cases), 16)
        self.assertEqual(sum(case["suite"] == "safety" for case in cases), 10)

    def test_cli_lists_registered_source_without_compiler(self):
        result = runner.execute([sys.executable, ROOT / "scripts/run_tests.py", "--manifest", self.manifest(),
                                 "--root", self.root, "--suite", "probe", "--source", self.root / "sample.vy", "--list"],
                                cwd=self.root, timeout=10)
        runner.assert_outcome(result, 0, ["Discovered 1 registered cases"], [], "list")

    def test_cli_source_filter_cannot_select_zero_cases(self):
        result = runner.execute([sys.executable, ROOT / "scripts/run_tests.py", "--manifest", self.manifest(),
                                 "--root", self.root, "--suite", "probe", "--source", self.root / "missing.vy", "--list"],
                                cwd=self.root, timeout=10)
        runner.assert_outcome(result, 2, ["source has no registered contract"], [], "selection rejection")

    def test_cli_unknown_suite_returns_failure(self):
        result = runner.execute([sys.executable, ROOT / "scripts/run_tests.py", "--manifest", self.manifest(),
                                 "--root", self.root, "--suite", "absent", "--list"], cwd=self.root, timeout=10)
        runner.assert_outcome(result, 2, ["no registered automated suite"], [], "suite rejection")


def baseline(original=False, compare=False):
    if os.name != "nt":
        raise RuntimeError("the preserved PowerShell baseline requires Windows")
    storage = ROOT / "build/optimization"
    storage.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="V03 baseline ", dir=storage) as temporary:
        root = Path(temporary)
        shim = root / "fake_compiler.cmd"
        shim.write_text(f'@"{sys.executable}" "{HERE / "fixtures/fake_safety_compiler.py"}" %*\n', encoding="utf-8")
        env = {**os.environ, "VYNEC": str(shim)}
        powershell = shutil.which("pwsh") or shutil.which("powershell")
        if not powershell:
            raise RuntimeError("PowerShell is unavailable")
        # Exercise the original local runner before replacement when requested,
        # and retain the acceptance predicate fixture for subsequent reproduction.
        for name in (("original", "preserved") if original else ("preserved",)):
            if name == "original":
                script = ROOT / "examples/safety/run_safety.ps1"
            else:
                directory = root / "examples/safety"
                directory.mkdir(parents=True)
                script = directory / "run_safety.ps1"
                script.write_text((HERE / "fixtures/legacy_safety.ps1.in").read_text(), encoding="utf-8")
            result = subprocess.run([powershell, "-NoProfile", "-File", script],
                                    capture_output=True, text=True, env=env, timeout=60)
            if result.returncode != 0 or "10 passed, 0 failed" not in result.stdout:
                raise RuntimeError(f"{name} baseline not reproduced\n{result.stdout}{result.stderr}")
            print(f"BASELINE V03 {name}: 10/10 accepted although the compiler double always exits 1")
        if compare:
            result = subprocess.run([powershell, "-NoProfile", "-File", ROOT / "examples/safety/run_safety.ps1"],
                                    capture_output=True, text=True, env=env, timeout=60)
            if result.returncode != 1 or "10 executed, 4 passed, 6 failed" not in result.stdout:
                raise RuntimeError(f"strict runner comparison failed\n{result.stdout}{result.stderr}")
            print("PASS V03 baseline comparison: current runner rejects the four false positives and two wrong-phase failures")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--original", action="store_true",
                        help="Also check the old local PowerShell runner, only before V03 replacement")
    parser.add_argument("--compare", action="store_true",
                        help="Compare the preserved predicate with the corrected Windows safety entry point")
    args = parser.parse_args()
    if args.baseline:
        baseline(args.original, args.compare)
        return 0
    if args.original or args.compare:
        parser.error("--original/--compare require --baseline")
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(RunnerRegressions))
    if result.wasSuccessful():
        print(f"PASS V03 runner regressions: {result.testsRun} checks")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
