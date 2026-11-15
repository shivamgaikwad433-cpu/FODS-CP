param(
    [string]$BaseUrl = 'http://localhost:8080'
)

$ErrorActionPreference = 'Stop'
$BaseUrl = $BaseUrl.TrimEnd('/')
$locationId = 'api-check-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
$locationCreated = $false

function Invoke-Api {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string]$Method = 'GET',
        $Body = $null
    )

    $request = @{
        Uri = "$BaseUrl$Path"
        Method = $Method
        ErrorAction = 'Stop'
    }
    if ($null -ne $Body) {
        $request.ContentType = 'application/json'
        $request.Body = $Body | ConvertTo-Json -Depth 10
    }
    Invoke-RestMethod @request
}

function Assert-Equal {
    param($Actual, $Expected, [string]$Message)
    if ($Actual -ne $Expected) {
        throw "$Message (expected '$Expected', got '$Actual')"
    }
    Write-Host "PASS $Message"
}

try {
    $locations = Invoke-Api -Path '/api/locations'
    if (-not ($locations.id -contains 'school') -or -not ($locations.id -contains 'college')) {
        throw 'The seeded school and college locations were not returned.'
    }
    Write-Host 'PASS seeded locations are available'

    $schoolBefore = Invoke-Api -Path '/api/network?locationId=school'
    $schoolDeviceCount = $schoolBefore.devices.Count
    Invoke-Api -Path '/api/locations' -Method POST -Body @{ id = $locationId; name = 'API Verification' } | Out-Null
    $locationCreated = $true
    Write-Host 'PASS created an isolated test location'

    foreach ($device in @(
        @{ id = 'router-a'; name = 'Router A'; type = 'router' },
        @{ id = 'switch-a'; name = 'Switch A'; type = 'switch' },
        @{ id = 'computer-a'; name = 'Computer A'; type = 'computer' }
    )) {
        Invoke-Api -Path "/api/devices?locationId=$locationId" -Method POST -Body $device | Out-Null
    }
    Invoke-Api -Path "/api/connections?locationId=$locationId" -Method POST -Body @{ id = 'link-a'; source = 'router-a'; target = 'switch-a'; latency = 2 } | Out-Null
    Invoke-Api -Path "/api/connections?locationId=$locationId" -Method POST -Body @{ id = 'link-b'; source = 'switch-a'; target = 'computer-a'; latency = 1 } | Out-Null

    $path = Invoke-Api -Path "/api/path?locationId=$locationId&source=router-a&destination=computer-a"
    Assert-Equal $path.latency 3 'Dijkstra returns the expected path cost'
    Assert-Equal $path.hops 2 'Dijkstra returns the expected hop count'
    $routes = Invoke-Api -Path "/api/alternative-paths?locationId=$locationId&source=router-a&destination=computer-a"
    Assert-Equal $routes.Count 1 'alternative route search finds the one available route'

    $critical = Invoke-Api -Path "/api/critical-devices?locationId=$locationId"
    if (-not ($critical.devices -contains 'switch-a')) { throw 'Tarjan analysis did not find switch-a as an articulation device.' }
    Write-Host 'PASS Tarjan analysis identifies the articulation device'

    Invoke-Api -Path "/api/simulate-failure?locationId=$locationId" -Method POST -Body @{ id = 'switch-a'; type = 'device' } | Out-Null
    $stats = Invoke-Api -Path "/api/statistics?locationId=$locationId"
    Assert-Equal $stats.failedDevices 1 'device failure updates the selected location statistics'
    Assert-Equal $stats.components 2 'device failure splits this topology into two components'
    $schoolAfter = Invoke-Api -Path '/api/network?locationId=school'
    Assert-Equal $schoolAfter.devices.Count $schoolDeviceCount 'failure in the test location leaves school device count unchanged'
    if (($schoolAfter.devices | Where-Object { $_.status -eq 'failed' }).Count -ne ($schoolBefore.devices | Where-Object { $_.status -eq 'failed' }).Count) {
        throw 'Failure in the test location changed school device state.'
    }
    Write-Host 'PASS failure state is isolated between locations'

    Invoke-Api -Path "/api/restore?locationId=$locationId" -Method POST -Body @{ id = 'switch-a'; type = 'device' } | Out-Null
    $restored = Invoke-Api -Path "/api/statistics?locationId=$locationId"
    Assert-Equal $restored.failedDevices 0 'restoration clears the failure'
    Assert-Equal $restored.components 1 'restoration reconnects the test topology'

    $savedPath = Join-Path (Get-Location) "data\locations\$locationId\network.json"
    if (-not (Test-Path -LiteralPath $savedPath)) { throw 'Location network JSON was not persisted.' }
    Write-Host 'PASS location state was persisted to JSON'
    Write-Host 'API smoke checks passed.'
}
finally {
    if ($locationCreated) {
        try {
            Invoke-Api -Path "/api/locations/$locationId" -Method DELETE | Out-Null
            $testDirectory = Join-Path (Get-Location) "data\locations\$locationId"
            if (Test-Path -LiteralPath $testDirectory) { Remove-Item -LiteralPath $testDirectory -Recurse -Force }
        } catch { Write-Warning "Could not remove test location $locationId : $_" }
    }
}
