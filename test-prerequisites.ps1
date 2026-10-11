$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'check-prerequisites.ps1')
$minimum = [version]'14.51.36231.0'
$cases = @(
    @{ Installed = $true; Version = 'v14.51.36231.0'; Expected = $true },
    @{ Installed = $true; Version = '14.52.1.0'; Expected = $true },
    @{ Installed = $true; Version = 'v14.50.99999.0'; Expected = $false },
    @{ Installed = $false; Version = 'v14.51.36231.0'; Expected = $false },
    @{ Installed = $true; Version = ''; Expected = $false },
    @{ Installed = $true; Version = 'invalid'; Expected = $false }
)
foreach ($case in $cases) {
    if ((Test-VcRuntimeVersion $case.Installed $case.Version $minimum) -ne $case.Expected) {
        throw "Prerequisite comparison failed: $($case.Version)"
    }
}
Write-Host '6/6 prerequisite version checks passed. No registry mutation or installation.'
