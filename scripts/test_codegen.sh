#!/bin/bash
# ============================================================================
# Golden-output tests for the C codegen (issue #79, §8).
#
# For every tests/codegen/*.vyne file:
#   1. Transpile it with vynec --c (this also gcc-compiles and runs the
#      generated C, so a broken emitter fails here too).
#   2. Normalize the generated C (drop the machine-specific #include path,
#      trim trailing whitespace / blank lines).
#   3. Diff against the checked-in .expected.c, ignoring whitespace.
#
# Usage:  VYNEC=path/to/vynec scripts/test_codegen.sh
# ============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

VYNEC="${VYNEC:-$ROOT_DIR/vynec.exe}"
if [ ! -x "$VYNEC" ] && [ ! -f "$VYNEC" ]; then
    echo "error: compiler binary not found at $VYNEC (set VYNEC=path)" >&2
    exit 1
fi

CODE_DIR="$ROOT_DIR/tests/codegen"
PASS=0
FAIL=0

normalize() {
    # 1 - drop the machine-specific "#include \"<abs path>/vyne_runtime.h\"" line
    # 2 - trim trailing whitespace on every line
    # 3 - drop lines that are empty after trimming
    sed '/^#include ".*vyne_runtime.h"/d' "$1" \
        | sed 's/[[:space:]]*$//' \
        | sed '/^[[:space:]]*$/d'
}

# Compare two files (both already normalized). Runs inside the repo, so
# `git diff --no-index` is always available and is the primary comparator;
# other tools are used opportunistically.
files_equal() {
    local a="$1" b="$2"
    if git diff --no-index --quiet -- "$a" "$b" 2> /dev/null; then
        return 0
    fi
    if command -v cmp > /dev/null 2>&1 && cmp -s "$a" "$b"; then
        return 0
    fi
    if command -v diff > /dev/null 2>&1 && diff -q "$a" "$b" > /dev/null 2>&1; then
        return 0
    fi
    return 1
}

for src in "$CODE_DIR"/*.vyne; do
    name="$(basename "$src" .vyne)"
    expected="$CODE_DIR/$name.expected.c"
    if [ ! -f "$expected" ]; then
        echo "FAIL  $name — missing $expected"
        FAIL=$((FAIL + 1))
        continue
    fi

    # Transpile (writes $CODE_DIR/$name.vy.c and runs the C, like --c always
    # does). Clean up artifacts afterwards.
    if ! "$VYNEC" --c "$src" > /dev/null 2>&1; then
        echo "FAIL  $name — transpile/compile/run failed"
        FAIL=$((FAIL + 1))
        continue
    fi

    gen="$CODE_DIR/$name.vy.c"
    tmp_gen="$(mktemp)"
    tmp_exp="$(mktemp)"
    normalize "$gen" > "$tmp_gen"
    normalize "$expected" > "$tmp_exp"

    if files_equal "$tmp_exp" "$tmp_gen"; then
        echo "PASS  $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL  $name — generated C differs from golden"
        if command -v diff > /dev/null 2>&1; then
            diff -u "$tmp_exp" "$tmp_gen" || true
        elif command -v git > /dev/null 2>&1; then
            git diff --no-index -- "$tmp_exp" "$tmp_gen" || true
        fi
        FAIL=$((FAIL + 1))
    fi
    rm -f "$tmp_gen" "$tmp_exp"
    rm -f "$gen" "$CODE_DIR/$name.exe" "$CODE_DIR/$name"
done

echo ""
echo "Codegen golden tests: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]