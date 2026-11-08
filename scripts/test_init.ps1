# test_init.ps1 - behavioural checks for `vynec init` (Milestone 1).
#
# Run from the repository root:
#   pwsh -File scripts/test_init.ps1
#
# The script never touches the repository working tree: every project is
# generated under the system temp directory and removed at the end.

$ErrorActionPreference = "Continue"

$repo = Split-Path -Parent $PSScriptRoot
$vynec = Join-Path $repo "vynec.exe"
if (-not (Test-Path -LiteralPath $vynec)) {
    $vynec = Join-Path $repo "vynec"
}
if (-not (Test-Path -LiteralPath $vynec)) {
    Write-Error "vynec not found under $repo"
}

$base = Join-Path ([System.IO.Path]::GetTempPath()) ("vyne_init_test_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $base | Out-Null

$failures = 0
function Check([string]$name, [bool]$condition) {
    if ($condition) {
        Write-Host "PASS $name"
    } else {
        Write-Host "FAIL $name"
        $script:failures++
    }
}

function Run-Init([string[]]$arguments, [string]$workdir = $null) {
    $previous = Get-Location
    try {
        if ($workdir) { Set-Location -LiteralPath $workdir }
        & $vynec @arguments 2>$null | Out-Null
        return $LASTEXITCODE
    } finally {
        Set-Location $previous
    }
}

try {
    # --- template engine unit checks ---------------------------------
    Check "self-test" ((Run-Init @("init", "--self-test")) -eq 0)

    # --- blank template ----------------------------------------------
    $blank = Join-Path $base "blank_demo"
    New-Item -ItemType Directory -Path $blank | Out-Null
    $rc = Run-Init @("init", "--name", "blank_demo", "--kind", "blank", "--no-git", $blank)
    Check "blank-exit" ($rc -eq 0)
    Check "blank-project-toml" (Test-Path (Join-Path $blank "project.toml"))
    Check "blank-main" (Test-Path (Join-Path $blank "src/main.vy"))
    Check "blank-readme" (Test-Path (Join-Path $blank "README.md"))
    Check "blank-gitignore" (Test-Path (Join-Path $blank ".gitignore"))
    Check "blank-license" (Test-Path (Join-Path $blank "LICENSE"))

    $toml = Get-Content -Raw (Join-Path $blank "project.toml")
    Check "toml-name" ($toml -match 'name = "blank_demo"')
    Check "toml-vyne" ($toml -match 'vyne = "0\.4\.0"')
    Check "toml-target" ($toml -match '\[targets\.desktop\]')

    $main = Get-Content -Raw (Join-Path $blank "src/main.vy")
    Check "main-substitution" ($main -match 'Hello from blank_demo!')
    Check "main-bounded" ($main -match '@bounded<1024>')
    Check "main-deterministic" ($main -match '@deterministic')

    # --- generated program compiles and runs -------------------------
    Push-Location -LiteralPath $blank
    try {
        & $vynec --compile "src/main.vy" 2>$null | Out-Null
        Check "compile-exit" ($LASTEXITCODE -eq 0)

        if (Test-Path -LiteralPath "src/main.exe") {
            $output = & ".\src\main.exe" 2>&1 | Out-String
            Check "run-hello" ($output -match "Hello from blank_demo!")
            Check "run-bounded" ($output -match "@bounded region completed")
            Check "run-deterministic" ($output -match "@deterministic region has no RNG")
        } else {
            Check "compile-exe" $false
        }
    } finally {
        Pop-Location
    }

    # --- overwrite refusal -------------------------------------------
    $overwrite = Run-Init @("init", "--name", "blank_demo", "--kind", "blank", "--no-git", $blank)
    Check "overwrite-exit" ($overwrite -eq 3)

    # --- MIT and proprietary licenses --------------------------------
    $mit = Join-Path $base "mit_demo"
    New-Item -ItemType Directory -Path $mit | Out-Null
    Check "mit-exit" ((Run-Init @("init", "--name", "mit_demo", "--kind", "blank", "--license", "MIT", "--no-git", $mit)) -eq 0)
    Check "mit-license" ((Get-Content (Join-Path $mit "LICENSE") -TotalCount 1) -eq "MIT License")

    $prop = Join-Path $base "prop_demo"
    New-Item -ItemType Directory -Path $prop | Out-Null
    Check "proprietary-exit" ((Run-Init @("init", "--name", "prop_demo", "--kind", "blank", "--license", "proprietary", "--no-git", $prop)) -eq 0)
    Check "proprietary-no-license" (-not (Test-Path (Join-Path $prop "LICENSE")))
    Check "proprietary-header" (Test-Path (Join-Path $prop "LICENSE_HEADER.txt"))

    # --- CUDA + dependency sections ----------------------------------
    $cuda = Join-Path $base "cuda_demo"
    New-Item -ItemType Directory -Path $cuda | Out-Null
    Check "cuda-exit" ((Run-Init @("init", "--name", "cuda_demo", "--kind", "blank", "--cuda", "yes", "--libraries", "vlin,vml", "--no-git", $cuda)) -eq 0)
    $cudaToml = Get-Content -Raw (Join-Path $cuda "project.toml")
    Check "cuda-ggml" ($cudaToml -match 'ggml = "0\.9\.4"')
    Check "cuda-section" ($cudaToml -match '\[device\.cuda\]')
    Check "cuda-sm" ($cudaToml -match 'sm_version = "86"')
    Check "cuda-vlin" ($cudaToml -match 'vlin = "0\.4"')
    Check "cuda-vml" ($cudaToml -match 'vml = "0\.4"')

    # --- target-driven device sections -------------------------------
    $jetson = Join-Path $base "jetson_demo"
    New-Item -ItemType Directory -Path $jetson | Out-Null
    Check "jetson-exit" ((Run-Init @("init", "--name", "jetson_demo", "--kind", "blank", "--targets", "desktop,jetson", "--no-git", $jetson)) -eq 0)
    $jetsonToml = Get-Content -Raw (Join-Path $jetson "project.toml")
    Check "jetson-cuda-section" ($jetsonToml -match '\[device\.cuda\]')
    Check "jetson-sm" ($jetsonToml -match 'sm_version = "86"')

    $coral = Join-Path $base "coral_demo"
    New-Item -ItemType Directory -Path $coral | Out-Null
    Check "coral-exit" ((Run-Init @("init", "--name", "coral_demo", "--kind", "blank", "--targets", "desktop,coral", "--no-git", $coral)) -eq 0)
    $coralToml = Get-Content -Raw (Join-Path $coral "project.toml")
    Check "coral-ggml" ($coralToml -match 'ggml = "0\.9\.4"')
    Check "coral-quantize" ($coralToml -match 'quantize = "int8"')
    Check "coral-no-cuda-section" (-not ($coralToml -match '\[device\.cuda\]'))

    # --- non-interactive and unknown kind ----------------------------
    $nope = Join-Path $base "no_name"
    New-Item -ItemType Directory -Path $nope | Out-Null
    Check "noninteractive-exit" ((Run-Init @("init", "--kind", "blank", "--no-git", $nope)) -eq 2)

    $badKind = Join-Path $base "bad_kind"
    New-Item -ItemType Directory -Path $badKind | Out-Null
    Check "unknown-kind-exit" ((Run-Init @("init", "--name", "bad_kind", "--kind", "ml", "--no-git", $badKind)) -eq 4)

    # --- git integration (uses local temp repo only) -----------------
    $gitDemo = Join-Path $base "git_demo"
    New-Item -ItemType Directory -Path $gitDemo | Out-Null
    $env:GIT_AUTHOR_NAME = "Vyne Test"
    $env:GIT_AUTHOR_EMAIL = "vyne-test@example.com"
    $env:GIT_COMMITTER_NAME = "Vyne Test"
    $env:GIT_COMMITTER_EMAIL = "vyne-test@example.com"
    Check "git-init-exit" ((Run-Init @("init", "--name", "git_demo", "--kind", "blank", $gitDemo)) -eq 0)
    $log = (& git -C $gitDemo log --oneline -1 2>&1 | Out-String)
    Check "git-commit" ($log -match "Initial commit from vynec init")
} finally {
    Remove-Item -Recurse -Force -LiteralPath $base -ErrorAction SilentlyContinue
}

if ($failures -gt 0) {
    Write-Host "vynec init: $failures check(s) failed"
    exit 1
}
Write-Host "vynec init: all checks passed"
exit 0
