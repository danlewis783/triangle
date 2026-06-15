#!/usr/bin/env pwsh
# Build and run the Triangle Unity test suite on Windows.
#
#   pwsh -File test\run-tests.ps1
#
# Exits non-zero if compilation or any test fails, so it is CI-friendly.

$ErrorActionPreference = 'Stop'
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

$exe = 'test\test_triangle.exe'

Write-Host 'Compiling test suite...' -ForegroundColor Cyan
& $clang -O2 -DTRILIBRARY -DCPU86 -DNO_TIMER -I. -Itest\unity `
    -o $exe `
    triangle.c test\unity\unity.c test\test_triangle.c -lm
if ($LASTEXITCODE -ne 0) { Write-Error "Compilation failed ($LASTEXITCODE)." }

Write-Host 'Running tests...' -ForegroundColor Cyan
& ".\$exe"
exit $LASTEXITCODE
