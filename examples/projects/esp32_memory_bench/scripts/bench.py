"""Stage, build, upload/capture and report correctness-gated ESP32 benchmarks."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import time
import tomllib

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[2]
BUILD = ROOT / "build"
LOOKUPS = {"weight1": ("bench_fc1_weight", 2048), "bias1": ("bench_fc1_bias", 32),
           "weight2": ("bench_fc2_weight", 320), "bias2": ("bench_fc2_bias", 10),
           "input": ("bench_inputs", 512), "expected": ("bench_expected", 80)}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def command(args, *, log=None, timeout=600):
    result = subprocess.run([str(x) for x in args], cwd=ROOT, capture_output=True,
                            text=True, encoding="utf-8", errors="replace", timeout=timeout)
    text = result.stdout + result.stderr
    if log:
        Path(log).write_text(text, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {args}\n{text}")
    return text


def configuration():
    project = tomllib.loads((ROOT / "project.toml").read_text())
    hardware = tomllib.loads((ROOT / "hardware.toml").read_text())
    target = project["targets"][project["targets"]["default"]]
    bench = project["bench"]
    if target["backend"] != "esp32" or target["board"] != "esp32:esp32:esp32":
        raise ValueError("this benchmark is pinned to the classic ESP32")
    if hardware["board"]["preset"] != project["targets"]["default"]:
        raise ValueError("project/hardware preset mismatch")
    if hardware["connection"]["baud"] != 115200:
        raise ValueError("firmware requires 115200 baud")
    for key, expected in {"warmup": 10, "iterations": 100, "batches": 20,
                          "cpu_mhz": 240, "absolute_tolerance": 2e-5,
                          "relative_tolerance": 2e-4}.items():
        if bench[key] != expected:
            raise ValueError(f"{key} must match firmware protocol ({expected})")
    return project, hardware


def verify_data():
    manifest = json.loads((ROOT / "data/manifest.json").read_text())
    if manifest["shape"] != [64, 32, 10] or manifest["dtype"] != "float32" or manifest["test_vectors"] != 8:
        raise ValueError("model contract mismatch")
    for filename, digest in manifest["hashes"].items():
        if sha(ROOT / "data" / filename) != digest:
            raise ValueError(f"artifact checksum mismatch: {filename}")
    if set(manifest["hashes"]) != {"model.vwt", "model.tflite", "model.h", "weights.h"}:
        raise ValueError("incomplete artifact hashes")
    return manifest


def replace_function(source, name, native_body):
    # Exact anchored definitions, not call-site substitution. Reject compiler ABI drift.
    for suffix, body in (("_native", native_body), ("", 'bench_fail("boxed_bridge");')):
        pattern = re.compile(r"^((?:float|int64_t|VyneValue) fn_bench_" + re.escape(name + suffix) +
                             r"\([^\n]*\) \{)\n.*?^\}", re.M | re.S)
        matches = list(pattern.finditer(source))
        if len(matches) != 1 or "exit(" not in matches[0].group():
            raise ValueError(f"unexpected generated ABI/stub for {name}{suffix}")
        source = pattern.sub(lambda m: m.group(1) + "\n    " + body + "\n}", source)
    return source


def bridge(source):
    if source.count('#include "vyne_esp32.h"') != 1 or "void vyne_setup()" not in source:
        raise ValueError("not ESP32 generated code; use --esp32 esp32 --emit-c in that order")
    # Keep the generated execution body byte-for-byte. Only ABI stubs change.
    original_body = source[source.index("void vyne_setup()") :]
    for name, (array, size) in LOOKUPS.items():
        index = f"v_bench_{name}_native_index"
        signature = rf"float fn_bench_{name}_native\(int64_t {index}\)"
        if not re.search(signature, source):
            raise ValueError("Float32 data accessor signature changed")
        source = replace_function(source, name,
            f'if ({index} < 0 || {index} >= {size}) bench_fail("data_index"); return {array}[{index}];')
    source = replace_function(source, "begin",
        'Serial.begin(115200); delay(1500); if (getCpuFrequencyMhz() != 240) bench_fail("cpu_frequency"); '
        'asm volatile("" : : "r"(g_mcu_arena) : "memory"); '
        'bench_arena_reserved = sizeof(g_mcu_arena); bench_scratch_bytes = 424; return 0;')
    source = replace_function(source, "end",
        'if (v_bench_end_native_batch < 0 || v_bench_end_native_batch >= 20) bench_fail("batch_index"); '
        'bench_elapsed[v_bench_end_native_batch] = v_bench_end_native_elapsed; return 0;')
    source = replace_function(source, "validate",
        'bench_check((int)v_bench_validate_native_index, v_bench_validate_native_value); return 0;')
    source = replace_function(source, "finish",
        'bench_arena_used = v_bench_finish_native_peak; bench_report(); return 0;')
    source = replace_function(source, "observe",
        'bench_sink = v_bench_observe_native_value; return 0;')
    if source[source.index("void vyne_setup()") :] != original_body:
        raise ValueError("bridge modified generated inference body")
    return source.replace('#include "vyne_esp32.h"', '#include "vyne_esp32.h"\n'
                          '#include "float32_compat.h"\n#define BENCH_IMPLEMENTATION "vyne"\n#include "report.h"')


def stage(implementation, directory, compiler):
    verify_data()
    sketch = directory / "sketch"
    sketch.mkdir(parents=True, exist_ok=True)
    for file in (ROOT / "shared").glob("*.h"):
        shutil.copy2(file, sketch / file.name)
    for name in ("weights.h", "model.h"):
        shutil.copy2(ROOT / "data" / name, sketch / name)
    if implementation == "tflm":
        shutil.copy2(ROOT / "src_tflm/esp32_memory_bench.ino", sketch / "sketch.ino")
    else:
        base = directory / "emitted"
        text = command([compiler, "--esp32", "esp32", "--emit-c", "-o", base,
                        ROOT / "src_vyne/bench.vy"], log=directory / "emit.log")
        emitted = base.with_suffix(".vy.c")
        if not emitted.exists() or "esp32-build" in text:
            raise RuntimeError("embedded emit-only compiler support required; rebuild this branch")
        source = emitted.read_text()
        lowered = bridge(source)
        (sketch / "sketch.ino").write_text(lowered, encoding="utf-8")
        shutil.copy2(REPO / "vyne/runtime/esp32/vyne_esp32.h", sketch / "vyne_esp32.h")
        shutil.copytree(REPO / "vyne/runtime/esp32/detail", sketch / "detail", dirs_exist_ok=True)
        (directory / "bridge-audit.json").write_text(json.dumps({
            "emitted_sha256": sha(emitted), "staged_sha256": sha(sketch / "sketch.ino"),
            "inference_body_unchanged": True, "replaced_stubs": list(LOOKUPS) +
            ["begin", "end", "validate", "finish", "observe"]}, indent=2) + "\n")
    return sketch


def build(implementation, profile, compiler, tflm_arena=None):
    project, _ = configuration()
    lock = json.loads((ROOT / "dependencies.json").read_text())
    cores = command(["arduino-cli", "core", "list"])
    if not any(line.split()[:2] == ["esp32:esp32", lock["esp32_core"]] for line in cores.splitlines()):
        raise ValueError("ESP32 core version differs from dependency pin")
    cli_version = command(["arduino-cli", "version"])
    if f"Version: {lock['arduino_cli']} " not in cli_version:
        raise ValueError("Arduino CLI version differs from dependency pin")
    directory = BUILD / profile / implementation
    directory.mkdir(parents=True, exist_ok=True)
    sketch = stage(implementation, directory, compiler)
    arena = (65536 if profile == "default" else project["bench"]["vyne_arena_bytes"])
    tensor_arena = tflm_arena if tflm_arena is not None else project["bench"][
        "tflm_arena_bytes" if profile == "default" else "tflm_sized_arena_bytes"]
    if tensor_arena < 16 or tensor_arena % 16 or arena < 8:
        raise ValueError("arena capacities must be valid/aligned")
    flags = f"-DVYNE_MCU_ARENA_SIZE={arena} -DBENCH_TFLM_ARENA_BYTES={tensor_arena} -ffp-contract=off"
    fqbn = project["targets"]["hw950"]["board"] + ":CPUFreq=240,PSRAM=disabled"
    args = ["arduino-cli", "compile", "--fqbn", fqbn, "--build-path", directory / "cache",
            "--output-dir", directory / "firmware", "--build-property", "compiler.cpp.extra_flags=" + flags,
            "--build-property", "compiler.c.extra_flags=-ffp-contract=off", sketch]
    text = command(args, log=directory / "compile.log", timeout=900)
    props = command(["arduino-cli", "compile", "--fqbn", fqbn,
                     "--show-properties=expanded", sketch])
    (directory / "properties.txt").write_text(props, encoding="utf-8")
    properties = dict(line.split("=", 1) for line in props.splitlines() if "=" in line)
    tool_path = Path(properties["compiler.path"])
    size_tool = tool_path / properties.get("compiler.size.cmd", "xtensa-esp32-elf-size")
    if os.name == "nt" and not size_tool.exists():
        size_tool = size_tool.with_suffix(".exe")
    elf = directory / "firmware/sketch.ino.elf"
    sections_text = command([size_tool, "-A", elf], log=directory / "sections.txt")
    sections = {m.group(1): int(m.group(2)) for m in
                re.finditer(r"^(\.[\w.]+)\s+(\d+)\s+", sections_text, re.M)}
    static_dram = sum(n for section, n in sections.items()
                      if re.fullmatch(r"\.dram\d+\.(?:data|bss)|\.noinit", section))
    libpath = Path(properties["compiler.sdk.path"]) / "lib/libespressif__esp-tflite-micro.a"
    if sha(libpath) != lock["tflm_library_sha256"]:
        raise ValueError("bundled TFLM binary differs from the pinned classic ESP32 distribution")
    source_hashes = {str(p.relative_to(ROOT)): sha(p) for folder in ("src_vyne", "src_tflm", "shared", "scripts")
                     for p in (ROOT / folder).glob("*") if p.is_file()}
    source_hashes.update({name: sha(ROOT / name) for name in
                         ("project.toml", "hardware.toml", "dependencies.json", "requirements.txt", "requirements.lock")})
    record = {"schema": 1, "implementation": implementation, "profile": profile,
              "created_utc": datetime.now(timezone.utc).isoformat(), "fqbn": fqbn,
              "flags": flags, "arena_capacity": arena if implementation == "vyne" else tensor_arena,
              "model": verify_data(), "dependency_lock": lock, "source_hashes": source_hashes,
              "git_revision": command(["git", "rev-parse", "HEAD"]).strip(),
              "git_dirty": bool(command(["git", "status", "--porcelain"]).strip()),
              "compiler_sha256": sha(compiler), "compiler_version": command([compiler, "--version"]).strip(),
              "arduino_cli": cli_version.strip(), "tflm_library_sha256": sha(libpath),
              "elf_sha256": sha(elf), "sections": sections, "static_dram_data_bss_bytes": static_dram,
              "compile_command": [str(x) for x in args], "compile_summary": text[-1500:]}
    (directory / "build.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(f"BUILD_PASS {profile}/{implementation}: static DRAM data/BSS {static_dram} bytes")
    return directory


def fields(line):
    pairs = line.split()[1:]
    result = {}
    for pair in pairs:
        key, value = pair.split("=", 1)
        if key in result:
            raise ValueError("duplicate field")
        result[key] = value
    return result


def parse_capture(text, implementation, model_hash, capacity, max_error_limit=0.001):
    if re.search(r"BENCH_ERROR|Guru Meditation|panic|abort\(", text, re.I):
        raise ValueError("firmware failure in capture")
    metadata = [fields(line) for line in text.splitlines() if line.startswith("BENCH_META ")]
    samples = [fields(line) for line in text.splitlines() if line.startswith("BENCH_SAMPLE ")]
    done = [fields(line) for line in text.splitlines() if line.startswith("BENCH_DONE ")]
    if len(metadata) != 1 or len(samples) != 20 or len(done) != 1:
        raise ValueError("missing/duplicate metadata, samples or completion")
    meta = metadata[0]
    if any(row.get("implementation") != implementation for row in metadata + samples + done):
        raise ValueError("wrong implementation")
    if meta["model"] != model_hash or int(meta["checked"]) != 80 or int(meta["cpu_mhz"]) != 240:
        raise ValueError("model/correctness/CPU contract")
    integer_fields = ("arena_reserved", "arena_used", "scratch_bytes", "sketch_bytes", "psram_bytes",
                      "heap_free", "heap_min", "heap_largest", "stack_free_min_bytes", "stack_reserved_bytes", "checked", "cpu_mhz")
    numbers = {key: int(meta[key]) for key in integer_fields}
    if any(value < 0 for value in numbers.values()) or numbers["arena_reserved"] != capacity:
        raise ValueError("invalid memory measurement/capacity")
    if numbers["arena_used"] > capacity or numbers["sketch_bytes"] == 0:
        raise ValueError("invalid memory/flash measurement")
    if numbers["psram_bytes"] != 0 or numbers["stack_free_min_bytes"] > numbers["stack_reserved_bytes"]:
        raise ValueError("PSRAM/stack contract")
    setup = int(meta["setup_us"])
    if setup < -1:
        raise ValueError("invalid setup measurement")
    numbers["setup_us"] = None if setup == -1 else setup
    error = float(meta["max_error"])
    if not math.isfinite(error) or error < 0 or error > max_error_limit:
        raise ValueError("invalid numerical error")
    batches = set()
    elapsed = []
    for row in samples:
        batch, iterations, duration = int(row["batch"]), int(row["iterations"]), int(row["elapsed_us"])
        if batch in batches or not 0 <= batch < 20 or iterations != 100 or duration <= 0:
            raise ValueError("invalid batch/iterations/timing")
        batches.add(batch)
        elapsed.append({"batch": batch, "elapsed_us": duration, "iterations": iterations,
                        "per_inference_us": duration / iterations})
    return {"metadata": {**numbers, "implementation": implementation, "model": model_hash,
                         "max_error": error}, "samples": sorted(elapsed, key=lambda row: row["batch"])}


def run_device(directory, port, baud, seconds):
    import serial
    record = json.loads((directory / "build.json").read_text())
    firmware = directory / "firmware"
    if sha(firmware / "sketch.ino.elf") != record["elf_sha256"]:
        raise ValueError("firmware changed since build")
    command(["arduino-cli", "upload", "--fqbn", record["fqbn"], "--port", port,
             "--input-dir", firmware], log=directory / "upload.log")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%f")
    raw = bytearray()
    log = directory / f"capture-{stamp}.log"
    try:
        with serial.Serial(port, baud, timeout=0.1) as uart:
            uart.dtr = False
            uart.rts = True
            time.sleep(0.1)
            uart.reset_input_buffer()
            uart.rts = False
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                raw.extend(uart.read(4096))
                if b"BENCH_DONE " in raw or b"BENCH_ERROR " in raw:
                    # Read the rest of the line before parsing completion.
                    raw.extend(uart.read(4096))
                    break
        log.write_bytes(raw)
        parsed = parse_capture(raw.decode(errors="replace"), record["implementation"],
                               record["model"]["hashes"]["model.tflite"], record["arena_capacity"],
                               2e-5 + 2e-4 * record["model"]["max_reference_abs"])
    except Exception as error:
        log.write_bytes(raw)
        (directory / f"failed-{stamp}.json").write_text(json.dumps({"error": str(error),
            "raw_log": log.name, "build": record}, indent=2) + "\n")
        raise
    result = {"schema": 1, "build": record, "port": port, "baud": baud,
              "raw_log": str(log.relative_to(ROOT)), "capture_sha256": sha(log), **parsed}
    (directory / f"result-{stamp}.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"DEVICE_PASS {record['profile']}/{record['implementation']} 80 outputs, 20 batches")


def loc(path):
    prefixes = ("#",) if Path(path).suffix in (".vy", ".py") else ("//",)
    return sum(bool(line.strip()) and not line.lstrip().startswith(prefixes)
               for line in Path(path).read_text().splitlines())


def report(update_readme=False):
    results = []
    for file in sorted(BUILD.glob("*/*/result-*.json")):
        result = json.loads(file.read_text())
        capture = ROOT / result["raw_log"]
        if sha(capture) != result["capture_sha256"]:
            raise ValueError("capture checksum changed")
        checked = parse_capture(capture.read_text(errors="replace"), result["build"]["implementation"],
                                result["build"]["model"]["hashes"]["model.tflite"],
                                result["build"]["arena_capacity"],
                                2e-5 + 2e-4 * result["build"]["model"]["max_reference_abs"])
        if checked["metadata"] != result["metadata"] or checked["samples"] != result["samples"]:
            raise ValueError("result/capture mismatch")
        results.append(result)
    if not results:
        raise ValueError("no validated device results")
    identities = {(r["build"]["model"]["hashes"]["model.tflite"],
                   r["build"]["compiler_sha256"], r["build"]["tflm_library_sha256"],
                   r["build"]["fqbn"], json.dumps(r["build"]["source_hashes"], sort_keys=True)) for r in results}
    if len(identities) != 1:
        raise ValueError("mixed model/source revisions; archive previous build results first")
    rows = []
    for profile in ("default", "sized"):
        for impl in ("tflm", "vyne"):
            group = [r for r in results if r["build"]["profile"] == profile and r["build"]["implementation"] == impl]
            if not group:
                continue
            capacities = {r["metadata"]["arena_reserved"] for r in group}
            if len(capacities) != 1:
                raise ValueError("mixed arena capacities in one comparison profile")
            values = [s["per_inference_us"] for r in group for s in r["samples"]]
            metadata = group[-1]["metadata"]
            rows.append({"profile": profile, "implementation": impl, "runs": len(group),
                         "batches": len(values), "median_us": statistics.median(values),
                         "min_us": min(values), "max_us": max(values),
                         "metadata": metadata, "static_dram_data_bss_bytes": group[-1]["build"]["static_dram_data_bss_bytes"]})
    summary = {"schema": 1, "rows": rows, "results": results,
               "failed_runs": [str(p.relative_to(ROOT)) for p in BUILD.glob("*/*/failed-*.json")],
               "loc": {"vyne_firmware": loc(ROOT / "src_vyne/bench.vy"),
                       "vyne_stubs": loc(ROOT / "src_vyne/data_bridge.vy"),
                       "vyne_compat": loc(ROOT / "shared/float32_compat.h"),
                       "tflm_firmware": loc(ROOT / "src_tflm/esp32_memory_bench.ino"),
                       "staging_runner": loc(ROOT / "scripts/bench.py"),
                       "common_reporting": loc(ROOT / "shared/report.h")}}
    (BUILD / "bench.json").write_text(json.dumps(summary, indent=2) + "\n")
    lines = ["## Measured device results", "", "Raw times are batch durations / 100, not independently timed inference calls.", "",
             "| Profile | Runtime | Runs/batches | Reserved arena B | Arena used/peak B | Scratch B | Static DRAM data/BSS B | Median ms/inference (min–max) | Flash B |",
             "| --- | --- | --- | ---: | ---: | ---: | ---: | --- | ---: |"]
    for row in rows:
        m = row["metadata"]
        lines.append(f"| {row['profile']} | {row['implementation']} | {row['runs']}/{row['batches']} | "
                     f"{m['arena_reserved']} | {m['arena_used']} | {m['scratch_bytes']} | "
                     f"{row['static_dram_data_bss_bytes']} | {row['median_us']/1000:.6f} "
                     f"({row['min_us']/1000:.6f}–{row['max_us']/1000:.6f}) | {m['sketch_bytes']} |")
    lines += ["", "All accepted runs checked 80 outputs at atol=2e-5, rtol=2e-4. "
              "Reserved capacity, arena high-water and scratch bytes overlap with total SRAM and must not be blindly summed.", "",
              "Source LOC (nonblank, whole-line language comments excluded; braces and C/C++ preprocessor lines counted): " +
              json.dumps(summary["loc"], sort_keys=True), "",
              f"Failed captures excluded: {len(summary['failed_runs'])}.", "",
              "### Capture provenance", ""]
    for r in results:
        m = r["metadata"]
        lines.append(f"- `{r['raw_log']}`: model `{m['model']}`, capture SHA256 `{r['capture_sha256']}`, "
                     f"max absolute error `{m['max_error']:.9g}`, stack free minimum `{m['stack_free_min_bytes']}` B, "
                     f"internal heap free/minimum/largest `{m['heap_free']}/{m['heap_min']}/{m['heap_largest']}` B, "
                     f"task stack reserved `{m['stack_reserved_bytes']}` B, "
                     f"setup `{m['setup_us'] if m['setup_us'] is not None else 'not measured'}` "
                     f"{'' if m['setup_us'] is None else 'us'}. Build metadata and ELF sections are retained alongside the capture.")
    markdown = "\n".join(lines) + "\n"
    (BUILD / "results.md").write_text(markdown, encoding="utf-8")
    if update_readme:
        readme = ROOT / "README.md"
        text = readme.read_text(encoding="utf-8")
        begin, end = "<!-- BENCH_RESULTS_BEGIN -->", "<!-- BENCH_RESULTS_END -->"
        if text.count(begin) != 1 or text.count(end) != 1:
            raise ValueError("README result markers missing/duplicate")
        text = text[:text.index(begin) + len(begin)] + "\n\n" + markdown + "\n" + text[text.index(end):]
        readme.write_text(text, encoding="utf-8")
    print(markdown)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("stage", "build", "run", "report", "archive"))
    parser.add_argument("--implementation", choices=("both", "vyne", "tflm"), default="both")
    parser.add_argument("--profile", choices=("default", "sized"), default="default")
    parser.add_argument("--compiler", type=Path, default=Path(os.environ.get("VYNEC", REPO / ("vynec.exe" if os.name == "nt" else "vynec"))))
    parser.add_argument("--port")
    parser.add_argument("--seconds", type=float, default=120)
    parser.add_argument("--tflm-arena", type=int)
    parser.add_argument("--update-readme", action="store_true")
    args = parser.parse_args()
    project, hardware = configuration()
    if args.action == "archive":
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%f")
        destination = BUILD / "archive" / stamp
        destination.mkdir(parents=True)
        for profile in ("default", "sized"):
            if (BUILD / profile).exists():
                shutil.move(str(BUILD / profile), str(destination / profile))
        shutil.copytree(ROOT / "data", destination / "data")
        print(f"Archived prior builds/results to {destination}")
        return
    if args.action == "report":
        report(args.update_readme)
        return
    implementations = ("tflm", "vyne") if args.implementation == "both" else (args.implementation,)
    for impl in implementations:
        directory = BUILD / args.profile / impl
        if args.action == "stage":
            directory.mkdir(parents=True, exist_ok=True)
            stage(impl, directory, args.compiler.resolve())
        else:
            directory = build(impl, args.profile, args.compiler.resolve(), args.tflm_arena)
            if args.action == "run":
                run_device(directory, args.port or hardware["connection"]["port"], hardware["connection"]["baud"], args.seconds)


if __name__ == "__main__":
    main()
