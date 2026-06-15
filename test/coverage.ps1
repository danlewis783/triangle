#!/usr/bin/env pwsh
# Measure source-based coverage of triangle.c by the golden corpus.
#
#   pwsh -File test\coverage.ps1
#
# Instruments triangle.c, runs every golden scenario, and reports:
#   * file-level coverage of triangle.c, and
#   * the list of functions never executed by the corpus.
#
# Re-run this after removing code: a function that *becomes* uncovered, or a
# drop in line coverage on the kept path, means a cut went too far.
#
# NOTE: "never executed" is NOT the same as "safe to delete". Three kinds of
# uncovered code show up here: (1) unused features - removable; (2) error /
# safety handlers (internalerror, precisionerror, triexit, triunsuitable) -
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
    if (-not (Test-Path $t)) { Write-Error "Tool not found: $t (see README-WINDOWS.md)." }
}

$work = Join-Path $root 'test\.coverage'
New-Item -ItemType Directory -Force (Join-Path $work 'out') | Out-Null
$exe = Join-Path $work 'golden_cov.exe'
$raw = Join-Path $work 'golden.profraw'
$pd  = Join-Path $work 'golden.profdata'

Write-Host 'Instrumenting triangle.c (-O0, coverage)...' -ForegroundColor Cyan
& $clang -O0 -g -fprofile-instr-generate -fcoverage-mapping `
    -DTRILIBRARY -DCPU86 -DNO_TIMER -I. `
    -o $exe triangle.c test\golden_runner.c -lm
if ($LASTEXITCODE -ne 0) { Write-Error "Instrumented build failed ($LASTEXITCODE)." }

$env:LLVM_PROFILE_FILE = $raw
& $exe (Join-Path $work 'out') | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Error "Corpus run failed ($LASTEXITCODE)." }

& $profdata merge -sparse $raw -o $pd
if ($LASTEXITCODE -ne 0) { Write-Error "llvm-profdata merge failed ($LASTEXITCODE)." }

Write-Host ''
Write-Host '== File coverage of triangle.c ==' -ForegroundColor Cyan
& $cov report $exe -instr-profile=$pd triangle.c

Write-Host ''
Write-Host '== Functions never executed by the corpus ==' -ForegroundColor Cyan
$json = & $cov export $exe -instr-profile=$pd 2>$null | ConvertFrom-Json
$tri  = $json.data[0].functions | Where-Object { ($_.filenames -join ';') -like '*triangle.c*' }
$dead = $tri | Where-Object { $_.count -eq 0 } | ForEach-Object { $_.name } | Sort-Object
Write-Host ("{0} of {1} functions never executed:" -f $dead.Count, $tri.Count)
$dead | ForEach-Object { "  $_" }
