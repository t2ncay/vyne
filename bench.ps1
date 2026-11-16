<#
.SYNOPSIS
    Vyne benchmark harness.
.DESCRIPTION
    Runs the target executable N times, sampling working set at a fixed
    interval. Collects process/observed elapsed time, CPU time, exit code,
    OS-reported process peaks where supported, optional vmem.dump() snapshots,
    and any checksum printed to stdout. Reports per-run rows plus
    aggregate statistics (mean, min, max, median, MAD, sample stddev),
    derived metrics (parallelism factor, GFLOP/s when a dgemm config
    line is detected), an environment fingerprint, and optional CSV /
    JSON export.

    Two ways to use:

      (a) benchmark an already-built exe:
              .\bench.ps1 .\examples\benchmark\matmul_1024_blas.exe

      (b) build from source then benchmark:
              .\bench.ps1 -Source .\examples\training\ml_seq.vy -Blas -Native

    The -Source path invokes `vynec --compile` on the source file first;
    -Blas, -Native, and -NoScratchBounds are forwarded to the compiler.
    The resulting exe's name is derived from the source's stem, and any
    -Exe parameter is ignored.

.PARAMETER Exe
    Path to a pre-built executable. Ignored when -Source is given.
    Default: .\matmul_1024.exe

.PARAMETER Source
    Path to a .vy source. When present, the harness compiles it with
    the new `--compile` CLI mode before benchmarking.

.PARAMETER Compiler
    Path to vynec.exe. If unset, searches PATH, then the directory
    containing this script, then its parent.

.PARAMETER Blas
    Forward --blas to the compiler. Only meaningful with -Source.

.PARAMETER Native
    Forward --native to the compiler. Only meaningful with -Source.

.PARAMETER NoScratchBounds
    Forward --no-scratch-bounds to the compiler. Only meaningful with -Source.

.PARAMETER Runs
    Number of timed runs. Default: 5. More runs give a tighter median
    and a meaningful MAD; three is too few.

.PARAMETER IntervalMs
    Working-set sampling interval in milliseconds. Default: 50.
    Sampling is a lower bound, not an OS-reported peak. Waits wake at exit.

.PARAMETER Timeout
    Per-run timeout in seconds. Default: 600.

.PARAMETER Discard
    Warmup runs to discard. Default: 1.

.PARAMETER NoWarmup
    Skip warmup entirely. Use only for deterministic, cheap-start targets.

.PARAMETER ExpectedChecksum
    If provided, any run whose checksum differs is treated as failure.

.PARAMETER Csv
    Optional path. Writes per-run rows as CSV.

.PARAMETER Json
    Optional path. Writes the aggregate result (statistics + environment)
    as JSON schema version 2, retaining raw per-run values and provenance.
    Version 1 polling/ambiguous-peak baselines are not comparable.

.PARAMETER Baseline
    Optional path to a previous -Json result. When present, the summary
    adds a "vs baseline" delta block.

.PARAMETER Label
    Row label. Defaults to exe basename.

.EXAMPLE
    # Benchmark a pre-built exe with 5 runs
    .\bench.ps1 .\examples\benchmark\matmul_1024_blas.exe -Runs 5

.EXAMPLE
    # Compile from source, then benchmark
    .\bench.ps1 -Source .\examples\training\ml_seq.vy -Blas -Native

.EXAMPLE
    # Save a baseline, then compare the next run against it
    .\bench.ps1 .\ml_seq.exe -Json .\ml_seq.json
    .\bench.ps1 .\ml_seq.exe -Baseline .\ml_seq.json
#>

[CmdletBinding()]
param(
    [Parameter(Position=0)]
    [string]$Exe,

    [string]$Source,
    [string]$Compiler,

    [switch]$Blas,
    [switch]$Native,
    [switch]$NoScratchBounds,

    [ValidateRange(1, 100000)][int]$Runs = 5,
    [ValidateRange(1, 2147483647)][int]$IntervalMs = 50,
    [ValidateRange(1, 2147483)][int]$Timeout = 600,
    [ValidateRange(0, 100000)][int]$Discard = 1,
    [switch]$NoWarmup,

    [string]$ExpectedChecksum,
    [string]$Csv,
    [string]$Json,
    [string]$Baseline,
    [string]$Label
)

$ErrorActionPreference = 'Stop'

if ($PSVersionTable.PSVersion.Major -lt 7) { throw 'Benchmark harness requires PowerShell 7+ (pwsh).' }
if (-not ('VyneBenchmarkProcess' -as [type])) {
    Add-Type -Path (Join-Path $PSScriptRoot 'scripts/benchmark_process.cs')
}
$compileSec = $null
$effectiveDiscard = if ($NoWarmup) { 0 } else { $Discard }

# ----------------------------------------------------------------------------
# Locate the compiler (only needed for -Source)
# ----------------------------------------------------------------------------
function Resolve-Compiler {
    param([string]$Explicit)
    if ($Explicit) {
        $r = Resolve-Path -LiteralPath $Explicit -ErrorAction SilentlyContinue
        if (-not $r) { throw "compiler not found: $Explicit" }
        return $r.Path
    }
    $compilerName = if ($IsWindows) { 'vynec.exe' } else { 'vynec' }
    $fromPath = Get-Command $compilerName -ErrorAction SilentlyContinue
    if ($fromPath) { return $fromPath.Source }

    foreach ($cand in @(
        (Join-Path $PSScriptRoot $compilerName),
        (Join-Path (Split-Path $PSScriptRoot -Parent) $compilerName),
        (Join-Path $PSScriptRoot "build/$compilerName")
    )) {
        if (Test-Path -LiteralPath $cand) { return (Resolve-Path -LiteralPath $cand).Path }
    }
    throw "$compilerName not found; pass -Compiler <path> or add it to PATH"
}

# ----------------------------------------------------------------------------
# Build from source, if requested
# ----------------------------------------------------------------------------
if ($Source) {
    $srcResolved = Resolve-Path -LiteralPath $Source -ErrorAction SilentlyContinue
    if (-not $srcResolved) { Write-Host "source not found: $Source" -ForegroundColor Red; exit 1 }
    $srcPath = $srcResolved.Path

    $compilerExe = Resolve-Compiler -Explicit $Compiler

    $stem = [System.IO.Path]::GetFileNameWithoutExtension($srcPath)
    $suffix = if ($IsWindows) { '.exe' } else { '' }
    $derivedExe = Join-Path ([System.IO.Path]::GetDirectoryName($srcPath)) ($stem + $suffix)
    $Exe = $derivedExe

    $compileArgs = @("--compile", $srcPath)
    if ($Blas)            { $compileArgs += "--blas" }
    if ($Native)          { $compileArgs += "--native" }
    if ($NoScratchBounds) { $compileArgs += "--no-scratch-bounds" }

    Write-Host ""
    Write-Host "=== Compile ===" -ForegroundColor Cyan
    Write-Host ("  compiler  : {0}" -f $compilerExe)
    Write-Host ("  source    : {0}" -f $srcPath)
    Write-Host ("  flags     : {0}" -f (($compileArgs | Select-Object -Skip 1) -join ' '))
    Write-Host ""

    $compileWatch = [System.Diagnostics.Stopwatch]::StartNew()
    & $compilerExe @compileArgs
    $compileWatch.Stop()
    $compileSec = $compileWatch.Elapsed.TotalSeconds
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  compile failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
}

# ----------------------------------------------------------------------------
# Default exe
# ----------------------------------------------------------------------------
if (-not $Exe) { $Exe = ".\matmul_1024.exe" }

$exeResolved = Resolve-Path -LiteralPath $Exe -ErrorAction SilentlyContinue
if (-not $exeResolved) {
    Write-Host "not found: $Exe" -ForegroundColor Red
    exit 1
}
$exePath = $exeResolved.Path
if (-not $Label) {
    $Label = [System.IO.Path]::GetFileNameWithoutExtension($exePath)
}

# ----------------------------------------------------------------------------
# ANSI stripper
# ----------------------------------------------------------------------------
$ansiPattern = [regex]'\x1b\[[0-9;]*[A-Za-z]'
function Strip-Ansi([string]$s) {
    if ([string]::IsNullOrEmpty($s)) { return '' }
    return $ansiPattern.Replace($s, '')
}

# ----------------------------------------------------------------------------
# Single timed run
# ----------------------------------------------------------------------------
function Invoke-TimedRun {
    param([string]$Path, [int]$IntervalMs, [int]$TimeoutSec)

    $measurement = [VyneBenchmarkProcess]::Run($Path, $IntervalMs, $TimeoutSec)
    $cleanOut = Strip-Ansi $measurement.Stdout
    $cleanErr = Strip-Ansi $measurement.Stderr

    # Checksum — accepts both `checksum: 3.47777` and `checksum = 3.47777`.
    $checksum = $null
    if ($cleanOut -match 'checksum[:=]\s*(\S+)') { $checksum = $Matches[1] }

    # Config line — tolerant of `config=N ... N=1024 iters=100` and the
    # bracketed forms some benchmarks print.
    $configLine = $null
    if ($cleanOut -match '(config[^\r\n]{3,80})') { $configLine = $Matches[1].Trim() }
    elseif ($cleanOut -match '(\bN=\d+\b[^\r\n]{0,40}\biters?=\d+\b)') { $configLine = $Matches[1] }

    # Optional allocation-free vmem.dump() snapshot. Keep the last complete dump;
    # its peak counters retain their runtime/reset scope, not an inferred run peak.
    $arenaPattern = 'vmem dump\r?\n\s+main:\s+live=(\d+)\s+peak=(\d+)\s+blocks=(\d+)\s+cap=(\d+)\r?\n\s+head used=\d+ / \d+\r?\n\s+commit:\s+live=(\d+)\s+peak=(\d+)\r?\n\s+ckpts:\s+depth=\d+ / \d+'
    $dumps = [regex]::Matches($cleanErr, $arenaPattern)
    $arena = if ($dumps.Count) { $dumps[$dumps.Count - 1] } else { $null }
    $arenaStatus = if ($arena) { 'workload-reported' } elseif ($cleanErr.Contains('vmem dump')) { 'malformed' } else { 'not-reported' }

    [pscustomobject]@{
        WallSec      = $measurement.WallSec
        LaunchSec    = $measurement.LaunchSec
        ExitObservedSec = $measurement.ExitObservedSec
        ProcessLifetimeSec = $measurement.ProcessLifetimeSec
        TimingSource = $measurement.TimingSource
        CpuSec       = $measurement.CpuSec
        CpuStatus    = $measurement.CpuStatus
        SampledWorkingSetBytes = $measurement.SampledWorkingSetBytes
        SampledWorkingSetMiB = if ($null -ne $measurement.SampledWorkingSetBytes) { $measurement.SampledWorkingSetBytes / 1MB } else { $null }
        SampledWorkingSetStatus = $measurement.SampledWorkingSetStatus
        ProcessPeakWorkingSetBytes = $measurement.ProcessPeakWorkingSetBytes
        ProcessPeakWorkingSetMiB = if ($null -ne $measurement.ProcessPeakWorkingSetBytes) { $measurement.ProcessPeakWorkingSetBytes / 1MB } else { $null }
        ProcessPeakPagefileBytes = $measurement.ProcessPeakPagefileBytes
        ProcessPeakStatus = $measurement.ProcessPeakStatus
        ArenaStatus = $arenaStatus
        ArenaSnapshots = $dumps.Count
        ArenaMainLiveBytes = if ($arena) { [long]$arena.Groups[1].Value } else { $null }
        ArenaMainPeakBytes = if ($arena) { [long]$arena.Groups[2].Value } else { $null }
        ArenaMainBlocks = if ($arena) { [long]$arena.Groups[3].Value } else { $null }
        ArenaMainCapacityBytes = if ($arena) { [long]$arena.Groups[4].Value } else { $null }
        ArenaCommitLiveBytes = if ($arena) { [long]$arena.Groups[5].Value } else { $null }
        ArenaCommitPeakBytes = if ($arena) { [long]$arena.Groups[6].Value } else { $null }
        ExitCode     = $measurement.ExitCode
        Samples      = $measurement.Samples
        TimedOut     = $measurement.TimedOut
        Checksum     = $checksum
        ConfigLine   = $configLine
        Stdout       = $cleanOut
        Stderr       = $cleanErr
    }
}

# ----------------------------------------------------------------------------
# Statistics: mean, min, max, sample stddev, median, MAD
# ----------------------------------------------------------------------------
function Get-Stats([double[]]$vals) {
    $empty = [pscustomobject]@{
        Mean=$null; Min=$null; Max=$null; Std=$null; Median=$null; Mad=$null; N=0
    }
    if ($null -eq $vals -or $vals.Count -eq 0) { return $empty }

    $sorted = @($vals | Sort-Object)
    $n = $sorted.Count

    $mean = ($vals | Measure-Object -Average).Average
    $min  = $sorted[0]
    $max  = $sorted[$n - 1]

    $median =
        if ($n % 2 -eq 1) { $sorted[[int][math]::Floor($n / 2)] }
        else              { ($sorted[$n / 2 - 1] + $sorted[$n / 2]) / 2.0 }

    $deviations = @($vals | ForEach-Object { [math]::Abs($_ - $median) } | Sort-Object)
    $mad =
        if ($n % 2 -eq 1) { $deviations[[int][math]::Floor($n / 2)] }
        else              { ($deviations[$n / 2 - 1] + $deviations[$n / 2]) / 2.0 }

    $std = 0.0
    if ($n -gt 1) {
        $ss = 0.0
        foreach ($v in $vals) { $ss += [math]::Pow($v - $mean, 2) }
        $std = [math]::Sqrt($ss / ($n - 1))
    }

    [pscustomobject]@{
        Mean   = $mean
        Min    = $min
        Max    = $max
        Std    = if ($n -gt 1) { $std } else { $null }
        Median = $median
        Mad    = $mad
        N      = $n
    }
}

# ----------------------------------------------------------------------------
# Environment fingerprint
# ----------------------------------------------------------------------------
function Get-EnvFingerprint {
    $gcc =
        try { @(gcc --version 2>$null)[0] } catch { $null }
    $cpu =
        try { @(Get-CimInstance Win32_Processor -ErrorAction Stop)[0].Name } catch { $null }
    $os  = [System.Environment]::OSVersion.VersionString
    $psv = $PSVersionTable.PSVersion.ToString()
    $obt = if ($env:OPENBLAS_NUM_THREADS) { $env:OPENBLAS_NUM_THREADS } else { $null }

    [pscustomobject]@{
        GCC              = $gcc
        CPU              = $cpu
        OS               = $os
        PowerShell       = $psv
        DotNet           = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
        Architecture     = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        OpenBLASThreads  = $obt
    }
}

# ----------------------------------------------------------------------------
# Header
# ----------------------------------------------------------------------------
$exeItem = Get-Item -LiteralPath $exePath
$exeMB   = [math]::Round($exeItem.Length / 1MB, 2)
$envInfo = Get-EnvFingerprint

Write-Host ""
Write-Host "=== Vyne Benchmark ===" -ForegroundColor Cyan
Write-Host ("  label            : {0}" -f $Label)
Write-Host ("  exe              : {0}  ({1} MB)" -f $exePath, $exeMB)
Write-Host ("  runs             : {0}  (discard {1} warmup)" -f $Runs, $effectiveDiscard)
Write-Host ("  sampling         : {0} ms" -f $IntervalMs)
Write-Host ("  timeout          : {0} s/run" -f $Timeout)
Write-Host '  timing           : Windows OS process lifetime; other hosts monotonic launch-to-exit observation'
Write-Host '  memory           : sampled WS lower bound / OS process peak / optional last vmem.dump snapshot'
Write-Host '  units            : seconds, bytes, MiB (1,048,576 bytes); missing metrics are not zero'
if ($null -ne $compileSec) { Write-Host ("  compile          : {0:N6} s (separate from timed runs)" -f $compileSec) }

if ($envInfo.OpenBLASThreads) {
    Write-Host ("  OPENBLAS_THREADS : {0}" -f $envInfo.OpenBLASThreads)
} else {
    Write-Host "  OPENBLAS_THREADS : (unset — OpenBLAS default)" -ForegroundColor Yellow
}
if ($ExpectedChecksum) {
    Write-Host ("  expected checksum: {0}" -f $ExpectedChecksum)
}
Write-Host ""

# ----------------------------------------------------------------------------
# Warmup — fail fast if it errors
# ----------------------------------------------------------------------------
if (-not $NoWarmup) {
    for ($i = 0; $i -lt $Discard; $i++) {
        Write-Host ("  warmup {0}/{1} ..." -f ($i + 1), $Discard) -ForegroundColor DarkGray
        $w = Invoke-TimedRun -Path $exePath -IntervalMs $IntervalMs -TimeoutSec $Timeout
        if ($w.TimedOut) {
            Write-Host "  warmup TIMEOUT — aborting" -ForegroundColor Red
            exit 1
        }
        if ($w.ExitCode -ne 0) {
            Write-Host ("  warmup exited {0} — aborting" -f $w.ExitCode) -ForegroundColor Red
            Write-Host ""
            Write-Host "  --- captured stderr ---" -ForegroundColor DarkGray
            Write-Host $w.Stderr
            exit 1
        }
    }
    if ($Discard -gt 0) { Write-Host "" }
}

# ----------------------------------------------------------------------------
# Timed runs
# ----------------------------------------------------------------------------
$results = @()
function Format-Metric($Value, [string]$Format = 'N6') {
    if ($null -eq $Value) { return 'n/a' }
    return ([double]$Value).ToString($Format)
}
for ($i = 0; $i -lt $Runs; $i++) {
    $r = Invoke-TimedRun -Path $exePath -IntervalMs $IntervalMs -TimeoutSec $Timeout
    $results += $r

    $status =
        if     ($r.TimedOut)            { "TIMEOUT" }
        elseif ($r.ExitCode -ne 0)      { "exit " + $r.ExitCode }
        else                            { "ok" }
    $color =
        if     ($r.TimedOut -or $r.ExitCode -ne 0) { "Red" }
        else                                       { "Green" }

    Write-Host ("  run {0}/{1}  {2,-8}  wall={3}s  sampled_ws={4} MiB  process_peak_ws={5} MiB  cpu={6}s  samples={7}" -f `
        ($i + 1), $Runs, $status, (Format-Metric $r.WallSec), (Format-Metric $r.SampledWorkingSetMiB 'N2'), `
        (Format-Metric $r.ProcessPeakWorkingSetMiB 'N2'), (Format-Metric $r.CpuSec), $r.Samples) -ForegroundColor $color
    Write-Host ("    source={0}; sampled_ws={1}; process_peak={2}; arena={3}" -f `
        $r.TimingSource, $r.SampledWorkingSetStatus, $r.ProcessPeakStatus, $r.ArenaStatus)
}

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
$wallStats = Get-Stats @($results | ForEach-Object { $_.WallSec })
$rssStats  = Get-Stats @($results | Where-Object { $null -ne $_.SampledWorkingSetMiB } | ForEach-Object { $_.SampledWorkingSetMiB })
$processPeakStats = Get-Stats @($results | Where-Object { $null -ne $_.ProcessPeakWorkingSetMiB } | ForEach-Object { $_.ProcessPeakWorkingSetMiB })
$cpuStats  = Get-Stats @($results | Where-Object { $null -ne $_.CpuSec } | ForEach-Object { $_.CpuSec })
$timingSources = @($results | ForEach-Object { $_.TimingSource } | Select-Object -Unique)

$checksums       = @($results | ForEach-Object { $_.Checksum } | Where-Object { $_ } | Select-Object -Unique)
$allChecksumSame = ($checksums.Count -le 1)

$checksumDisplay =
    if     ($checksums.Count -eq 0) { "(none)" }
    elseif ($checksums.Count -eq 1) { $checksums[0] }
    else                            { ($checksums -join ", ") + "  *MISMATCH*" }

$configLine = ($results | Where-Object { $_.ConfigLine } | Select-Object -First 1).ConfigLine
$anyTimeout = @($results | Where-Object { $_.TimedOut }).Count -gt 0
$anyNonZero = @($results | Where-Object { $_.ExitCode -ne 0 }).Count -gt 0

Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan

if ($configLine) { Write-Host ("  config      : {0}" -f $configLine); Write-Host "" }

Write-Host ("  {0,-24} {1,10} {2,10} {3,10} {4,10} {5,10} {6,10} {7,6}" -f "metric", "mean", "min", "max", "median", "mad", "stddev", "count")
foreach ($entry in @(
    @{ Name='wall (s)'; Stats=$wallStats },
    @{ Name='sampled WS max (MiB)'; Stats=$rssStats },
    @{ Name='OS peak WS (MiB)'; Stats=$processPeakStats },
    @{ Name='cpu (s)'; Stats=$cpuStats }
)) {
    $s = $entry.Stats
    Write-Host ("  {0,-24} {1,10} {2,10} {3,10} {4,10} {5,10} {6,10} {7,6}" -f `
        $entry.Name, (Format-Metric $s.Mean), (Format-Metric $s.Min), (Format-Metric $s.Max), `
        (Format-Metric $s.Median), (Format-Metric $s.Mad), (Format-Metric $s.Std), $s.N)
}
Write-Host ""

if ($allChecksumSame) {
    Write-Host ("  checksum    : {0}   (identical across {1} runs)" -f $checksumDisplay, $Runs) -ForegroundColor Green
} else {
    Write-Host ("  checksum    : {0}" -f $checksumDisplay) -ForegroundColor Red
}

if ($wallStats.Mean -gt 0 -and $cpuStats.Mean -gt 0) {
    $parallelism = $cpuStats.Mean / $wallStats.Mean
    Write-Host ("  parallelism : {0,6:N2}x   (cpu-time / wall-time)" -f $parallelism)
}

# GFLOP/s for dgemm-style benchmarks. Uses the MEDIAN wall time, not the
# mean: on a noisy machine the mean is skewed by stragglers and the median
# is the number the paper should quote.
$gflops = $null
if ($configLine -match 'N=(\d+)\s+iters?=(\d+)') {
    [int]$n  = [int]$Matches[1]
    [int]$it = [int]$Matches[2]
    $totalFlops = 2.0 * [math]::Pow($n, 3) * $it
    if ($wallStats.Median -gt 0) {
        $gflops = ($totalFlops / $wallStats.Median) / 1e9
        Write-Host ("  throughput  : {0,6:N1} GFLOP/s   (2·N³·iters / wall_median)" -f $gflops)
    }
}

# ----------------------------------------------------------------------------
# Baseline comparison
# ----------------------------------------------------------------------------
if ($Baseline) {
    if (-not (Test-Path -LiteralPath $Baseline)) {
        Write-Host ("  baseline    : {0} not found — skipping comparison" -f $Baseline) -ForegroundColor Yellow
    } else {
        try {
            $bl = Get-Content -LiteralPath $Baseline -Raw | ConvertFrom-Json
            if ($bl.schema_version -ne 2 -or @($bl.timing_sources).Count -ne $timingSources.Count -or `
                (Compare-Object @($bl.timing_sources) $timingSources)) {
                throw 'Incompatible baseline schema or timing source; collect a schema-v2 baseline with the same timing source.'
            }
            Write-Host ""
            Write-Host "=== vs Baseline ===" -ForegroundColor Cyan
            Write-Host ("  source      : {0}" -f (Resolve-Path -LiteralPath $Baseline).Path)

            function Delta([double]$cur, [double]$base, [string]$unit) {
                if ($base -le 0) { return "(baseline zero)" }
                $d = ($cur - $base) / $base * 100.0
                $arrow = if ($d -lt -0.05) { "faster" } elseif ($d -gt 0.05) { "slower" } else { "same" }
                # For wall/peak: lower is faster/less. For throughput: higher is better.
                return ("{0:N1}% {1}   ({2:N3} vs {3:N3} {4})" -f [math]::Abs($d), $arrow, $cur, $base, $unit)
            }

            if ($bl.stats -and $bl.stats.wall_median -ne $null) {
                Write-Host ("  wall        : {0}" -f (Delta $wallStats.Median $bl.stats.wall_median "s"))
            }
            if ($rssStats.N -gt 0 -and $bl.stats.sampled_ws_median -ne $null -and $bl.interval_ms -eq $IntervalMs) {
                Write-Host ("  sampled WS  : {0}" -f (Delta $rssStats.Median $bl.stats.sampled_ws_median "MiB"))
            }
            if ($processPeakStats.N -gt 0 -and $bl.stats.process_peak_ws_median -ne $null) {
                Write-Host ("  OS peak WS  : {0}" -f (Delta $processPeakStats.Median $bl.stats.process_peak_ws_median "MiB"))
            }
            if ($bl.stats -and $bl.stats.gflops -and $gflops) {
                $d = ($gflops - $bl.stats.gflops) / $bl.stats.gflops * 100.0
                Write-Host ("  throughput  : {0:N1}%   ({1:N1} vs {2:N1} GFLOP/s)" -f $d, $gflops, $bl.stats.gflops)
            }
        } catch {
            Write-Host ("  baseline    : failed to parse {0}: {1}" -f $Baseline, $_) -ForegroundColor Yellow
        }
    }
}

Write-Host ""
if ($anyTimeout) { Write-Host "  WARNING: at least one run timed out" -ForegroundColor Red }
if ($anyNonZero) { Write-Host "  WARNING: at least one run exited non-zero" -ForegroundColor Red }
if ($anyTimeout -or $anyNonZero) { Write-Host '  V05 pending: legacy aggregates include failed runs; do not use this report for performance claims.' -ForegroundColor Yellow }
if ($ExpectedChecksum -and -not $allChecksumSame) {
    Write-Host "  WARNING: checksum mismatch across runs" -ForegroundColor Red
}
if ($ExpectedChecksum -and $allChecksumSame -and $checksums.Count -eq 1 -and $checksums[0] -ne $ExpectedChecksum) {
    Write-Host ("  WARNING: checksum {0} != expected {1}" -f $checksums[0], $ExpectedChecksum) -ForegroundColor Red
}

# ----------------------------------------------------------------------------
# CSV
# ----------------------------------------------------------------------------
if ($Csv) {
    $results |
        Select-Object * -ExcludeProperty Stdout, Stderr |
        Export-Csv -LiteralPath $Csv -NoTypeInformation -Encoding UTF8
    Write-Host ("  csv         : {0}" -f (Resolve-Path -LiteralPath $Csv).Path)
}

# ----------------------------------------------------------------------------
# JSON (stable schema for CI baselines)
# ----------------------------------------------------------------------------
if ($Json) {
    $jsonObj = [pscustomobject]@{
        schema_version = 2
        label         = $Label
        exe           = $exePath
        exe_bytes     = $exeItem.Length
        timestamp     = (Get-Date).ToString("o")
        runs          = $Runs
        discard       = $effectiveDiscard
        interval_ms   = $IntervalMs
        compile_sec   = $compileSec
        timing_sources = $timingSources
        measurement_scope = 'direct child; lifetime includes startup/shutdown; launch and observed time overlap lifetime, not additive; sampled WS is a lower bound; last complete vmem.dump counters retain runtime/reset scope'
        expected_ck   = $ExpectedChecksum
        config_line   = $configLine
        checksums     = $checksums
        environment   = $envInfo
        stats         = [pscustomobject]@{
            wall_mean     = $wallStats.Mean
            wall_min      = $wallStats.Min
            wall_max      = $wallStats.Max
            wall_median   = $wallStats.Median
            wall_mad      = $wallStats.Mad
            wall_stddev   = $wallStats.Std
            sampled_ws_mean = $rssStats.Mean
            sampled_ws_min = $rssStats.Min
            sampled_ws_max = $rssStats.Max
            sampled_ws_median = $rssStats.Median
            sampled_ws_mad = $rssStats.Mad
            sampled_ws_stddev = $rssStats.Std
            sampled_ws_count = $rssStats.N
            process_peak_ws_mean = $processPeakStats.Mean
            process_peak_ws_median = $processPeakStats.Median
            process_peak_ws_count = $processPeakStats.N
            cpu_mean      = $cpuStats.Mean
            cpu_median    = $cpuStats.Median
            cpu_count     = $cpuStats.N
            gflops        = $gflops
        }
        runs_detail   = $results
    }
    $jsonObj | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Json -Encoding UTF8
    Write-Host ("  json        : {0}" -f (Resolve-Path -LiteralPath $Json).Path)
}

if ($Csv -or $Json) { Write-Host "" }

# ----------------------------------------------------------------------------
# Exit code
# ----------------------------------------------------------------------------
$fail = $anyTimeout `
        -or $anyNonZero `
        -or -not $allChecksumSame `
        -or ($ExpectedChecksum -and $checksums.Count -eq 1 -and $checksums[0] -ne $ExpectedChecksum)
if ($fail) { exit 1 } else { exit 0 }
