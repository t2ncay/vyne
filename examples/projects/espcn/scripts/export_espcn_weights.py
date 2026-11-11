#!/usr/bin/env python3
"""Export an ESPCN checkpoint to a Vyne weight bundle.

Two output formats are supported:

  .vwy  binary (default) - little-endian, f32 payload. This is what
        `vconv.load_model` reads fastest.
  .vwt  text - human inspectable, useful for small fixtures.

With --checkpoint, PyTorch is required. The script accepts a state_dict,
a full nn.Module, or a checkpoint dict containing either under
"state_dict" / "model". Common key spellings are normalised.

A 3-conv ESPCN is assumed. The last conv must produce
in_channels * scale^2 channels. With --random (or no --checkpoint) the
script writes a deterministic passthrough network useful for smoke tests.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path
from typing import Iterable, List, Sequence, Tuple

ACT_CODES = {"relu": 0, "tanh": 1, "leaky_relu": 2}
Tensor = Tuple[str, Sequence[int], List[float]]


def _prod(dims: Sequence[int]) -> int:
    n = 1
    for d in dims:
        n *= int(d)
    return n


def _flatten(values) -> List[float]:
    if hasattr(values, "detach"):
        values = values.detach().cpu().float().numpy()
    if hasattr(values, "reshape"):
        values = values.reshape(-1)
    if hasattr(values, "tolist"):
        values = values.tolist()
    out: List[float] = []

    def walk(v):
        if isinstance(v, (list, tuple)):
            for item in v:
                walk(item)
        else:
            out.append(float(v))

    walk(values)
    return out


def _shape(values) -> List[int]:
    if hasattr(values, "shape"):
        return [int(x) for x in values.shape]
    if hasattr(values, "detach"):
        return [int(x) for x in values.detach().cpu().shape]
    shape = []
    cur = values
    while isinstance(cur, (list, tuple)):
        shape.append(len(cur))
        cur = cur[0] if len(cur) else None
    return shape


def write_binary(path: Path, tensors: Iterable[Tensor]) -> None:
    items = list(tensors)
    with path.open("wb") as f:
        f.write(b"VYW1")
        f.write(struct.pack("<I", len(items)))
        for name, dims, values in items:
            name_bytes = name.encode("utf-8")
            f.write(struct.pack("<I", len(name_bytes)))
            f.write(name_bytes)
            f.write(struct.pack("<I", len(dims)))
            for d in dims:
                f.write(struct.pack("<I", int(d)))
            for v in values:
                f.write(struct.pack("<f", float(v)))


def write_text(path: Path, tensors: Iterable[Tensor]) -> None:
    items = list(tensors)
    with path.open("w", encoding="utf-8") as f:
        f.write("VYWT1\n")
        f.write(f"{len(items)}\n")
        for name, dims, values in items:
            f.write(f"{name} {len(dims)}")
            for d in dims:
                f.write(f" {int(d)}")
            f.write("\n")
            for i, v in enumerate(values):
                if i:
                    f.write(" ")
                f.write(repr(float(v)))
            f.write("\n")


def passthrough_tensors(channels: int, scale: int) -> List[Tensor]:
    rr = scale * scale
    out_c = channels * rr

    def identity(cin: int) -> List[float]:
        w = []
        for oc in range(cin):
            for ic in range(cin):
                w.append(1.0 if oc == ic else 0.0)
        return w

    upsample = []
    for oc in range(out_c):
        for ic in range(channels):
            upsample.append(1.0 if (oc // rr) == ic else 0.0)

    return [
        ("conv1.weight", [channels, channels, 1, 1], identity(channels)),
        ("conv1.bias", [channels], [0.0] * channels),
        ("conv2.weight", [channels, channels, 1, 1], identity(channels)),
        ("conv2.bias", [channels], [0.0] * channels),
        ("conv3.weight", [out_c, channels, 1, 1], upsample),
        ("conv3.bias", [out_c], [0.0] * out_c),
        (
            "meta",
            [9],
            [float(scale), float(channels), float(channels), float(channels),
             float(out_c), 1.0, 1.0, 1.0, float(ACT_CODES["relu"])],
        ),
    ]


def _load_checkpoint(path: Path):
    try:
        import torch  # type: ignore
    except Exception as exc:
        raise SystemExit(
            "error: --checkpoint requires PyTorch (pip install torch): "
            f"{exc}"
        )
    obj = torch.load(str(path), map_location="cpu")
    if hasattr(obj, "state_dict"):
        obj = obj.state_dict()
    elif isinstance(obj, dict) and "state_dict" in obj:
        obj = obj["state_dict"]
    elif isinstance(obj, dict) and "model" in obj:
        inner = obj["model"]
        obj = inner.state_dict() if hasattr(inner, "state_dict") else inner
    if not isinstance(obj, dict):
        raise SystemExit("error: checkpoint is not a state_dict")
    return obj


def _strip_prefix(key: str) -> str:
    while key.startswith("module."):
        key = key[len("module."):]
    return key


def extract_convs(state: dict) -> Tuple[List[Tuple[str, object]], List[Tuple[str, object]]]:
    weights = []
    for key, val in state.items():
        k = _strip_prefix(key)
        if k.endswith(".weight") and len(_shape(val)) == 4:
            weights.append((k, val))
    if len(weights) != 3:
        raise SystemExit(
            f"error: expected 3 conv weights, found {len(weights)} "
            "(pass a plain ESPCN state_dict)"
        )

    biases = []
    for wkey, _ in weights:
        prefix = wkey[:-len("weight")]
        bkey = prefix + "bias"
        if bkey not in state:
            stripped = _strip_prefix(bkey)
            if stripped in state:
                bkey = stripped
            else:
                raise SystemExit(f"error: missing bias for {wkey}")
        biases.append((bkey, state[bkey]))
    return weights, biases


def tensors_from_state(state: dict, scale: int, activation: str) -> List[Tensor]:
    weights, biases = extract_convs(state)
    first_shape = _shape(weights[0][1])
    in_c = int(first_shape[1])
    c1 = int(first_shape[0])
    k1 = int(first_shape[2])
    c2 = int(_shape(weights[1][1])[0])
    k2 = int(_shape(weights[1][1])[2])
    last_shape = _shape(weights[2][1])
    c3 = int(last_shape[0])
    k3 = int(last_shape[2])

    if c3 % (in_c * scale * scale) != 0:
        raise SystemExit(
            f"error: last conv has {c3} output channels, expected a "
            f"multiple of in_c*scale^2 = {in_c * scale * scale}"
        )

    tensors: List[Tensor] = []
    for idx, (_, wval) in enumerate(weights):
        _, bval = biases[idx]
        tensors.append((f"conv{idx + 1}.weight", _shape(wval), _flatten(wval)))
        tensors.append((f"conv{idx + 1}.bias", _shape(bval), _flatten(bval)))
    tensors.append((
        "meta",
        [9],
        [float(scale), float(in_c), float(c1), float(c2), float(c3),
         float(k1), float(k2), float(k3), float(ACT_CODES[activation])],
    ))
    return tensors


def main(argv: Sequence[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--checkpoint", type=Path, default=None,
                    help="PyTorch .pth checkpoint")
    ap.add_argument("--out", type=Path, required=True,
                    help="output .vwy or .vwt path")
    ap.add_argument("--scale", type=int, default=2,
                    help="upscaling factor (default: 2)")
    ap.add_argument("--channels", type=int, default=3,
                    help="input channels for --random (default: 3)")
    ap.add_argument("--activation", choices=sorted(ACT_CODES), default="tanh",
                    help="activation used by the checkpoint")
    ap.add_argument("--random", action="store_true",
                    help="write the deterministic passthrough model")
    ap.add_argument("--text", action="store_true",
                    help="write text .vwt instead of binary .vwy")
    args = ap.parse_args(argv)

    if args.scale < 1:
        raise SystemExit("error: --scale must be >= 1")
    if args.channels < 1:
        raise SystemExit("error: --channels must be >= 1")

    if args.checkpoint is not None:
        tensors = tensors_from_state(
            _load_checkpoint(args.checkpoint), args.scale, args.activation
        )
    else:
        if not args.random:
            print(
                "note: no --checkpoint supplied; writing passthrough model",
                file=sys.stderr,
            )
        tensors = passthrough_tensors(args.channels, args.scale)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.text or args.out.suffix.lower() == ".vwt":
        write_text(args.out, tensors)
    else:
        write_binary(args.out, tensors)
    total = sum(_prod(dims) for _, dims, _ in tensors)
    print(f"wrote {args.out} ({len(tensors)} tensors, {total} values)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
