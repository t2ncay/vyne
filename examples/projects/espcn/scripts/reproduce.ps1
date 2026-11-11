#!/usr/bin/env pwsh
# Reproduce the bundled ESPCN GPU-training and inference smoke numbers.
# Run from anywhere; paths are resolved from this file.

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$vynec = Join-Path (Resolve-Path "../../..") "vynec.exe"
if (!(Test-Path $vynec)) {
    throw "vynec.exe not found at $vynec"
}

if (Test-Path "build") {
    $removed = $false
    foreach ($attempt in 1..10) {
        try {
            Remove-Item -Recurse -Force "build" -ErrorAction Stop
            $removed = $true
            break
        } catch {
            if ($attempt -eq 10) {
                throw "could not remove build/. Close any program viewing files in build/ (for example Excel with train_loss.csv) and rerun scripts/reproduce.ps1"
            }
            Start-Sleep -Milliseconds 250
        }
    }
    if (!$removed) { throw "could not remove build directory" }
}
New-Item -ItemType Directory -Path "build" | Out-Null

Write-Host "== exporter smoke =="
python scripts/export_espcn_weights.py --random --scale 2 --out build/espcn_repro_passthrough.vwy

Write-Host "== train =="
& $vynec --compile src/train_gpu.vy
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./src/train_gpu.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== infer =="
& $vynec --compile src/main.vy
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./src/main.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$train = Import-Csv "build/train_loss.csv"
$val = Import-Csv "build/val_metrics.csv"
$firstTrain = [double]$train[0].loss
$lastTrain = [double]$train[$train.Count - 1].loss
$firstVal = [double]$val[0].val_mse
$lastVal = [double]$val[$val.Count - 1].val_mse

if (!($lastTrain -lt $firstTrain)) {
    throw "expected train loss to decrease, got $firstTrain -> $lastTrain"
}
if (!($lastVal -lt $firstVal)) {
    throw "expected validation MSE to decrease, got $firstVal -> $lastVal"
}

Write-Host "== summary =="
Write-Host "train loss: $firstTrain -> $lastTrain"
Write-Host "val mse   : $firstVal -> $lastVal"
Write-Host "checkpoint: build/espcn_gpu_best.vwt"
Write-Host "outputs   : build/output_espcn_cpu.png, build/output_espcn_gpu.png"
Write-Host "reproduce: PASS"
