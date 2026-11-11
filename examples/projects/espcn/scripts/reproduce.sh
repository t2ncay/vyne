#!/usr/bin/env sh
# Reproduce the bundled ESPCN GPU-training and inference smoke numbers.
# Run from anywhere; paths are resolved from this file.

set -eu
cd "$(dirname "$0")/.."

VYNEC=../../../vynec
if [ ! -x "$VYNEC" ]; then
    echo "vynec not found at $VYNEC" >&2
    exit 1
fi

rm -rf build
mkdir -p build

echo "== exporter smoke =="
python scripts/export_espcn_weights.py --random --scale 2 --out build/espcn_repro_passthrough.vwy

echo "== train =="
"$VYNEC" --compile src/train_gpu.vy
./src/train_gpu

echo "== infer =="
"$VYNEC" --compile src/main.vy
./src/main

first_train=$(awk -F, 'NR==2 {print $2}' build/train_loss.csv)
last_train=$(awk -F, 'END {print $2}' build/train_loss.csv)
first_val=$(awk -F, 'NR==2 {print $2}' build/val_metrics.csv)
last_val=$(awk -F, 'END {print $2}' build/val_metrics.csv)

awk -v a="$first_train" -v b="$last_train" 'BEGIN { if (!(b < a)) exit 1 }'
awk -v a="$first_val" -v b="$last_val" 'BEGIN { if (!(b < a)) exit 1 }'

echo "== summary =="
echo "train loss: $first_train -> $last_train"
echo "val mse   : $first_val -> $last_val"
echo "checkpoint: build/espcn_gpu_best.vwt"
echo "outputs   : build/output_espcn_cpu.png, build/output_espcn_gpu.png"
echo "reproduce: PASS"
