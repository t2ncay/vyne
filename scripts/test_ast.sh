#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# Move to root
cd "$(dirname "$0")/.." || exit

# Check for exe
if [ ! -f "vynec.exe" ]; then
    echo -e "${RED}ERROR:${NC} vynec.exe not found."
    exit 1
fi

# The Magic: Use a relative path for the test file too
# This removes the "Coding projects" part of the string entirely
REL_TEST_FILE="tests/$(basename "$1")"

echo "--------------------------------"
# Using 'eval' or direct execution with literal quotes
./vynec.exe --ast "$REL_TEST_FILE" 2>&1
EXIT_CODE=$?
echo "--------------------------------"

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}PASS:${NC} $1"
    exit 0
else
    echo -e "${RED}FAIL:${NC} $1 (Code $EXIT_CODE)"
    exit 1
fi