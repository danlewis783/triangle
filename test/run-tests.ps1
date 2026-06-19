#!/usr/bin/env pwsh
# Build and run the Triangle test suites on Windows.
#
#   pwsh -File test\run-tests.ps1            # run unit tests + golden corpus
#   pwsh -File test\run-tests.ps1 -Update    # re-bless golden baseline files
#
# Exits non-zero if compilation, a unit test, or a golden comparison fails,
# so it is CI-friendly.
#
# Two layers of testing:
#   * Unity unit tests (test_triangle.c) - human-readable invariants.
#   * Golden corpus (golden_runner.c)    - byte-for-byte characterization of
#     the kept feature set; the tripwire for refactoring / code removal.

[CmdletBinding()]
param([switch]$Update)

Set-StrictMode -Version Latest

# Project root is this script's parent directory.
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Find clang: prefer PATH (set by the PowerShell profile), else the known dir.
$clangCmd = Get-Command clang -ErrorAction SilentlyContinue
$clang = if ($clangCmd) { $clangCmd.Source } else { 'C:\tools\llvm-mingw\bin\clang.exe' }
if (-not (Test-Path $clang)) {
    Write-Error "clang not found. Install LLVM-MinGW (see README-WINDOWS.md)."
}

$cflags  = @('-O2', '-DCPU86', '-DNO_TIMER', '-I.', '-Itest\unity')
$failed  = $false

function Invoke-Clang {
    param([string]$OutFile, [string[]]$Arguments)
    # Remove any stale binary first, so a failed compile cannot fall through to
    # running an old build and reporting a false pass.
    if (Test-Path $OutFile) { Remove-Item $OutFile -Force }
    & $clang @Arguments
    if (($LASTEXITCODE -ne 0) -or -not (Test-Path $OutFile)) {
        Write-Host "Compilation failed ($LASTEXITCODE)." -ForegroundColor Red
        exit 1
    }
}

# --- 1. Unity unit tests -------------------------------------------------- #

Write-Host '== Unit tests ==' -ForegroundColor Cyan
$unitExe = 'test\test_triangle.exe'
Invoke-Clang $unitExe (@('-o', $unitExe, 'triangle.c', 'test\unity\unity.c',
                'test\test_triangle.c', '-lm') + $cflags)
& ".\$unitExe"
if ($LASTEXITCODE -ne 0) { $failed = $true }

# --- 2. Golden corpus ----------------------------------------------------- #

Write-Host ''
Write-Host '== Golden corpus ==' -ForegroundColor Cyan
$goldenExe = 'test\golden_runner.exe'
Invoke-Clang $goldenExe (@('-o', $goldenExe, 'triangle.c', 'test\golden_runner.c', '-lm') + $cflags)

$goldenDir = 'test\golden'
$actualDir = 'test\.golden-actual'
if (Test-Path $actualDir) { Remove-Item $actualDir -Recurse -Force }
New-Item -ItemType Directory -Force $actualDir | Out-Null

& ".\$goldenExe" $actualDir | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Error "golden_runner failed ($LASTEXITCODE)." }

function Get-Normalized {
    param([string]$Path)
    (Get-Content -Raw -LiteralPath $Path) -replace "`r`n", "`n"
}

if ($Update) {
    New-Item -ItemType Directory -Force $goldenDir | Out-Null
    Copy-Item "$actualDir\*.txt" $goldenDir -Force
    $n = (Get-ChildItem "$goldenDir\*.txt").Count
    Write-Host "  Blessed $n golden file(s) in $goldenDir" -ForegroundColor Yellow
}
else {
    $goldenFiles = @(Get-ChildItem "$goldenDir\*.txt" -ErrorAction SilentlyContinue)
    if ($goldenFiles.Count -eq 0) {
        Write-Host '  No golden baseline found. Run with -Update to create it.' -ForegroundColor Yellow
        $failed = $true
    }
    foreach ($g in $goldenFiles) {
        $a = Join-Path $actualDir $g.Name
        if (-not (Test-Path $a)) {
            Write-Host "  MISSING  $($g.Name) (scenario not produced)" -ForegroundColor Red
            $failed = $true
            continue
        }
        if ((Get-Normalized $g.FullName) -eq (Get-Normalized $a)) {
            Write-Host "  PASS     $($g.Name)" -ForegroundColor Green
        }
        else {
            Write-Host "  FAIL     $($g.Name) (output changed vs baseline)" -ForegroundColor Red
            $diff = Compare-Object (Get-Content $g.FullName) (Get-Content $a) |
                    Select-Object -First 4
            $diff | ForEach-Object { Write-Host "             $($_.SideIndicator) $($_.InputObject)" }
            $failed = $true
        }
    }
    # Flag any new scenario that has no baseline yet.
    foreach ($a in @(Get-ChildItem "$actualDir\*.txt")) {
        if (-not (Test-Path (Join-Path $goldenDir $a.Name))) {
            Write-Host "  NEW      $($a.Name) (no baseline; run -Update to bless)" -ForegroundColor Yellow
            $failed = $true
        }
    }
}

Write-Host ''
if ($failed) { Write-Host 'FAILED' -ForegroundColor Red; exit 1 }
Write-Host 'OK' -ForegroundColor Green
exit 0
