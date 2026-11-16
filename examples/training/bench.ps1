# Compatibility entry: measurements and exports are maintained in the root harness.
[CmdletBinding()]
param([string]$Exe = '.\ml_seq.exe')
& (Join-Path $PSScriptRoot '../../bench.ps1') -Exe $Exe -Runs 1 -NoWarmup
exit $LASTEXITCODE
