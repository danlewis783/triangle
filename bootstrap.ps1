#requires -Version 7.0
param(
    [string]$BuildRoot = (Join-Path $PSScriptRoot '.build'),
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$b = [IO.Path]::GetFullPath($BuildRoot)
$manifest = Get-Content (Join-Path $PSScriptRoot 'native-dependencies.json') -Raw | ConvertFrom-Json
$archive = $manifest.archive
New-Item -ItemType Directory -Path "$b/downloads", "$b/tools" -Force | Out-Null
$file = Join-Path "$b/downloads" $archive.file
if (-not (Test-Path -LiteralPath $file)) {
    if ($Offline) { throw "Offline input missing: $file" }
    Write-Host "Downloading $($archive.file)"
    Invoke-WebRequest -Uri $archive.url -OutFile "$file.partial"
    if ((Get-FileHash -LiteralPath "$file.partial" -Algorithm SHA256).Hash -ne $archive.sha256) {
        throw "SHA-256 mismatch: $file.partial"
    }
    Move-Item -LiteralPath "$file.partial" -Destination $file
}
if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $archive.sha256) {
    throw "SHA-256 mismatch: $file"
}
$compiler = Join-Path "$b/tools/$($archive.directory)" 'bin/clang.exe'
$marker = "$file.extracted"
if (-not (Test-Path -LiteralPath $marker) -or
    (Get-Content -LiteralPath $marker -Raw).Trim() -ne $archive.sha256 -or
    -not (Test-Path -LiteralPath $compiler)) {
    Write-Host "Extracting $($archive.file)"
    & "$env:SystemRoot/System32/tar.exe" -xf $file -C "$b/tools"
    if ($LASTEXITCODE -ne 0) { throw 'Toolchain extraction failed' }
    Set-Content -LiteralPath $marker -Value $archive.sha256
}
$version = & $compiler --version
if ($LASTEXITCODE -ne 0 -or ($version -join "`n") -notmatch
    ("clang version " + [regex]::Escape($manifest.clang) + "\b")) {
    throw 'Unexpected Clang version'
}
Write-Host ($version -join "`n")
