#requires -Version 7.0
param(
    [string]$BuildRoot = (Join-Path $PSScriptRoot '.build'),
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$b = [IO.Path]::GetFullPath($BuildRoot)
$toolchain = & "$PSScriptRoot/bootstrap.ps1" -BuildRoot $b -Offline:$Offline
$manifest = Get-Content "$PSScriptRoot/native-dependencies.json" -Raw | ConvertFrom-Json
$out = Join-Path $b 'out'
New-Item -ItemType Directory -Path $out -Force | Out-Null
foreach ($name in @('triangle.c', 'triangle.h', 'tricall.c', 'triangle.def', 'makefile')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $out -Force
}
New-Item -ItemType Directory -Path "$out/test" -Force | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot/test/msvc-dll.c" -Destination "$out/test" -Force
$environmentNames = @('PATH', 'INCLUDE', 'LIB', 'LIBPATH', 'CL', '_CL_', 'LINK', '_LINK_', 'MAKEFLAGS')
$saved = @{}
foreach ($name in $environmentNames) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    $env:PATH = "$($toolchain.Bin);$env:SystemRoot/System32"
    $env:INCLUDE = $toolchain.Include
    $env:LIB = $toolchain.Lib
    Push-Location $out
    try {
        & "$($toolchain.Bin)/nmake.exe" /nologo /A /f makefile all
        if ($LASTEXITCODE -ne 0) { throw 'MSVC build failed.' }
        $hashes = foreach ($name in $manifest.artifacts) {
            "{0}  {1}" -f (Get-FileHash -LiteralPath $name -Algorithm SHA256).Hash.ToLowerInvariant(), $name
        }
        $hashes | Set-Content -LiteralPath 'SHA256SUMS'
        Copy-Item -LiteralPath "$PSScriptRoot/native-dependencies.json" -Destination 'native-dependencies.json' -Force
    } finally { Pop-Location }
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}
Write-Host "Built Triangle in $out"
