#requires -Version 7.0
param(
    [string]$BuildRoot = (Join-Path $PSScriptRoot '.build'),
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Discovery only: no downloads or machine-wide environment changes.
$manifest = Get-Content (Join-Path $PSScriptRoot 'native-dependencies.json') -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 2 -or $manifest.target -ne 'x64') {
    throw 'Unsupported toolchain manifest (expected schema 2, x64).'
}
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Install Visual Studio or Build Tools with Desktop development with C++.'
}
$installations = & $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio discovery failed.' }
$vc = $null
foreach ($installation in $installations) {
    $candidate = Join-Path $installation "VC/Tools/MSVC/$($manifest.msvc)"
    if (Test-Path -LiteralPath "$candidate/bin/Hostx64/x64/cl.exe") {
        $vc = $candidate
        break
    }
}
if (-not $vc) { throw "Install MSVC $($manifest.msvc) using Visual Studio Installer, or explicitly update the manifest pin." }
$sdkRoot = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10
$bin = Join-Path $vc 'bin/Hostx64/x64'
$includes = @("$vc/include", "$sdkRoot/Include/$($manifest.windowsSdk)/ucrt",
    "$sdkRoot/Include/$($manifest.windowsSdk)/shared", "$sdkRoot/Include/$($manifest.windowsSdk)/um")
$libs = @("$vc/lib/x64", "$sdkRoot/Lib/$($manifest.windowsSdk)/ucrt/x64",
    "$sdkRoot/Lib/$($manifest.windowsSdk)/um/x64")
foreach ($path in ($includes + $libs + @("$bin/cl.exe", "$bin/link.exe", "$bin/lib.exe", "$bin/nmake.exe", "$bin/dumpbin.exe"))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required MSVC/Windows SDK input missing: $path" }
}
Write-Host "MSVC $($manifest.msvc), Windows SDK $($manifest.windowsSdk), Hostx64/x64"
[pscustomobject]@{
    Bin = $bin
    Include = $includes -join ';'
    Lib = $libs -join ';'
}
