"""Independent adversarial contracts; standard library only unless export test selected."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[2]
spec = importlib.util.spec_from_file_location("bench", ROOT / "scripts/bench.py")
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


def valid_capture():
    lines = ["BENCH_META implementation=vyne model=" + "a" * 64 +
             " cpu_mhz=240 psram_bytes=0 arena_capacity=1024 arena_used=0 scratch_bytes=424 setup_us=0"
             " sketch_bytes=300000 heap_free=200000 heap_min=190000 heap_largest=100000"
             " stack_free_min_bytes=4096 stack_reserved_bytes=8192 checked=80 max_error=0.000001"]
    lines += [f"BENCH_SAMPLE implementation=vyne batch={i} iterations=100 elapsed_us={10000+i}"
              for i in range(20)]
    lines += ["BENCH_DONE implementation=vyne"]
    return "\n".join(lines) + "\n"


class CaptureContracts(unittest.TestCase):
    def parse(self, text):
        return bench.parse_capture(text, "vyne", "a" * 64, 1024)

    def test_valid_capture_precision(self):
        result = self.parse(valid_capture())
        self.assertEqual(result["samples"][-1]["per_inference_us"], 100.19)

    def test_wrong_model_and_runtime(self):
        for replacement in (("model=" + "a" * 64, "model=" + "b" * 64),
                            ("implementation=vyne", "implementation=tflm")):
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                self.parse(valid_capture().replace(*replacement))

    def test_missing_and_duplicate_batches(self):
        text = valid_capture()
        sample = "BENCH_SAMPLE implementation=vyne batch=0 iterations=100 elapsed_us=10000\n"
        for modified in (text.replace(sample, ""), text.replace("batch=1 ", "batch=0 "), text + sample):
            with self.subTest(text=modified), self.assertRaises(ValueError):
                self.parse(modified)

    def test_missing_done_validation_and_metadata(self):
        for modified in (valid_capture().replace("BENCH_DONE implementation=vyne", ""),
                         valid_capture().replace("checked=80", "checked=79"),
                         "\n".join(valid_capture().splitlines()[1:])):
            with self.subTest(text=modified), self.assertRaises(ValueError):
                self.parse(modified)

    def test_failed_firmware_never_counts(self):
        for error in ("BENCH_ERROR reason=invoke", "Guru Meditation", "abort()", "panic"):
            with self.subTest(error=error), self.assertRaises(ValueError):
                self.parse(valid_capture() + error)

    def test_invalid_measurements(self):
        replacements = (("elapsed_us=10000", "elapsed_us=0"), ("iterations=100", "iterations=99"),
                        ("arena_used=0", "arena_used=1025"), ("arena_capacity=1024", "arena_capacity=0"),
                        ("cpu_mhz=240", "cpu_mhz=160"), ("max_error=0.000001", "max_error=nan"),
                        ("heap_free=200000", "heap_free=-1"), ("sketch_bytes=300000", "sketch_bytes=0"),
                        ("psram_bytes=0", "psram_bytes=4194304"), ("max_error=0.000001", "max_error=1.0"),
                        ("stack_free_min_bytes=4096", "stack_free_min_bytes=8193"))
        for pair in replacements:
            with self.subTest(pair=pair), self.assertRaises(ValueError):
                self.parse(valid_capture().replace(*pair))

    def test_config_is_real_project_hardware_pair(self):
        project, hardware = bench.configuration()
        self.assertEqual(project["targets"]["hw950"]["board"], "esp32:esp32:esp32")
        self.assertEqual(hardware["board"]["preset"], "hw950")
        self.assertEqual(hardware["connection"]["port"], "COM5")


class EmissionContracts(unittest.TestCase):
    def setUp(self):
        self.compiler = Path(os.environ.get("VYNEC", REPO / ("vynec.exe" if os.name == "nt" else "vynec"))).resolve()
        if not self.compiler.exists():
            if "VYNEC" in os.environ:
                self.fail("explicit VYNEC does not exist")
            self.skipTest("build vynec first or set VYNEC")
        (ROOT / "build").mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=ROOT / "build")
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)

    def emit(self, source, board="esp32", target="--esp32", expected=0):
        file = self.directory / "fixture.vy"
        file.write_text(source, encoding="utf-8")
        base = self.directory / "fixture"
        result = subprocess.run([str(self.compiler), target, board, "--emit-c", "-o", str(base), str(file)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        self.assertNotIn("esp32-build", result.stdout + result.stderr)
        self.assertFalse((self.directory / "fixture_sketch").exists())
        return (self.directory / "fixture.vy.c").read_text() if expected == 0 else result.stdout + result.stderr

    def test_esp32_emit_uses_embedded_runtime(self):
        source = self.emit("use native esp32; esp32.serial_begin(115200);\n")
        self.assertIn('#include "vyne_esp32.h"', source)
        self.assertIn("void vyne_setup()", source)

    def test_avr_emit_uses_embedded_runtime(self):
        source = self.emit("use native arduino; arduino.serial_begin(9600);\n", "uno", "--arduino")
        self.assertIn("vyne_arduino.h", source)

    def test_oversized_static_scratch_rejected(self):
        text = self.emit("use native esp32; @bounded<8> region test { scratch x :: Float32[3]; };\n", expected=1)
        self.assertIn("VNE-101", text)

    def test_bridge_preserves_inference_and_rejects_abi_drift(self):
        result = subprocess.run([str(self.compiler), "--esp32", "esp32", "--emit-c", "-o",
                                 str(self.directory / "fixture"), str(ROOT / "src_vyne/bench.vy")],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        original = (self.directory / "fixture.vy.c").read_text()
        lowered = bench.bridge(original)
        self.assertEqual(original[original.index("void vyne_setup()") :],
                         lowered[lowered.index("void vyne_setup()") :])
        self.assertIn("float v_input[64]", original)
        self.assertIn("float v_hidden[32]", original)
        self.assertIn("float v_output[10]", original)
        self.assertIn("fn_bench_finish_native", original[original.index("void vyne_setup()") :])
        self.assertNotIn("fn_bench_finish(", original[original.index("void vyne_setup()") :])
        with self.assertRaises(ValueError):
            bench.bridge(original.replace("fn_bench_weight1_native", "different_abi"))
        with self.assertRaises(ValueError):
            bench.bridge(lowered)

    def test_mlp_inner_loop_keeps_float32_conversions_native(self):
        result = subprocess.run([str(self.compiler), "--esp32", "esp32", "--emit-c", "-o",
                                 str(self.directory / "fixture"), str(ROOT / "src_vyne/bench.vy")],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        source = (self.directory / "fixture.vy.c").read_text()
        body = source[source.index("void vyne_setup()") :]
        hot_loop = body.split("int64_t v_iteration", 1)[1].split("arduino_timer_read()", 1)[0]
        for unwanted in ("vyne_binop(", "vyne_to_float32(", "vyne_float32("):
            self.assertNotIn(unwanted, hot_loop)
        self.assertIn("float convert_", hot_loop)
        self.assertIn("fn_bench_weight1_native", hot_loop)

    def test_native_conversion_rounding_and_effects(self):
        fixture = ROOT / "tests/native_conversions.vy"
        for optimization in ("-O0", "-O3"):
            base = self.directory / ("conversions" + optimization[-1])
            result = subprocess.run([str(self.compiler), "--emit-c", "-o", str(base), str(fixture)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            binary = base.with_suffix(".exe" if os.name == "nt" else "")
            args = ["gcc", "-std=gnu11", optimization, "-ffp-contract=off", str(base.with_suffix(".vy.c")),
                    "-I", str(REPO), "-o", str(binary)]
            if os.name != "nt":
                args += ["-lm", "-lpthread"]
            compiled = subprocess.run(args, capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("NATIVE_CONVERSIONS_PASS", run.stdout)


class ReportContracts(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.build = self.root / "build"
        for folder in ("src_vyne", "src_tflm", "shared", "scripts"):
            (self.root / folder).mkdir(parents=True)
        for name in ("src_vyne/bench.vy", "src_vyne/data_bridge.vy", "src_tflm/esp32_memory_bench.ino",
                     "shared/float32_compat.h", "shared/report.h", "scripts/bench.py"):
            (self.root / name).write_text("stub\n")

    def fixture(self, runtime="vyne", compiler_hash="compiler-a"):
        directory = self.build / "sized" / runtime
        directory.mkdir(parents=True)
        text = valid_capture().replace("implementation=vyne", "implementation=" + runtime)
        log = directory / "capture.log"
        log.write_text(text)
        result = {"build": {"implementation": runtime, "profile": "sized", "arena_capacity": 1024,
                            "source_hashes": {"bench": "same"}, "compiler_sha256": compiler_hash,
                            "tflm_library_sha256": "library", "fqbn": "board",
                            "static_dram_data_bss_bytes": 12345,
                            "linked_arena_bytes": 0,
                            "model": {"hashes": {"model.tflite": "a" * 64}, "max_reference_abs": 1.0}},
                  "raw_log": str(log.relative_to(self.root)), "capture_sha256": bench.sha(log),
                  **bench.parse_capture(text, runtime, "a" * 64, 1024)}
        (directory / "result-fixture.json").write_text(json.dumps(result))
        return log

    def test_failed_reports_not_aggregated(self):
        self.fixture()
        (self.build / "sized/vyne/failed-fixture.json").write_text('{"error":"timeout"}')
        with patch.object(bench, "ROOT", self.root), patch.object(bench, "BUILD", self.build):
            bench.report()
        report = json.loads((self.build / "bench.json").read_text())
        self.assertEqual(len(report["results"]), 1)
        self.assertEqual(len(report["failed_runs"]), 1)
        self.assertEqual(report["rows"][0]["batches"], 20)

    def test_capture_tampering_rejected(self):
        log = self.fixture()
        log.write_text(log.read_text().replace("elapsed_us=10000", "elapsed_us=1"))
        with patch.object(bench, "ROOT", self.root), patch.object(bench, "BUILD", self.build), self.assertRaises(ValueError):
            bench.report()

    def test_mixed_compilers_rejected(self):
        self.fixture()
        self.fixture("tflm", "compiler-b")
        with patch.object(bench, "ROOT", self.root), patch.object(bench, "BUILD", self.build), self.assertRaises(ValueError):
            bench.report()


if __name__ == "__main__":
    unittest.main()
