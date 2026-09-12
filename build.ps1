#requires -Version 7.0
param(
    [string]$BuildRoot = (Join-Path $PSScriptRoot '.build'),
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$b = [IO.Path]::GetFullPath($BuildRoot)
& "$PSScriptRoot/bootstrap.ps1" -BuildRoot $b -Offline:$Offline
$manifest = Get-Content "$PSScriptRoot/native-dependencies.json" -Raw | ConvertFrom-Json
$toolBin = Join-Path "$b/tools/$($manifest.archive.directory)" 'bin'
$out = Join-Path $b 'out'
New-Item -ItemType Directory -Path $out -Force | Out-Null

# Compile stable relative source names from a controlled directory.
foreach ($name in @('triangle.c', 'triangle.h', 'tricall.c')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $out -Force
}
$environmentNames = @('PATH', 'SOURCE_DATE_EPOCH', 'CPATH', 'C_INCLUDE_PATH',
    'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'COMPILER_PATH', 'CCC_OVERRIDE_OPTIONS',
    'CLANG_CONFIG_FILE_SYSTEM_DIR', 'CLANG_CONFIG_FILE_USER_DIR')
$saved = @{}
foreach ($name in $environmentNames) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    $env:PATH = "$toolBin;$env:SystemRoot/System32"
    $env:SOURCE_DATE_EPOCH = [string]$manifest.sourceDateEpoch
    $common = @('--no-default-config', "--target=$($manifest.target)", '-O2',
        '-DCPU86', '-DNO_TIMER', '-ffile-prefix-map=.=/triangle',
        "-ffile-prefix-map=$($out.Replace('\','/'))=/triangle")
    $link = @('-fuse-ld=lld', '-rtlib=compiler-rt', '-unwindlib=libunwind', '-Wl,--no-insert-timestamp')
    function Invoke-Clang([string[]]$CompilerArguments) {
        & "$toolBin/clang.exe" @common @CompilerArguments
        if ($LASTEXITCODE -ne 0) { throw 'Clang failed' }
    }
    Push-Location $out
    try {
        Invoke-Clang ($link + @('-o', 'triangle.exe', 'triangle.c', '-lm'))
        Invoke-Clang @('-DTRILIBRARY', '-c', '-o', 'triangle.o', 'triangle.c')
        Invoke-Clang ($link + @('-o', 'tricall.exe', 'tricall.c', 'triangle.o', '-lm'))
        Invoke-Clang ($link + @('-DTRILIBRARY', '-shared', '-Wl,--export-all-symbols',
            '-o', 'triangle.dll', 'triangle.c'))
        $hashes = foreach ($name in @('triangle.exe', 'triangle.o', 'tricall.exe', 'triangle.dll')) {
            "{0}  {1}" -f (Get-FileHash -LiteralPath $name -Algorithm SHA256).Hash.ToLowerInvariant(), $name
        }
        $hashes | Set-Content -LiteralPath 'SHA256SUMS'
    } finally { Pop-Location }
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}
Write-Host "Built Triangle in $out"
