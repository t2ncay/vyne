"""Compare two independent export processes, including bit-identical model data."""
import argparse
import hashlib
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=Path)
    parser.add_argument("second", type=Path)
    args = parser.parse_args()
    for name in ("model.vwt", "model.tflite", "model.h", "weights.h"):
        first = hashlib.sha256((args.first / name).read_bytes()).hexdigest()
        second = hashlib.sha256((args.second / name).read_bytes()).hexdigest()
        if first != second:
            raise AssertionError(f"export is not byte-reproducible: {name}")
        print(f"REEXPORT_PASS {name} {first}")


if __name__ == "__main__":
    main()
