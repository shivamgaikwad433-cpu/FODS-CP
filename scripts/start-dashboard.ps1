$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$serverPath = Join-Path $projectRoot 'server.exe'
$compilerPath = 'C:\msys64\ucrt64\bin\g++.exe'
$serverOut = Join-Path $env:TEMP 'network-failure-analyzer-server.out.log'
$serverErr = Join-Path $env:TEMP 'network-failure-analyzer-server.err.log'
$dashboardUrl = 'http://localhost:8080/'
$apiUrl = 'http://127.0.0.1:8080/api/locations'

# Reuse this app if it is already listening, but do not disturb another service on port 8080.
$listener = Get-NetTCPConnection -LocalPort 8080 -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
if ($listener) {
    try {
        $locations = Invoke-RestMethod -Uri $apiUrl -TimeoutSec 3
        if (-not ($locations.id -contains 'school')) { throw 'The server on port 8080 is not the Network Failure Analyzer.' }
        Start-Process $dashboardUrl
        Write-Host "Dashboard is already running: $dashboardUrl"
        exit 0
    } catch {
        throw "Port 8080 is already occupied (PID $($listener.OwningProcess)), but the dashboard API did not respond. Close that process or use another port."
    }
}

if (-not (Test-Path -LiteralPath $compilerPath)) {
    throw "MinGW-w64 compiler not found at $compilerPath. Install it or update compilerPath in this script."
}

Write-Host 'Building the C++ REST server...'
Push-Location $projectRoot
try {
    & $compilerPath -std=c++11 -pthread -Ibackend -Icpp backend/server.cpp -o server.exe -lws2_32
    if ($LASTEXITCODE -ne 0) { throw "Server build failed with exit code $LASTEXITCODE." }
} finally {
    Pop-Location
}

# httplib's Windows executable needs the MSYS2 UCRT runtime DLLs at launch time.
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
$process = Start-Process -FilePath $serverPath -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $serverOut -RedirectStandardError $serverErr

$ready = $false
for ($attempt = 0; $attempt -lt 15; $attempt++) {
    Start-Sleep -Seconds 1
    if ($process.HasExited) {
        $details = if (Test-Path -LiteralPath $serverErr) { (Get-Content -LiteralPath $serverErr -Tail 20) -join "`n" } else { 'No server error log was produced.' }
        throw "The REST server exited during startup. $details"
    }
    try {
        $locations = Invoke-RestMethod -Uri $apiUrl -TimeoutSec 2
        if ($locations.id -contains 'school') { $ready = $true; break }
    } catch { }
}

if (-not $ready) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $details = if (Test-Path -LiteralPath $serverErr) { (Get-Content -LiteralPath $serverErr -Tail 20) -join "`n" } else { 'The API did not become ready within 15 seconds.' }
    throw "The REST server did not become ready. $details"
}

Start-Process $dashboardUrl
Write-Host "Dashboard is ready: $dashboardUrl"
Write-Host "REST server PID: $($process.Id)"
