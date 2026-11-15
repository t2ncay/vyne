# Compatibility entry point; the portable runner asserts each execution phase.
$ErrorActionPreference = 'Stop'
$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Python = if ($env:PYTHON) { $env:PYTHON } else { 'python' }
& $Python (Join-Path $Repo 'scripts\run_tests.py') --suite safety @args
exit $LASTEXITCODE
