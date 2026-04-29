@echo off
setlocal enabledelayedexpansion

:: --- Configuration ---
set CXX=g++
set CC=gcc
set TARGET=vynec.exe
set BUILD_DIR=build
set URAGE_LIB=urage.dll

:: --- Paths ---
set RAYLIB_INCLUDE=-I./vendor/raylib/include
set RAYLIB_LIB_PATH=-L./vendor/raylib/lib
set URAGE_INCLUDES=-I./vendor/urage/core/include -I./vendor/urage/core/src

:: --- Flags ---
set CXXFLAGS=-std=c++23 -O3 -I. %RAYLIB_INCLUDE% -I./lsp/backend/src -I./lsp/backend/include -DCPPHTTPLIB_OPENSSL_SUPPORT -D_WIN32 -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER
set URAGE_CFLAGS=-shared -DURAGE_BUILD_SHARED
set LDFLAGS=-mconsole -pthread %RAYLIB_LIB_PATH% -static -static-libgcc -static-libstdc++ -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lwinpthread

:: --- Sources ---
set URAGE_SRCS=./vendor/urage/core/src/database_api.c ./vendor/urage/core/src/database.c ./vendor/urage/core/src/btree.c ./vendor/urage/core/src/storage.c ./vendor/urage/core/src/pager.c ./vendor/urage/core/src/type.c

:: --- Build Logic ---
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo [1/3] Compiling %URAGE_LIB%...
%CC% %URAGE_CFLAGS% %URAGE_INCLUDES% %URAGE_SRCS% -o %URAGE_LIB%
if %errorlevel% neq 0 (
    echo Vyne Error: Failed to build %URAGE_LIB%
    exit /b %errorlevel%
)

echo [2/3] Gathering Source Files...
:: Makefile-dakı wildcard məntiqini simulyasiya edirik
set "SRCS="
for /r %%f in (*.cpp) do (
    set "file=%%f"
    :: vendor qovluğunu compile etməmək üçün (əgər Makefile-dakı kimi daxil deyilsə) yoxlama qoya bilərsən
    echo %%f | findstr /v "vendor" >nul
    if !errorlevel! equ 0 (
        set SRCS=!SRCS! "%%f"
    )
)

echo [3/3] Compiling %TARGET%...
%CXX% %CXXFLAGS% !SRCS! -o %TARGET% %LDFLAGS%

if %errorlevel% equ 0 (
    echo.
    echo Vyne Build Success: %TARGET% is ready.
) else (
    echo.
    echo Vyne Build Error: Compilation failed.
    exit /b %errorlevel%
)

pause