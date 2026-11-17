"""Prepare isolated export environment and verify pinned bundled Arduino TFLM."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python", default=sys.executable,
                        help="Python 3.13 executable used to create build/venv")
    parser.add_argument("--export-env", action="store_true")
    parser.add_argument("--tflm", action="store_true")
    args = parser.parse_args()
    if not args.export_env and not args.tflm:
        parser.error("select --export-env and/or --tflm")
    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    if args.export_env:
        env = build / "venv"
        subprocess.run([args.python, "-m", "venv", str(env)], check=True)
        python = env / ("Scripts/python.exe" if sys.platform == "win32" else "bin/python")
        subprocess.run([str(python), "-m", "pip", "install", "-r", str(ROOT / "requirements.lock")], check=True)
    if args.tflm:
        lock = json.loads((ROOT / "dependencies.json").read_text())
        cores = subprocess.check_output(["arduino-cli", "core", "list"], text=True)
        if not any(line.split()[:2] == ["esp32:esp32", lock["esp32_core"]]
                   for line in cores.splitlines()):
            raise RuntimeError(f"install arduino-cli core install esp32:esp32@{lock['esp32_core']}")
        print(lock["tflm_distribution"])


if __name__ == "__main__":
    main()
