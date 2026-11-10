#!/usr/bin/env pwsh
# Build the synthetic dataset, train the residual SRCNN, then run the
# inference/benchmark. Run from anywhere; paths are resolved from this file.

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$vynec = Join-Path (Resolve-Path "../../..") "vynec.exe"
if (!(Test-Path $vynec)) {
    throw "vynec.exe not found at $vynec"
}

& $vynec --compile src/dataset.vy
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./src/dataset.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $vynec --compile src/train.vy
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./src/train.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $vynec --compile src/main.vy
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./src/main.exe
exit $LASTEXITCODE
