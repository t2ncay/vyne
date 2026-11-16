"""V04 controlled baseline and exported-measurement acceptance (PowerShell 7)."""
import argparse
import ctypes
import csv
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import statistics
import subprocess
import tempfile
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def execute(command, *, env=None, timeout=90, expected=0):
    result = subprocess.run([str(x) for x in command], cwd=ROOT, env=env,
                            capture_output=True, text=True, encoding="utf-8",
                            errors="replace", timeout=timeout)
    if result.returncode != expected:
        raise AssertionError(f"{command}: exit {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--harness", type=Path, help="Explicit harness, e.g. a preserved original for baseline")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--compiler", default=os.environ.get("VYNEC", str(ROOT / ("vynec.exe" if os.name == "nt" else "vynec"))))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    pwsh = shutil.which("pwsh")
    if not pwsh:
        parser.error("PowerShell 7 (pwsh) is required for V04; no skipped success")
    artifacts = ROOT / "build" / "optimization"
    artifacts.mkdir(parents=True, exist_ok=True)
    evidence = {"baseline": args.baseline, "cases": []}
    checks = 0
    with tempfile.TemporaryDirectory(prefix="v04 benchmark ", dir=artifacts) as temp:
        work = Path(temp)
        if args.harness is None:
            if args.baseline:
                args.harness = work / "legacy_benchmark.ps1"
                shutil.copyfile(HERE / "fixtures/v04_legacy_benchmark.ps1.in", args.harness)
            else:
                args.harness = ROOT / "bench.ps1"
        binary = work / ("workload.exe" if os.name == "nt" else "workload")
        execute([*shlex.split(args.cxx, posix=os.name != "nt"), "-std=c++17", "-O2",
                 "-Wall", "-Wextra", "-pedantic", HERE / "v04_workload.cpp", "-o", binary])

        def benchmark(name, target=binary, *, duration=20, interval=50, memory=0,
                      burst=False, volume=False, runs=3, timeout=10, expected=0):
            nonlocal checks
            output = work / f"{name}.json"
            csv_path = work / f"{name}.csv"
            env = {**os.environ, "V04_DURATION_MS": str(duration), "V04_MEMORY_BYTES": str(memory)}
            for key, enabled in [("V04_BURST", burst), ("V04_OUTPUT_VOLUME", volume)]:
                env.pop(key, None)
                if enabled:
                    env[key] = "1"
            execute([pwsh, "-NoProfile", "-File", args.harness, "-Exe", target,
                     "-Runs", runs, "-NoWarmup", "-IntervalMs", interval,
                     "-Timeout", timeout, "-Json", output, "-Csv", csv_path],
                    env=env, expected=expected)
            report = json.loads(output.read_text(encoding="utf-8-sig"))
            with csv_path.open(encoding="utf-8-sig", newline="") as stream:
                rows = list(csv.DictReader(stream))
            evidence["cases"].append({"name": name, "duration_ms": duration,
                                      "interval_ms": interval, "report": report})
            if args.baseline:
                assert all(r["exit"] == 0 and r["ck"] == str((memory + 4095) // 4096 * 7)
                           for r in report["runs_detail"])
                print(f"BASELINE V04 {name}: {report['runs_detail']}")
                return report
            assert report["schema_version"] == 2, report
            assert len(rows) == runs == len(report["runs_detail"])
            wall = [raw["WallSec"] for raw in report["runs_detail"]]
            assert abs(report["stats"]["wall_mean"] - statistics.mean(wall)) < 1e-12
            assert report["stats"]["wall_median"] == statistics.median(wall)
            if runs > 1:
                assert abs(report["stats"]["wall_stddev"] - statistics.stdev(wall)) < 1e-12
            else:
                assert report["stats"]["wall_stddev"] is None
            for field, prefix in [("SampledWorkingSetMiB", "sampled_ws"),
                                  ("ProcessPeakWorkingSetMiB", "process_peak_ws"),
                                  ("CpuSec", "cpu")]:
                values = [raw[field] for raw in report["runs_detail"] if raw[field] is not None]
                assert report["stats"][prefix + "_count"] == len(values)
                mean = report["stats"][prefix + "_mean"]
                assert mean is None if not values else abs(mean - statistics.mean(values)) < 1e-12
            for row, raw in zip(rows, report["runs_detail"]):
                assert float(row["WallSec"]) == raw["WallSec"]
                assert raw["LaunchSec"] >= 0 and raw["ExitObservedSec"] > 0
                assert raw["WallSec"] > 0
                assert raw["SampledWorkingSetStatus"] in ("sampled", "missed", "unsupported")
                assert (raw["SampledWorkingSetBytes"] is None) == (raw["Samples"] == 0)
                assert raw["SampledWorkingSetMiB"] == (None if raw["SampledWorkingSetBytes"] is None
                                                       else raw["SampledWorkingSetBytes"] / 1048576)
                for field in ("CpuSec", "SampledWorkingSetBytes", "ProcessPeakWorkingSetBytes",
                              "ArenaMainLiveBytes", "ArenaMainPeakBytes"):
                    assert row[field] == "" if raw[field] is None else float(row[field]) == raw[field]
                if os.name == "nt":
                    assert raw["TimingSource"] == "windows-process-times"
                    assert raw["ProcessPeakStatus"] == "os-reported"
                    assert raw["ProcessPeakWorkingSetBytes"] > 0
                    assert raw["WallSec"] == raw["ProcessLifetimeSec"]
                else:
                    assert raw["TimingSource"] == "monotonic-exit-observed"
                    assert raw["ProcessPeakStatus"] == "unsupported"
                    assert raw["ProcessPeakWorkingSetBytes"] is None
                if target == binary and expected == 0:
                    independent = float(re.search(r"work_seconds: (\S+)", raw["Stdout"])[1])
                    # Broad scheduling tolerance; coarse 1s polling would fail short cases.
                    assert independent <= raw["WallSec"] + 0.05
                    assert raw["WallSec"] < independent + 0.35, raw
                    assert raw["Checksum"] == str((memory + 4095) // 4096 * 7)
                    assert raw["ArenaStatus"] == ("malformed" if os.environ.get("V04_MALFORMED_ARENA") else "not-reported")
                    assert raw["ArenaMainLiveBytes"] is None
            checks += 1
            print(f"PASS V04 {name}: wall={[r['WallSec'] for r in report['runs_detail']]}")
            return report

        benchmark("immediate", duration=0)
        benchmark("short_default", duration=20)
        short = benchmark("short_coarse", duration=20, interval=1000)
        benchmark("long_default", duration=1200, runs=2)
        benchmark("long_coarse", duration=1200, interval=1000, runs=2)
        sustained = benchmark("sustained_memory", duration=400, memory=32 * 1048576)
        burst = benchmark("burst_memory", duration=250, memory=32 * 1048576, burst=True, interval=1000)
        if args.baseline:
            # A cold .NET metadata query can itself outlive a short child, so
            # not every run necessarily enters the polling sleep. Reproduce it.
            assert any(r["wall"] > 0.8 for r in short["runs_detail"]), short
            print("BASELINE V04 reproduced: coarse polling dominates short workload; rounded/ambiguous memory exports")
        else:
            assert sustained["stats"]["sampled_ws_count"] > 0
            assert max(r["SampledWorkingSetBytes"] or 0 for r in sustained["runs_detail"]) > 24 * 1048576
            if os.name == "nt":
                assert min(r["ProcessPeakWorkingSetBytes"] for r in burst["runs_detail"]) > 24 * 1048576
            benchmark("output_volume", volume=True, runs=1)
            timed = benchmark("timeout", duration=5000, interval=5000, timeout=1, runs=1, expected=1)
            assert timed["runs_detail"][0]["TimedOut"]
            assert timed["runs_detail"][0]["ExitObservedSec"] < 2.5
            os.environ["V04_DESCENDANT"] = "1"
            try:
                tree = benchmark("timeout_tree", duration=5000, interval=5000, timeout=1, runs=1, expected=1)
                pid = int(re.search(r"descendant_pid: (\d+)", tree["runs_detail"][0]["Stdout"])[1])
                if os.name == "nt":
                    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
                    kernel.OpenProcess.restype = ctypes.c_void_p
                    handle = kernel.OpenProcess(0x1000, False, pid)
                    if handle:
                        status = ctypes.c_ulong()
                        kernel.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
                        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
                        try:
                            assert kernel.GetExitCodeProcess(handle, ctypes.byref(status))
                            assert status.value != 259, "descendant still running"
                        finally:
                            kernel.CloseHandle(handle)
                else:
                    proc = Path(f"/proc/{pid}/stat")
                    for _ in range(20):
                        if not proc.exists() or proc.read_text().split(")", 1)[1].split()[0] == "Z":
                            break
                        time.sleep(0.05)
                    else:
                        raise AssertionError("descendant still running")
            finally:
                os.environ.pop("V04_DESCENDANT", None)
            os.environ["V04_MALFORMED_ARENA"] = "1"
            try:
                malformed = benchmark("malformed_arena", runs=1)
                assert malformed["runs_detail"][0]["ArenaMainPeakBytes"] is None
            finally:
                os.environ.pop("V04_MALFORMED_ARENA", None)
            base = work / "arena"
            execute([args.compiler, "--compile", HERE / "v04_arena.vy", "-o", base])
            arena_binary = Path(str(base) + (".exe" if os.name == "nt" else ""))
            arena = benchmark("arena_snapshot", target=arena_binary, runs=1)
            raw = arena["runs_detail"][0]
            assert raw["ArenaStatus"] == "workload-reported"
            assert raw["ArenaMainLiveBytes"] == 0
            assert raw["ArenaMainPeakBytes"] >= 8192
            assert raw["ArenaMainCapacityBytes"] == 0
            assert raw["ArenaCommitLiveBytes"] == raw["ArenaCommitPeakBytes"] == 0
            assert raw["Checksum"] == "7"
            assert any(r["WallSec"] != round(r["WallSec"], 3) for r in short["runs_detail"])

            # Independently exercise no-sample statistics: absence must not become zero.
            stats_script = work / "statistics.ps1"
            stats_script.write_text('''param($Harness)
$tokens = $null; $errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($Harness, [ref]$tokens, [ref]$errors)
$function = $ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-Stats'}, $true)
Invoke-Expression $function.Extent.Text
@{ empty = (Get-Stats @()); one = (Get-Stats @(0.0000123456789)) } | ConvertTo-Json
''', encoding="utf-8")
            stats = json.loads(execute([pwsh, "-NoProfile", "-File", stats_script, args.harness]).stdout)
            assert stats["empty"]["N"] == 0
            assert all(stats["empty"][key] is None for key in ("Mean", "Min", "Max", "Std", "Median", "Mad"))
            assert stats["one"]["Mean"] == 0.0000123456789 and stats["one"]["Std"] is None
            checks += 1

            for name, baseline in [("compatible", work / "short_coarse.json"), ("old_schema", work / "old.json")]:
                if name == "old_schema":
                    baseline.write_text('{"stats":{"wall_median":1,"peak_median":99}}', encoding="utf-8")
                result = execute([pwsh, "-NoProfile", "-File", args.harness, "-Exe", binary,
                                  "-Runs", 1, "-NoWarmup", "-Baseline", baseline])
                if name == "compatible":
                    assert "=== vs Baseline ===" in result.stdout
                else:
                    assert "Incompatible baseline schema or timing source" in result.stdout
                    assert "=== vs Baseline ===" not in result.stdout
                checks += 1
            wrapper = execute([pwsh, "-NoProfile", "-File", ROOT / "examples/training/bench.ps1", "-Exe", binary])
            assert "sampled_ws=" in wrapper.stdout and "process_peak_ws=" in wrapper.stdout
            checks += 1
            source = work / "source.vy"
            shutil.copyfile(HERE / "v04_arena.vy", source)
            source_report = work / "source.json"
            execute([pwsh, "-NoProfile", "-File", args.harness, "-Source", source,
                     "-Compiler", args.compiler, "-Runs", 1, "-NoWarmup", "-Json", source_report])
            source_raw = json.loads(source_report.read_text(encoding="utf-8-sig"))
            assert source_raw["compile_sec"] > 0 and source_raw["discard"] == 0
            assert source_raw["runs_detail"][0]["Checksum"] == "7"
            checks += 1
            print(f"PASS V04: {checks} exported benchmark contracts")
    if args.report:
        args.report.write_text(json.dumps(evidence, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
