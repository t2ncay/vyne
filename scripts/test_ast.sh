#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

if [ -z "$1" ]; then
    echo "Usage: ./scripts/test_ast.sh <path_to_vy_file>"
    exit 1
fi

echo -e "Testing: $1..."

OUTPUT=$(./vynec.exe --ast "$1" 2>&1)
EXIT_CODE=$?

echo "--------------------------------"
echo "$OUTPUT"
echo "--------------------------------"

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}PASS:${NC} $1 executed successfully."
else
    echo -e "${RED}FAIL:${NC} $1 exited with code $EXIT_CODE."
fi