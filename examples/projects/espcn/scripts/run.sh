#!/usr/bin/env sh
# Compile and run the ESPCN inference benchmark. Run from anywhere; paths are
# resolved from this file.

set -eu
cd "$(dirname "$0")/.."

VYNEC=../../../vynec
if [ ! -x "$VYNEC" ]; then
    echo "vynec not found at $VYNEC" >&2
    exit 1
fi

"$VYNEC" --compile src/main.vy
./src/main
