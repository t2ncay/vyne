#!/usr/bin/env bash
# Run registered core suites; --suite all explicitly includes domain acceptance.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [ -z "${PYTHON:-}" ]; then
    if command -v python3 >/dev/null 2>&1; then PYTHON=python3; else PYTHON=python; fi
fi
exec "$PYTHON" "$ROOT_DIR/scripts/run_tests.py" "$@"
