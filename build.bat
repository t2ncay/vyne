@echo off
setlocal

set CXX=g++
set CC=gcc

set CXXFLAGS=-std=c++23 -O3 -Wall -Wextra
set OUT=vyne.exe
set URAGE_OUT=urage.dll

set URAGE_SRC_FILES=third_party/urage/core/src/database_api.c ^
third_party/urage/core/src/database.c ^
third_party/urage/core/src/btree.c ^
third_party/urage/core/src/storage.c ^
third_party/urage/core/src/pager.c ^
third_party/urage/core/src/type.c

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
vyne/modules/common/vurage/vurage.cpp ^
cli/repl.cpp ^
editors/vscode/lsp/backend/src/lsp_server.cpp ^
cli/file_handler.cpp

echo ---------------------------------------
echo Building Vyne Interpreter (Windows) with GLFW...
echo ---------------------------------------

echo Building bundled URAGE shared library...
%CC% -shared -Ithird_party/urage/core/include -Ithird_party/urage/core/src %URAGE_SRC_FILES% -o %URAGE_OUT%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] URAGE build failed. Check the errors above.
    pause
    endlocal
    exit /b 1
)

%CXX% %CXXFLAGS% %SRC_FILES% -o %OUT%

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
