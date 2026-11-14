"""Compile/run selected complete examples directly from maintained docs.

Run from any directory: python docs/scripts/test-examples.py [--graphics]
VYNEC or --compiler selects an existing compiler; no board/toolchain is required.
--graphics additionally compiles the managed window example (without opening it).
"""

import argparse
import os
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]


def block(source, title=None):
    text = (ROOT / source).read_text(encoding="utf-8")
    for match in re.finditer(r"^```vyne([^\n]*)\n(.*?)^```", text, re.M | re.S):
        if title is None or f'title="{title}"' in match[1]:
            return match[2]
    raise RuntimeError(f"Missing Vyne example {title or '(first)'} in {source}")


def run(command, cwd):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=180)
    if result.returncode:
        raise RuntimeError(f"{' '.join(map(str, command))}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("VYNEC"))
    parser.add_argument("--graphics", action="store_true")
    args = parser.parse_args()
    compiler = Path(args.compiler).resolve() if args.compiler else ROOT / ("vynec.exe" if os.name == "nt" else "vynec")
    if not compiler.is_file():
        parser.error(f"Compiler not found: {compiler}; set VYNEC or --compiler")
    output = ROOT / "build/docs-examples"
    output.mkdir(parents=True, exist_ok=True)
    cases = [
        ("slice", "docs/src/language/types.md", "slice.vy", ["50", "100", "30"]),
        ("buffer", "docs/src/language/builtins.md", "buffer.vy", ["3"]),
        ("device-helper", "docs/src/language/device.md", "device-helper.vy", None),
        ("vtensor", "vyne/modules/external/vtensor/README.md", None, None),
        ("vnn-loss", "vyne/modules/external/vnn/README.md", None, ["2.5"]),
        ("save-line", "docs/src/library/vplot.md", "save-line.vy", None),
    ]
    if args.graphics:
        cases.append(("managed-window", "vyne/modules/external/vraylib/MANAGED.md", None, None))
    for name, source, title, expected in cases:
        code = block(source, title)
        # Pin this documentation smoke test to portable CPU semantics. CUDA/GPU
        # kernels have their own acceptance suites; the snippet itself is unchanged.
        if name == "device-helper":
            code = code.replace("@device<cuda>", "@device<cpu>")
        vyne = output / f"{name}.vy"
        binary = output / (f"{name}.exe" if os.name == "nt" else name)
        vyne.write_text(code, encoding="utf-8")
        run([str(compiler), "--compile", "--quiet", "-o", str(binary), str(vyne)], output)
        if name == "managed-window":
            print(f"PASS {name}: compiled (interactive window not opened)")
            continue
        if name == "save-line":
            (output / "temperature.png").unlink(missing_ok=True)
        stdout = run([str(binary)], output)
        if expected is not None:
            values = [float(value) for value in stdout.strip().splitlines()]
            if values != [float(value) for value in expected]:
                raise RuntimeError(f"{name}: expected {expected}, got {stdout!r}")
        if name == "device-helper":
            values = [float(value) for value in re.findall(r"[-+]?\d+(?:\.\d+)?", stdout)]
            if values != [2.0, 4.0, 6.0]:
                raise RuntimeError(f"device-helper: unexpected values {stdout!r}")
        if name == "save-line":
            if not (output / "temperature.png").read_bytes().startswith(b"\x89PNG\r\n\x1a\n"):
                raise RuntimeError("save-line: PNG was not produced")
        print(f"PASS {name}: compiled and ran")
    print(f"Verified {len(cases)} documentation examples; outputs in {output}")


if __name__ == "__main__":
    main()
