#!/usr/bin/env sh
# Build the synthetic dataset, train the residual SRCNN, then run the
# inference/benchmark. Run from anywhere; paths are resolved from this file.

set -eu
cd "$(dirname "$0")/.."

VYNEC=../../../vynec
if [ ! -x "$VYNEC" ]; then
    echo "vynec not found at $VYNEC" >&2
    exit 1
fi

"$VYNEC" --compile src/dataset.vy
./src/dataset

"$VYNEC" --compile src/train.vy
./src/train

"$VYNEC" --compile src/main.vy
./src/main
