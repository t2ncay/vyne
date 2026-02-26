#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

SCRIPT_DIR="$(dirname "$0")"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ ! -f "$ROOT_DIR/vynec.exe" ]; then
    echo -e "${RED}Error: vynec.exe not found in $ROOT_DIR${NC}"
    exit 1
fi

TEST_DIR="$ROOT_DIR/tests"
PASS_COUNT=0
FAIL_COUNT=0

echo "Starting Vyne Test Suite..."
echo "============================"

for test_file in "$TEST_DIR"/*.vy; do
    bash "$SCRIPT_DIR/test_ast.sh" "$test_file"
    
    if [ $? -eq 0 ]; then
        ((PASS_COUNT++))
    else
        ((FAIL_COUNT++))
    fi
done

echo "============================"
echo -e "Final Results: ${GREEN}$PASS_COUNT Passed${NC}, ${RED}$FAIL_COUNT Failed${NC}"

[ $FAIL_COUNT -eq 0 ] || exit 1