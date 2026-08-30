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
set RAYLIB_LIB=-lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lwinpthread
set OPENSSL_INC=-I"C:/msys64/ucrt64/include"
set OPENSSL_LIB_PATH=-L"C:/msys64/ucrt64/lib"
set OPENSSL_LIB=-lssl -lcrypto -lcrypt32 -lbcrypt -lws2_32
set URAGE_INCLUDES=-I./vendor/urage/core/include -I./vendor/urage/core/src

:: --- Flags ---
set CXXFLAGS=-std=c++23 -O3 -I. %RAYLIB_INCLUDE% -I./lsp/backend/src -I./lsp/backend/include %OPENSSL_INC% -DCPPHTTPLIB_OPENSSL_SUPPORT -D_WIN32 -DWIN32_LEAN_AND_MEAN -DNOGDI -DNOUSER -MMD -MP
set URAGE_CFLAGS=-shared -DURAGE_BUILD_SHARED
set LDFLAGS=-mconsole -pthread %RAYLIB_LIB_PATH% %RAYLIB_LIB% %OPENSSL_LIB_PATH% %OPENSSL_LIB%

:: --- URAGE Sources ---
set URAGE_SRCS=./vendor/urage/core/src/database_api.c ./vendor/urage/core/src/database.c ./vendor/urage/core/src/btree.c ./vendor/urage/core/src/storage.c ./vendor/urage/core/src/pager.c ./vendor/urage/core/src/type.c

:: --- Create build directory ---
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo ========================================
echo  Vyne Build System (Windows Batch)
echo ========================================
echo.

:: ---- Step 1: Build URAGE ----
echo [1/4] Building %URAGE_LIB%...
%CC% %URAGE_CFLAGS% %URAGE_INCLUDES% %URAGE_SRCS% -o %URAGE_LIB%
if %errorlevel% neq 0 (
    echo [ERROR] Failed to build %URAGE_LIB%
    exit /b %errorlevel%
)
echo [OK] %URAGE_LIB% built successfully
echo.

:: ---- Step 2: Collect all source files (excluding vendor) ----
echo [2/4] Collecting source files...
set "SRCS="

:: ONLY collect from cli, vyne, and editors folders
for /f "delims=" %%f in ('dir /s /b cli\*.cpp 2^>nul') do set "SRCS=!SRCS! "%%f""
for /f "delims=" %%f in ('dir /s /b vyne\*.cpp 2^>nul') do set "SRCS=!SRCS! "%%f""
for /f "delims=" %%f in ('dir /s /b editors\vscode\lsp\backend\src\*.cpp 2^>nul') do set "SRCS=!SRCS! "%%f""

echo [OK] Found source files
echo.

:: ---- Step 3: Compile each source file ----
echo [3/4] Compiling object files...
set "COMPILE_ERROR=0"

for %%s in (%SRCS%) do (
    echo   Compiling: %%s
    
    :: Create the directory structure inside build
    for %%d in ("%BUILD_DIR%\%%~ps") do (
        if not exist "%%~d" mkdir "%%~d" 2>nul
    )
    
    %CXX% %CXXFLAGS% -c %%s -o "%BUILD_DIR%\%%~ns.o"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to compile %%s
        set "COMPILE_ERROR=1"
    )
)

if %COMPILE_ERROR% equ 1 (
    echo [ERROR] Compilation failed
    exit /b 1
)
echo [OK] All objects compiled
echo.

:: ---- Step 4: Link ----
echo [4/4] Linking %TARGET%...
set "LINK_OBJS="
for /r %BUILD_DIR% %%f in (*.o) do (
    set "LINK_OBJS=!LINK_OBJS! "%%f""
)

%CXX% !LINK_OBJS! -o %TARGET% %LDFLAGS%
if %errorlevel% neq 0 (
    echo [ERROR] Linking failed
    exit /b %errorlevel%
)

echo.
echo ========================================
echo  [SUCCESS] %TARGET% built successfully!
echo ========================================
echo.
pause