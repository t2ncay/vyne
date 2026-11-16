"""Baseline double: every invocation fails, including supposed positive cases."""
from pathlib import Path
import sys

name = next(Path(arg).name for arg in sys.argv[1:] if arg.endswith(".vy"))
if name in {"negative_index.vy", "too_large_index.vy"}:
    # Wrong phase deliberately: the old runner cannot distinguish this from a trap.
    print("Compile Error (VNE-072): controlled compiler-phase failure")
elif name == "wrong_index_count.vy":
    print("Compile Error (VNE-071): controlled rank mismatch")
elif name.startswith("escape_via_"):
    print("Compile Error (VNE-070): controlled escape rejection")
else:
    print("Syntax Error: controlled failure of a supposed passing fixture")
sys.exit(1)
