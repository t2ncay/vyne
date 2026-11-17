"""Physical COM5 negative test: undersized TFLM must fail during initialization."""
import importlib.util
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bench", ROOT / "scripts/bench.py")
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


def main():
    compiler = Path(os.environ.get("VYNEC", bench.REPO / ("vynec.exe" if os.name == "nt" else "vynec"))).resolve()
    _, hardware = bench.configuration()
    directory = bench.BUILD / "sized/tflm"
    before = set(directory.glob("result-*.json"))
    directory = bench.build("tflm", "sized", compiler, tflm_arena=896)
    failures_before = set(directory.glob("failed-*.json"))
    try:
        bench.run_device(directory, hardware["connection"]["port"], hardware["connection"]["baud"], 30)
    except ValueError:
        pass
    else:
        raise AssertionError("undersized arena unexpectedly accepted")
    failures = set(directory.glob("failed-*.json")) - failures_before
    if len(failures) != 1 or set(directory.glob("result-*.json")) != before:
        raise AssertionError("negative run entered successful samples or failed to retain evidence")
    failure = json.loads(failures.pop().read_text())
    log = (directory / failure["raw_log"]).read_text(errors="replace")
    if "BENCH_ERROR implementation=tflm reason=allocate_tensors" not in log:
        raise AssertionError("failure was unrelated to arena capacity: " + log)
    print("ARENA_FAILURE_PASS: 896-byte TFLM arena rejected; raw evidence retained; no successful sample")
    print("Rebuild/run the ordinary sized profile to restore successful benchmark firmware.")


if __name__ == "__main__":
    main()
