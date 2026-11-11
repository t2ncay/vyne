#!/usr/bin/env pwsh
# Compile and run the ESPCN inference benchmark. Run from anywhere; paths are
# resolved from this file.

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$vynec = Join-Path (Resolve-Path "../../..") "vynec.exe"
if (!(Test-Path $vynec)) {
    throw "vynec.exe not found at $vynec"
}

& $vynec --compile src/main.vy
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./src/main.exe
exit $LASTEXITCODE
