$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$compilerPath = 'C:\msys64\ucrt64\bin\g++.exe'
$startScript = Join-Path $PSScriptRoot 'start-dashboard.ps1'

# Ensure the shared REST server is running before starting the interactive client.
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $startScript
if ($LASTEXITCODE -ne 0) { throw 'Could not start the dashboard server.' }

if (-not (Test-Path -LiteralPath $compilerPath)) {
    throw "MinGW-w64 compiler not found at $compilerPath."
}
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"

Push-Location $projectRoot
try {
    & $compilerPath -std=c++11 -pthread -Ibackend main.cpp -o analyzer.exe -lws2_32
    if ($LASTEXITCODE -ne 0) { throw "Terminal client build failed with exit code $LASTEXITCODE." }
    Write-Host "Starting the terminal client. Dashboard updates use the same $($env:COMPUTERNAME) network data."
    & (Join-Path $projectRoot 'analyzer.exe')
} finally {
    Pop-Location
}
