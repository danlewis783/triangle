#!/usr/bin/env pwsh
# Measure source-based coverage of triangle.c by the full test suite.
#
#   pwsh -File test\coverage.ps1
#
# Instruments triangle.c, runs the Unity and golden-corpus tests, and reports:
#   * file-level coverage of triangle.c, and
#   * the list of functions never executed by either test layer.
#
# Re-run this after removing code: a function that *becomes* uncovered, or a
# drop in line coverage on the kept path, means a cut went too far.
#
# NOTE: "never executed" is NOT the same as "safe to delete". Three kinds of
# uncovered code show up here: (1) unused features - removable; (2) error /
# safety handlers (internalerror, precisionerror, triexit) -
# keep; (3) rare branches of kept subsystems (e.g. subsegdealloc inside the
# hole-carving plague) - keep. Judgement, not the raw list, drives removal.

Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$bin = 'C:\tools\llvm-mingw\bin'
$clang    = (Get-Command clang    -ErrorAction SilentlyContinue)?.Source ?? "$bin\clang.exe"
$profdata = (Get-Command llvm-profdata -ErrorAction SilentlyContinue)?.Source ?? "$bin\llvm-profdata.exe"
$cov      = (Get-Command llvm-cov  -ErrorAction SilentlyContinue)?.Source ?? "$bin\llvm-cov.exe"
foreach ($t in $clang, $profdata, $cov) {
    if (-not (Test-Path $t)) {
        Write-Error "Tool not found: $t (see README-WINDOWS.md)."
        exit 1
    }
}

$work = Join-Path $root 'test\.coverage'
New-Item -ItemType Directory -Force (Join-Path $work 'out') | Out-Null
$goldenExe = Join-Path $work 'golden_cov.exe'
$unitExe   = Join-Path $work 'unit_cov.exe'
$goldenRaw = Join-Path $work 'golden.profraw'
$unitRaw   = Join-Path $work 'unit.profraw'
$pd        = Join-Path $work 'combined.profdata'

Write-Host 'Instrumenting triangle.c (-O0, coverage)...' -ForegroundColor Cyan
& $clang -O0 -g -fprofile-instr-generate -fcoverage-mapping `
    -DTRILIBRARY -DCPU86 -DNO_TIMER -I. `
    -o $goldenExe triangle.c test\golden_runner.c -lm
if ($LASTEXITCODE -ne 0) {
    Write-Error "Instrumented golden build failed ($LASTEXITCODE)."
    exit 1
}

& $clang -O0 -g -fprofile-instr-generate -fcoverage-mapping `
    -DTRILIBRARY -DCPU86 -DNO_TIMER -I. -Itest\unity `
    -o $unitExe triangle.c test\unity\unity.c test\test_triangle.c -lm
if ($LASTEXITCODE -ne 0) {
    Write-Error "Instrumented Unity build failed ($LASTEXITCODE)."
    exit 1
}

$env:LLVM_PROFILE_FILE = $goldenRaw
& $goldenExe (Join-Path $work 'out') | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "Corpus run failed ($LASTEXITCODE)."
    exit 1
}

$env:LLVM_PROFILE_FILE = $unitRaw
& $unitExe | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "Unity run failed ($LASTEXITCODE)."
    exit 1
}

& $profdata merge -sparse $goldenRaw $unitRaw -o $pd
if ($LASTEXITCODE -ne 0) {
    Write-Error "llvm-profdata merge failed ($LASTEXITCODE)."
    exit 1
}

Write-Host ''
Write-Host '== File coverage of triangle.c ==' -ForegroundColor Cyan
& $cov report $goldenExe -object $unitExe "-instr-profile=$pd" triangle.c
if ($LASTEXITCODE -ne 0) {
    Write-Error "llvm-cov report failed ($LASTEXITCODE)."
    exit 1
}

Write-Host ''
Write-Host '== Functions never executed by either test layer ==' -ForegroundColor Cyan
$export = & $cov export $goldenExe -object $unitExe "-instr-profile=$pd" 2>&1 |
    Out-String
if ($LASTEXITCODE -ne 0) {
    Write-Error "llvm-cov export failed ($LASTEXITCODE)."
    exit 1
}
$json = $export | ConvertFrom-Json
$tri  = $json.data[0].functions |
    Where-Object { $_.filenames[0] -match '[/\\]triangle\.c$' }
$functions = @($tri | Group-Object name | ForEach-Object {
    [pscustomobject]@{
        Name = $_.Name
        Count = ($_.Group | Measure-Object count -Sum).Sum
    }
})
$dead = @($functions | Where-Object { $_.Count -eq 0 } |
    ForEach-Object { $_.Name } | Sort-Object)
$triangleFile = $json.data[0].files |
    Where-Object { $_.filename -match '[/\\]triangle\.c$' } |
    Select-Object -First 1
$totalFunctions = $triangleFile.summary.functions.count
Write-Host ("{0} of {1} functions never executed:" -f $dead.Count, $totalFunctions)
$dead | ForEach-Object { "  $_" }
