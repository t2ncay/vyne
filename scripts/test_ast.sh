#!/usr/bin/env bash
# Interpreter cases require registered output contracts, not basename rewriting.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [ -z "${PYTHON:-}" ]; then
    if command -v python3 >/dev/null 2>&1; then PYTHON=python3; else PYTHON=python; fi
fi
if [ "$#" -gt 1 ]; then
    echo "error: test_ast.sh accepts at most one registered source path" >&2
    exit 2
fi
if [ "$#" -gt 0 ]; then
    exec "$PYTHON" "$ROOT_DIR/scripts/run_tests.py" --suite ast --source "$1"
fi
exec "$PYTHON" "$ROOT_DIR/scripts/run_tests.py" --suite ast
