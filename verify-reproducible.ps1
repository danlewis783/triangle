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
# Unique directories preserve previous results and ensure no outputs are reused.
$verification = Join-Path $b ("repro-" + [guid]::NewGuid().ToString('N'))
$roots = @("$verification/first", "$verification/second path")
foreach ($root in $roots) {
    New-Item -ItemType Directory -Path "$root/downloads" -Force | Out-Null
    Copy-Item -LiteralPath "$b/downloads/$($manifest.archive.file)" -Destination "$root/downloads"
    & "$PSScriptRoot/build.ps1" -BuildRoot $root -Offline
}
foreach ($name in @('triangle.exe', 'triangle.o', 'tricall.exe', 'triangle.dll')) {
    $first = (Get-FileHash -LiteralPath "$($roots[0])/out/$name" -Algorithm SHA256).Hash
    $second = (Get-FileHash -LiteralPath "$($roots[1])/out/$name" -Algorithm SHA256).Hash
    if ($first -ne $second) { throw "Reproducibility mismatch: $name" }
    Write-Host "$name $first"
}
$out = "$($roots[0])/out"
Copy-Item -LiteralPath "$PSScriptRoot/A.poly" -Destination $out
Push-Location $out
try {
    & './triangle.exe' 'A.poly'
    if ($LASTEXITCODE -ne 0) { throw 'Triangle sample failed' }
    foreach ($name in @('A.1.node', 'A.1.ele', 'A.1.poly')) {
        if (-not (Test-Path -LiteralPath $name)) { throw "Missing sample output: $name" }
    }
    & './triangle.exe' '-pq30a5' 'A.poly'
    if ($LASTEXITCODE -ne 0) { throw 'Triangle quality mesh failed' }
    & './tricall.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Triangle library sample failed' }
    $objdump = "$($roots[0])/tools/$($manifest.archive.directory)/bin/llvm-objdump.exe"
    $headers = & $objdump -p 'triangle.dll'
    if ($LASTEXITCODE -ne 0) { throw 'DLL inspection failed' }
    foreach ($symbol in @('triangulate', 'trifree')) {
        if (($headers -join "`n") -notmatch ("(?m)\s" + $symbol + "\s*$")) {
            throw "Missing DLL export: $symbol"
        }
    }
} finally { Pop-Location }
Write-Host "PASS: four identical artifacts, sample programs, and DLL exports. Results: $verification"
