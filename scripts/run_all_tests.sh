#!/bin/bash

if [ ! -f "./vynec.exe" ]; then
    echo "Error: vynec.exe not found. Build the project first."
    exit 1
fi

TEST_DIR="./tests"
PASS_COUNT=0
FAIL_COUNT=0

echo "Starting Vyne Test Suite..."
echo "============================"

for test_file in "$TEST_DIR"/*.vy; do
    ./scripts/test_ast.sh "$test_file"
    if [ $? -eq 0 ]; then
        ((PASS_COUNT++))
    else
        ((FAIL_COUNT++))
    fi
done

echo "============================"
echo -e "Results: ${GREEN}$PASS_COUNT Passed${NC}, ${RED}$FAIL_COUNT Failed${NC}"

if [ $FAIL_COUNT -gt 0 ]; then
    exit 1
fi