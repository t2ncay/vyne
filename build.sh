#!/bin/bash
set -e

CXX=g++
CC=gcc
OUT="vyne_bin"
URAGE_OUT="liburage.so"

if [[ "$*" == *"--asan"* ]]; then
    echo "Building with AddressSanitizer..."
    EXTRA_FLAGS="-g -fsanitize=address -fsanitize=undefined"
else
    EXTRA_FLAGS="-O3"
fi

CXXFLAGS="-std=c++23 $EXTRA_FLAGS -Wall -Wextra -Wpedantic -DCPPHTTPLIB_OPENSSL_SUPPORT"
URAGE_CFLAGS="-shared -DURAGE_BUILD_SHARED -fPIC"

URAGE_SRC_FILES="third_party/urage/core/src/database_api.c \
third_party/urage/core/src/database.c \
third_party/urage/core/src/btree.c \
third_party/urage/core/src/storage.c \
third_party/urage/core/src/pager.c \
third_party/urage/core/src/type.c"

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
vyne/modules/common/vfs/vfs.cpp \
vyne/modules/common/vurage/vurage.cpp \
cli/file_handler.cpp \
editors/vscode/lsp/backend/src/lsp_server.cpp \
cli/repl.cpp"

echo "---------------------------------------"
echo "Building Vyne Interpreter (Unix-like)..."
echo "Mode: ${EXTRA_FLAGS}"
echo "---------------------------------------"

echo "Building bundled URAGE shared library..."
$CC $URAGE_CFLAGS -I./third_party/urage/core/include -I./third_party/urage/core/src \
    $URAGE_SRC_FILES -o $URAGE_OUT

$CXX $CXXFLAGS $SRC_FILES -o $OUT -lssl -lcrypto -ldl -pthread

echo "Build Successful: $OUT created."

if [ "$1" == "--test" ]; then
    TEST_FILE=${2:-bench}
    echo "Running tests/$TEST_FILE.vy with Bytecode..."
    ./$OUT --bytecode "tests/$TEST_FILE.vy"
fi
