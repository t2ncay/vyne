#!/bin/bash
set -e

CXX=g++
OUT="vyne_bin"

if [[ "$*" == *"--asan"* ]]; then
    echo "Building with AddressSanitizer..."
    EXTRA_FLAGS="-g -fsanitize=address -fsanitize=undefined"
else
    EXTRA_FLAGS="-O3"
fi

CXXFLAGS="-std=c++23 $EXTRA_FLAGS -Wall -Wextra -Wpedantic"

SRC_FILES="main.cpp \
vyne/vm/vm.cpp \
vyne/compiler/codegen/chunk.cpp \
vyne/compiler/codegen/codegen.cpp \
vyne/compiler/lexer/lexer.cpp \
vyne/compiler/parser/parser.cpp \
vyne/compiler/ast/ast.cpp \
vyne/compiler/ast/value.cpp \
vyne/modules/common/vcore/vcore.cpp \
vyne/modules/common/vglib/vglib.cpp \
vyne/modules/common/vmem/vmem.cpp \
vyne/modules/common/vmath/vmath.cpp \
cli/file_handler.cpp \
cli/repl.cpp"

echo "---------------------------------------"
echo "Building Vyne Interpreter (Unix-like)..."
echo "Mode: ${EXTRA_FLAGS}"
echo "---------------------------------------"

$CXX $CXXFLAGS $SRC_FILES -o $OUT

echo "Build Successful: $OUT created."

if [ "$1" == "--test" ]; then
    TEST_FILE=${2:-bench}
    echo "Running tests/$TEST_FILE.vy with Bytecode..."
    ./$OUT --bytecode "tests/$TEST_FILE.vy"
fi