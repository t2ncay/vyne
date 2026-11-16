#!/usr/bin/env bash
# Compatibility entry point for registered golden compile-and-execute tests.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [ -z "${PYTHON:-}" ]; then
    if command -v python3 >/dev/null 2>&1; then PYTHON=python3; else PYTHON=python; fi
fi
exec "$PYTHON" "$ROOT_DIR/scripts/run_tests.py" --suite codegen "$@"
