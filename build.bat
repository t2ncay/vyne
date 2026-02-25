@echo off
setlocal

set CXX=g++

set CXXFLAGS=-std=c++23 -O3 -Wall -Wextra -DCPPHTTPLIB_OPENSSL_SUPPORT
set OUT=vyne.exe

set SRC_FILES=main.cpp ^
vyne/vm/vm.cpp ^
vyne/compiler/codegen/chunk.cpp ^
vyne/compiler/codegen/codegen.cpp ^
vyne/compiler/lexer/lexer.cpp ^
vyne/compiler/parser/parser.cpp ^
vyne/compiler/ast/ast.cpp ^
vyne/compiler/ast/value.cpp ^
vyne/modules/common/vcore/vcore.cpp ^
vyne/modules/common/vglib/vglib.cpp ^
vyne/modules/common/vmem/vmem.cpp ^
vyne/modules/common/vmath/vmath.cpp ^
vyne/modules/common/vfs/vfs.cpp ^
cli/repl.cpp ^
editors/vscode/lsp/backend/src/lsp_server.cpp ^
cli/file_handler.cpp

echo ---------------------------------------
echo Building Vyne Interpreter (Windows) with GLFW...
echo ---------------------------------------

%CXX% %CXXFLAGS% %SRC_FILES% -o %OUT% -lssl -lcrypto -lws2_32 -pthread

if %ERRORLEVEL% EQU 0 (
    echo Build Successful: %OUT% created.
    
    if "%1"=="--test" (
        if not "%2"=="" (
            echo Running tests/%2.vy with Bytecode...
            .\%OUT% --bytecode tests/%2.vy
        ) else (
            echo Running default bench.vy...
            .\%OUT% --bytecode tests/bench.vy
        )
    )
) else (
    echo.
    echo [ERROR] Build failed. Check the errors above.
    pause
)

endlocal